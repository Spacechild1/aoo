#include "packet_buffer.hpp"

#include "common/utils.hpp"

#include <algorithm>
#include <cassert>

namespace aoo {

//-------------------------- sent_block -----------------------------//

void sent_block::set(const data_packet& d, int16_t frame_size_)
{
    sequence = d.sequence;
    message_size = d.msg_size;
    samplerate = d.samplerate;
    flags = d.flags;
    num_frames = d.num_frames;
    frame_size = frame_size_;
    if (d.total_size > 0) {
        buffer_.assign(d.data, d.data + d.total_size);
    } else {
        buffer_.clear();
    }
}

int32_t sent_block::get_frame(int32_t index, AooByte *data, int32_t count) const {
    assert((frame_size > 0) == (num_frames > 0));
    assert(index >= 0 && index < num_frames);
    auto onset = index * frame_size;
    auto size = (index == num_frames - 1) ? buffer_.size() - onset : frame_size;
    if (count >= size){
        auto ptr = buffer_.data() + onset;
        std::copy(ptr, ptr + size, data);
        return size;
    } else {
        LOG_ERROR("sent_block::get_frame(): buffer too small!");
        return 0;
    }
}

//-------------------- history_buffer ----------------------//

void history_buffer::clear(){
    head_ = 0;
    size_ = 0;
}

void history_buffer::resize(int32_t n){
    buffer_.resize(n);
#if 1
    buffer_.shrink_to_fit();
#endif
    clear();
}

sent_block * history_buffer::find(int32_t seq){
    // the code below only works if the buffer is not empty!
    if (size_ > 0){
        // check if sequence number is outdated
        // (tail always starts at buffer begin and becomes
        // equal to head once the buffer is full)
        auto head = buffer_.begin() + head_;
        auto tail = (size_ == capacity()) ? head : buffer_.begin();
        if (seq < tail->sequence){
            LOG_DEBUG("history buffer: block " << seq << " too old");
            return nullptr;
        }

    #if 0
        // linear search
        auto end = buffer_.begin() + size_;
        for (auto it = buffer_.begin(); it != end; ++it){
            if (it->sequence == seq){
                return &(*it);
            }
        }
    #else
        // binary search
        auto dofind = [&](auto begin, auto end) -> sent_block * {
            auto result = std::lower_bound(begin, end, seq, [](auto& a, auto& b){
                return a.sequence < b;
            });
            if (result != end && result->sequence == seq){
                return &(*result);
            } else {
                return nullptr;
            }
        };
        if (head != tail){
            // buffer not full, just search range [tail, head]
            auto result = dofind(tail, head);
            if (result){
                return result;
            }
        } else {
            // blocks are always pushed in chronological order,
            // so the ranges [begin, head] and [head, end] will always be sorted.
            auto result = dofind(buffer_.begin(), head);
            if (!result){
                result = dofind(head, buffer_.end());
            }
            if (result){
                return result;
            }
        }
    #endif
    }

    LOG_ERROR("history buffer: couldn't find block " << seq);
    return nullptr;
}

sent_block * history_buffer::push()
{
    assert(!buffer_.empty());
    auto old = head_++;
    if (head_ >= capacity()){
        head_ = 0;
    }
    if (size_ < capacity()){
        ++size_;
    }
    return &buffer_[old];
}

//---------------------- received_block ------------------------//

void received_block::init(int32_t seq)
{
    assert(frames_.size() == 0);

    sequence = seq;
    total_size = 0;
    message_size = 0;
    samplerate = 0;
    channel = 0;
    received_frames = -1; // sentinel for placeholder block!
    num_tries_ = 0;
    timestamp_ = 0;
}

void received_block::init(const data_packet& d)
{
    assert(frames_.size() == 0);
    assert((d.total_size > 0) == (d.num_frames > 0));

    auto prev_sequence = d.sequence;
    sequence = d.sequence;
    total_size = d.total_size;
    message_size = d.msg_size;
    flags = d.flags;
    tt = d.tt;
    samplerate = d.samplerate;
    channel = d.channel;
    received_frames = 0; // !
    // keep timestamp and numtries if we're actually reiniting
    if (sequence != prev_sequence) {
        timestamp_ = 0;
        num_tries_ = 0;
    }
    frames_.init(d.num_frames);
}

bool received_block::update(double time, double interval) {
    if (timestamp_ > 0 && (time - timestamp_) < interval){
        return false;
    }
    timestamp_ = time;
    num_tries_++;
#if AOO_DEBUG_JITTER_BUFFER
    LOG_DEBUG("jitter buffer: request block " << sequence);
#endif
    return true;
}

//----------------------- jitter_buffer ----------------------//

jitter_buffer::~jitter_buffer() {
    reset();
    // ~data_frame_allocator will actually release the memory
}

void jitter_buffer::reset() {
    // first deallocate all frames!
    for (auto& b : data_) {
        b.clear(alloc_);
    }
    // but don't release the memory!
    head_ = tail_ = size_ = 0;
    last_popped_ = last_pushed_ = sentinel;
}

void jitter_buffer::resize(int32_t n) {
    // first reset to release frames
    reset();
    // finally resize the queue
    data_.resize(n);
}

received_block* jitter_buffer::find(int32_t seq){
    // first try the end, as we most likely have to complete the most recent block
    if (empty()){
        return nullptr;
    } else if (back().sequence == seq){
        return &back();
    }
#if 0
    // linear search
    if (head_ > tail_){
        for (int32_t i = tail_; i < head_; ++i){
            if (data_[i].sequence == seq){
                return &data_[i];
            }
        }
    } else {
        for (int32_t i = 0; i < head_; ++i){
            if (data_[i].sequence == seq){
                return &data_[i];
            }
        }
        for (int32_t i = tail_; i < capacity(); ++i){
            if (data_[i].sequence == seq){
                return &data_[i];
            }
        }
    }
    return nullptr;
#else
    // binary search
    // (blocks are always pushed in chronological order)
    auto dofind = [&](auto begin, auto end) -> received_block * {
        auto result = std::lower_bound(begin, end, seq, [](auto& a, auto& b){
            return a.sequence < b;
        });
        if (result != end && result->sequence == seq){
            return &(*result);
        } else {
            return nullptr;
        }
    };

    auto begin = data_.data();
    if (head_ > tail_){
        // [tail, head]
        return dofind(begin + tail_, begin + head_);
    } else {
        // [begin, head] + [tail, end]
        auto result = dofind(begin, begin + head_);
        if (!result){
            result = dofind(begin + tail_, begin + data_.capacity());
        }
        return result;
    }
#endif
}

received_block* jitter_buffer::push(int32_t seq){
    assert(!full());
    auto current = head_;
    if (++head_ == capacity()){
        head_ = 0;
    }
    size_++;
    assert((last_pushed_ == sentinel) || ((seq - last_pushed_) == 1));
    last_pushed_ = seq;
    auto block = &data_[current];
#if 0
    block->clear(alloc_);
#endif
    return block;
}

void jitter_buffer::pop(){
    assert(!empty());
    data_[tail_].clear(alloc_); // !
    last_popped_ = data_[tail_].sequence;
    if (++tail_ == capacity()){
        tail_ = 0;
    }
    size_--;
}

received_block& jitter_buffer::front(){
    assert(!empty());
    return data_[tail_];
}

const received_block& jitter_buffer::front() const {
    assert(!empty());
    return data_[tail_];
}

received_block& jitter_buffer::back(){
    assert(!empty());
    auto index = head_ - 1;
    if (index < 0){
        index = capacity() - 1;
    }
    return data_[index];
}

const received_block& jitter_buffer::back() const {
    assert(!empty());
    auto index = head_ - 1;
    if (index < 0){
        index = capacity() - 1;
    }
    return data_[index];
}

jitter_buffer::iterator jitter_buffer::begin(){
    if (empty()){
        return end();
    } else {
        return iterator(this, &data_[tail_]);
    }
}

jitter_buffer::const_iterator jitter_buffer::begin() const {
    if (empty()){
        return end();
    } else {
        return const_iterator(this, &data_[tail_]);
    }
}

jitter_buffer::iterator jitter_buffer::end(){
    return iterator(this);
}

jitter_buffer::const_iterator jitter_buffer::end() const {
    return const_iterator(this);
}

std::ostream& operator<<(std::ostream& os, const jitter_buffer& jb){
    os << "jitterbuffer (" << jb.size() << " / " << jb.capacity() << "): ";
    for (auto& b : jb){
        os << "\n" << b.sequence << " " << "(" << b.received_frames << "/" << b.num_frames() << ")";
    }
    return os;
}

} // aoo
