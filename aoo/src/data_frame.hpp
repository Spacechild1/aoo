#pragma once

#include "detail.hpp"

#include <array>
#include <atomic>
#include <bitset>
#include <cassert>
#include <climits>

namespace aoo {

//-------------------------- data_frame --------------------------------//

// used in data_frame_allocator and received_block
struct data_frame_header {
    int32_t size;
    int16_t bin_index;
    int16_t frame_index;
    data_frame_header *next;
};

struct data_frame {
    data_frame_header header;
    AooByte data[1];
};

namespace detail {

inline constexpr size_t calc_bin_count(size_t min, size_t max) {
    size_t size = min;
    size_t count = 0;
    while (size <= max) {
        size *= 2;
        count++;
    }
    return count;
}

} // detail

//---------------------- data_frame_allocator ----------------------------//

#ifndef AOO_DATA_FRAME_LEAK_DETECTION
// enabled by default in debug builds
#if !defined(NDEBUG)
#define AOO_DATA_FRAME_LEAK_DETECTION 1
#else
#define AOO_DATA_FRAME_LEAK_DETECTION 0
#endif
#endif // AOO_DATA_FRAME_LEAK_DETECTION

#ifndef AOO_DEBUG_DATA_FRAME_ALLOCATOR
#define AOO_DEBUG_DATA_FRAME_ALLOCATOR 0
#endif

#if AOO_DEBUG_DATA_FRAME_ALLOCATOR
#undef AOO_DATA_FRAME_LEAK_DETECTION
#define AOO_DATA_FRAME_LEAK_DETECTION 1
#endif

// Stream data frames have bounded allocation sizes
// and show a similar allocation pattern over time.
// This is a perfect fit for a pooling allocator!
// Stream messages may disturb the allocation pattern,
// but only up to a certain degree. Typically, the data
// frame limit lies between 512 and 1024 bytes,
// so the frames will be distributed across 4-5 buckets.
// The main objectives are:
// 1. reduce memory usage,
// 2. prevent memory fragmentation
// 3. support heterogenous stream data sizes

class data_frame_allocator {
public:
    static constexpr size_t min_bin_size = 64;
    static constexpr size_t max_bin_size = AOO_MAX_PACKET_SIZE;
    static constexpr size_t min_bin_ilog2_size = 6;
    static_assert(1 << min_bin_ilog2_size == min_bin_size,
                  "bad value(s) for min_bin_[ilog2]_size");

    data_frame_allocator() = default;
    ~data_frame_allocator() { release_memory(); }

    data_frame* allocate(int32_t size);
    void deallocate(data_frame *frame);

    void release_memory();
private:
    static size_t size_to_bin(size_t size);
    static size_t bin_to_alloc_size(size_t index);

    static constexpr size_t bin_count = detail::calc_bin_count(min_bin_size, max_bin_size);
    std::array<std::atomic<data_frame_header*>, bin_count> bins_{}; // initialize!
#if AOO_DATA_FRAME_LEAK_DETECTION
    std::atomic<int> num_alloc_bytes_{0};
    std::atomic<int> num_alloc_frames_{0};
#endif
#if AOO_DEBUG_DATA_FRAME_ALLOCATOR
    std::atomic<int> num_frames_{0};
#endif
};

//---------------------- data_frame_storage ----------------------------//

struct data_frame_storage {
    static constexpr size_t frame_limit = 256;
    static constexpr size_t small_frame_limit =
        2 + frame_limit / (sizeof(uintptr_t) * CHAR_BIT);

    data_frame_storage() : data_{} {}

    data_frame_storage(data_frame_storage&& other) noexcept
        : data_(other.data_), size_(other.size_) {
        other.size_ = 0;
    }

    ~data_frame_storage() { assert(size_ == 0); }

    data_frame_storage& operator=(data_frame_storage&& other) noexcept {
        data_ = other.data_;
        size_ = other.size_;
        other.size_ = 0;
        return *this;
    }

    size_t size() const { return size_; }

    void init(size_t size);

    void clear(data_frame_allocator& alloc);

    bool has_frame(int32_t index) const;

    void add_frame(int32_t index, data_frame* frame);

    void copy_frames(AooByte *buffer, size_t total_size);
private:
    union data_union {
        // static vector for small frame count
        std::array<data_frame*, small_frame_limit> vec;
        // linked list + bitset for larger frame count
        struct {
            data_frame *head;
            data_frame *tail;
            std::bitset<frame_limit> bitset;
        } l; // can't be anonymous...
    } data_;
    int32_t size_ = 0;

    data_frame* find_frame(int32_t index) const;
};

} // aoo
