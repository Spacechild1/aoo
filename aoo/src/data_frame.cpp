#include "data_frame.hpp"

#include "common/bit_utils.hpp"

namespace aoo {

//-------------------------- data_frame_allocator --------------------------------//

// TODO: should we try to split larger blocks before allocating a new one?
data_frame* data_frame_allocator::allocate(int32_t size) {
    auto index = size_to_bin(size);
    // load head
    auto frame = bins_[index].load(std::memory_order_relaxed);
    do {
        if (frame == nullptr) {
            // allocate new frame
            auto alloc_size = bin_to_alloc_size(index);
            assert((alloc_size - sizeof(data_frame_header)) >= size);
            frame = (data_frame_header*)aoo::allocate(alloc_size);
            frame->next = nullptr;
            frame->bin_index = index;
            frame->frame_index = 0;
#if DATA_FRAME_LEAK_DETECTION
            auto num_bytes = num_alloc_bytes_.fetch_add(alloc_size, std::memory_order_relaxed) + alloc_size;
            auto num_frames = num_alloc_frames_.fetch_add(1, std::memory_order_relaxed) + 1;
#if DEBUG_DATA_FRAME_ALLOCATOR
            LOG_DEBUG("data_frame_allocator: allocate " << alloc_size << " bytes (total bytes: "
                      << num_bytes << ", total frames: " << num_frames << ")");
#endif
#endif
            break;
        }
        // try to reuse existing frame.
        // NB: compare_exchange() will update frame on failure!
    } while (!bins_[index].compare_exchange_weak(frame, frame->next,
                                                 std::memory_order_acq_rel,
                                                 std::memory_order_relaxed));
    frame->size = size;
#if DEBUG_DATA_FRAME_ALLOCATOR
    auto num_frames = num_frames_.fetch_add(1, std::memory_order_acquire) + 1;
    LOG_DEBUG("data_frame_allocator: allocate frame (size: "
              << size << " bytes, total frames: " << num_frames << ")");
#endif
    return (data_frame*)frame;
}

void data_frame_allocator::deallocate(data_frame *frame) {
    if (!frame) {
        return;
    }
    assert(frame->header.bin_index >= 0 && frame->header.frame_index >= 0);
#if DEBUG_DATA_FRAME_ALLOCATOR
    auto num_frames = num_frames_.fetch_sub(1, std::memory_order_release) - 1;
    LOG_DEBUG("data_frame_allocator: deallocate frame (size: "
              << frame->header.size << " bytes, remaining frames: " << num_frames << ")");
    assert(num_frames >= 0);
#endif
#if 1
    // catch double free
    assert(frame->header.size != (int32_t)0xdeadbeef);
    frame->header.size = (int32_t)0xdeadbeef;
#endif
    // add to free list
    auto index = frame->header.bin_index;
    frame->header.next = bins_[index].load(std::memory_order_relaxed);
    // check if the head has changed and update it atomically.
    // (if the CAS fails, 'next' is updated to the current head)
    while (!bins_[index].compare_exchange_weak(frame->header.next, &frame->header,
                                               std::memory_order_acq_rel,
                                               std::memory_order_relaxed)) ;
}

void data_frame_allocator::release_memory() {
#if DATA_FRAME_LEAK_DETECTION
    LOG_DEBUG("data_frame_allocator: release memory (" << num_alloc_bytes_.load()
              << " bytes, " << num_alloc_frames_.load() << " frames)");
#endif
    for (auto& b : bins_) {
        auto ptr = b.exchange(nullptr, std::memory_order_relaxed);
        while (ptr) {
            assert(ptr->bin_index >= 0 && ptr->frame_index >= 0);
            auto next = ptr->next;
            auto alloc_size = bin_to_alloc_size(ptr->bin_index);
#if 0
            LOG_DEBUG("data_frame: bin index = " << ptr->bin_index << ", frame index = "
                      << ptr->frame_index << ", alloc size = " << alloc_size);
#endif
            aoo::deallocate(ptr, alloc_size);
#if DATA_FRAME_LEAK_DETECTION
            num_alloc_bytes_.fetch_sub(alloc_size, std::memory_order_relaxed);
            num_alloc_frames_.fetch_sub(1, std::memory_order_relaxed);
#endif
            ptr = next;
        }
    }
#if DATA_FRAME_LEAK_DETECTION
    auto num_alloc_bytes = num_alloc_bytes_.exchange(0, std::memory_order_relaxed);
    auto num_alloc_frames = num_alloc_frames_.exchange(0, std::memory_order_relaxed);
    if (num_alloc_frames != 0) {
        LOG_ERROR("data_frame_allocator: leaked " << num_alloc_frames << " frames");
    }
    if (num_alloc_bytes != 0) {
        LOG_ERROR("data_frame_allocator: leaked " << num_alloc_bytes << " bytes");
    }
    assert(num_alloc_frames == 0 && num_alloc_bytes == 0); // trigger assertion
#endif
}

size_t data_frame_allocator::size_to_bin(size_t size) {
    assert(size > 0);
    // NB: it shouldn't be possible to receive a data frame that is larger
    // than AOO_MAX_BLOCK_SIZE in the first place!
    if (size > max_bin_size) {
        throw std::runtime_error("data frame allocation request too large");
    }
    // round up to next power of 2
    auto next_ilog2 = 32 - clz((uint32_t)size - 1);
    auto index = (next_ilog2 > min_bin_ilog2_size) ? next_ilog2 - min_bin_ilog2_size : 0;
    assert(index < bin_count);
    return index;
}

size_t data_frame_allocator::bin_to_alloc_size(size_t index) {
    auto size = (size_t)1 << (index + min_bin_ilog2_size);
    assert(size <= max_bin_size);
    return size + sizeof(data_frame_header);
}

//-------------------------- data_frame_storage --------------------------------//

void data_frame_storage::init(size_t size) {
    assert(size_ == 0);
    if (size > small_frame_limit) {
        data_.l.head = nullptr;
        data_.l.tail = nullptr;
        data_.l.bitset.reset();
    } else {
        std::fill(data_.vec.begin(), data_.vec.end(), nullptr);
    }
    size_ = size;
}

void data_frame_storage::clear(data_frame_allocator& alloc) {
    // LOG_DEBUG("data_frame_storage: return " << size_ << " frames");
    if (size_ > small_frame_limit) {
        // linked list
        for (auto frame = data_.l.head; frame; ) {
            auto next = (data_frame*)frame->header.next;
            alloc.deallocate(frame);
            frame = next;
        }
        data_.l.head = nullptr;
        data_.l.tail = nullptr;
    } else if (size_ > 0) {
        // static array
        for (auto& frame : data_.vec) {
            alloc.deallocate(frame);
        }
    }
    size_ = 0;
}

bool data_frame_storage::has_frame(int32_t index) const {
    if (size_ > small_frame_limit) {
        // use bitset
        if (index < data_.l.bitset.size()) {
            return data_.l.bitset.test(index);
        } else {
            // (slow) fallback for huge frame counts
            // TODO: should we just set a static limit instead?
            return find_frame(index);
        }
    } else {
        // check array slot
        return data_.vec[index] != nullptr;
    }
}

void data_frame_storage::add_frame(int32_t index, data_frame* frame) {
    // LOG_DEBUG("data_frame_storage: add frame " << index << "/" << size());
    assert(size_ > 0 && index >= 0 && index < size_);
    frame->header.frame_index = index;
    if (size_ > small_frame_limit) {
        // add to linked list
        if (auto tail = &data_.l.tail->header; tail && index > tail->frame_index) {
            // a) append (this is the typical pattern!)
            frame->header.next = nullptr;
            tail->next = &frame->header;
            data_.l.tail = frame;
        } else {
            auto ptr = &data_.l.head->header;
            if (ptr == nullptr || index < ptr->frame_index) {
                // b) prepend
                frame->header.next = ptr;
                data_.l.head = frame;
            } else {
                // c) insert
                assert(ptr->frame_index != index);
                while (ptr->next && index > ptr->next->frame_index) {
                    assert(ptr->next->frame_index != index);
                    ptr = ptr->next;
                }
                frame->header.next = ptr->next;
                ptr->next = &frame->header;
            }
            if (frame->header.next == nullptr) {
                // update tail!
                data_.l.tail = frame;
            }
        }
        if (index < data_.l.bitset.size()) {
            // update bitset
            data_.l.bitset.set(index, true);
        }
    } else {
        // static array
        assert(data_.vec[index] == nullptr);
        data_.vec[index] = frame;
    }
}

void data_frame_storage::copy_frames(AooByte *buffer, size_t total_size) {
    auto ptr = buffer;
    auto end = buffer + total_size;
    if (size_ > small_frame_limit) {
        // linked list
        int index = 0;
        for (auto frame = data_.l.head; frame; frame = (data_frame*)frame->header.next, ++index) {
            assert(frame->header.frame_index == index);
            memcpy(ptr, frame->data, frame->header.size);
            ptr += frame->header.size;
        }
    } else {
        // static array
        for (int i = 0; i < size_; ++i) {
            memcpy(ptr, data_.vec[i]->data, data_.vec[i]->header.size);
            ptr += data_.vec[i]->header.size;
        }
    }
    assert(ptr == end);
}

data_frame* data_frame_storage::find_frame(int32_t index) const {
    assert(size_ > small_frame_limit);
    for (auto ptr = data_.l.head; ptr; ptr = (data_frame*)ptr->header.next) {
        if (ptr->header.frame_index == index) {
            return ptr;
        } else if (ptr->header.frame_index > index) {
            break;
        }
    }
    return nullptr;
}

} // aoo
