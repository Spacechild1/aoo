#pragma once

#include "detail.hpp"
#include "data_frame.hpp"

namespace aoo {

struct data_frame;

struct data_packet {
    int32_t sequence;
    int32_t channel;
    aoo::time_tag tt;
    double samplerate;
    int32_t total_size;
    int32_t msg_size;
    int32_t num_frames;
    int32_t frame_index;
    union {
        const AooByte* data;
        data_frame* frame;
    };
    int32_t size;
    uint32_t flags;
};

//---------------------- sent_block ---------------------------//

class sent_block {
public:
    // methods
    void set(const data_packet& d, int32_t framesize);

    const AooByte* data() const { return buffer_.data(); }
    int32_t size() const { return buffer_.size(); }

    int32_t num_frames() const { return numframes_; }
    int32_t frame_size(int32_t which) const;
    int32_t get_frame(int32_t which, AooByte * data, int32_t n) const;

    // data
    int32_t sequence = -1;
    int32_t message_size = 0;
    double samplerate = 0;
    uint32_t flags = 0;
protected:
    aoo::vector<AooByte> buffer_;
    int32_t numframes_ = 0;
    int32_t framesize_ = 0;
};

//---------------------------- history_buffer ------------------------------//

class history_buffer {
public:
    void clear();
    bool empty() const {
        return size_ == 0;
    }
    int32_t size() const {
        return size_;
    }
    int32_t capacity() const {
        return buffer_.size();
    }
    void resize(int32_t n);
    sent_block * find(int32_t seq);
    sent_block * push();
private:
    aoo::vector<sent_block> buffer_;
    int32_t head_ = 0;
    int32_t size_ = 0;
};



//---------------------------- received_block ------------------------------//

// LATER use a (sorted) linked list of data frames (coming from the network thread)
// which are eventually written sequentially into a contiguous client-size buffer.
// This has the advantage that we don't need to preallocate memory and we can easily
// handle arbitrary number of data frames. We can still use the bitset as an optimization,
// but only up to a certain number of frames (e.g. 32); above that we do a linear search
// over the linked list.
// NB: the current practice of reserving blocksize * nchannels * sizeof(double) is
// not appropriate for stream message!

class received_block {
public:
    void init(int32_t seq);
    void init(const data_packet& d);

    void clear(data_frame_allocator& alloc) {
        frames_.clear(alloc);
        received_frames = 0;
    }

    int32_t num_frames() const { return frames_.size(); }

    bool has_frame(int32_t index) const {
        return frames_.has_frame(index);
    }

    void add_frame(int32_t index, data_frame* frame) {
        assert(received_frames >= 0);
    #if AOO_DEBUG_JITTER_BUFFER
        LOG_DEBUG("jitter buffer: add frame " << index << " with " << frame->size << " bytes");
    #endif
        frames_.add_frame(index, frame);
        received_frames++;
    }

    void copy_frames(AooByte* buffer) {
        assert(complete());
        frames_.copy_frames(buffer, total_size);
    }

    int32_t resend_count() const { return num_tries_; }

    bool complete() const { return received_frames == frames_.size(); }

    bool placeholder() const { return received_frames < 0; }

    bool empty() const { return frames_.size() == 0; }

    bool update(double time, double interval);

    // data
    int32_t sequence = -1;
    int32_t total_size = 0;
    int32_t message_size = 0;
    uint32_t flags = 0;
    uint64_t tt = 0;
    double samplerate = 0;
    int16_t channel = 0;
    int16_t received_frames = 0;
#ifndef NDEBUG
private:
#endif
    int32_t num_tries_ = 0;
    double timestamp_ = 0;
    data_frame_storage frames_;
};

//---------------------------- jitter_buffer ------------------------------//

class jitter_buffer {
public:
    template<typename T, typename U>
    class base_iterator {
        T *data_;
        U *owner_;
    public:
        base_iterator(U* owner)
            : data_(nullptr), owner_(owner){}
        base_iterator(U* owner, T* data)
            : data_(data), owner_(owner){}
        base_iterator(const base_iterator&) = default;
        base_iterator& operator=(const base_iterator&) = default;
        T& operator*() { return *data_; }
        T* operator->() { return data_; }
        base_iterator& operator++() {
            auto begin = owner_->data_.data();
            auto end = begin + owner_->data_.size();
            auto next = data_ + 1;
            if (next == end){
                next = begin;
            }
            if (next == (begin + owner_->head_)){
                next = nullptr; // sentinel
            }
            data_ = next;
            return *this;
        }
        base_iterator operator++(int) {
            base_iterator old = *this;
            operator++();
            return old;
        }
        bool operator==(const base_iterator& other){
            return data_ == other.data_;
        }
        bool operator!=(const base_iterator& other){
            return data_ != other.data_;
        }
    };

    using iterator = base_iterator<received_block, jitter_buffer>;
    using const_iterator = base_iterator<const received_block, const jitter_buffer>;

    static constexpr int32_t sentinel = INT32_MIN;

    jitter_buffer(data_frame_allocator& alloc) : alloc_(alloc) {}
    ~jitter_buffer();

    void reset();

    void resize(int32_t n);

    bool empty() const {
        return size_ == 0;
    }
    bool full() const {
        return size_ == capacity();
    }
    int32_t size() const {
        return size_;
    }
    int32_t capacity() const {
        return data_.size();
    }

    received_block* find(int32_t seq);

    received_block* push(int32_t seq);

    void pop();

    void reset_head(int32_t seq = sentinel) {
        last_pushed_ = seq;
    }

    int32_t last_pushed() const {
        return last_pushed_;
    }

    int32_t last_popped() const {
        return last_popped_;
    }

    received_block& front();
    const received_block& front() const;
    received_block& back();
    const received_block& back() const;

    iterator begin();
    const_iterator begin() const;
    iterator end();
    const_iterator end() const;

    friend std::ostream& operator<<(std::ostream& os, const jitter_buffer& b);
private:
    data_frame_allocator& alloc_;
    aoo::vector<received_block> data_;
    int32_t size_ = 0;
    int32_t head_ = 0;
    int32_t tail_ = 0;
    int32_t last_pushed_ = -1;
    int32_t last_popped_ = -1;
};

} // aoo
