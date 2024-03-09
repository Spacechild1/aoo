#pragma once

#include "detail.hpp"

#include <bitset>
#include <list>

namespace aoo {
namespace net {

//-------------------------- sent_message -----------------------------//

struct sent_message {
    sent_message(const metadata& data, aoo::time_tag tt, int32_t sequence,
                 int32_t num_frames, int32_t frame_size, float resend_interval);

    // methods
    bool need_resend(aoo::time_tag now);

    bool has_frame(int32_t index) const {
        return !frames_[index];
    }

    void get_frame(int32_t index, const AooByte *& data, int32_t& size);

    void ack_frame(int32_t index) {
        frames_[index] = false;
    }

    void ack_all() {
        frames_.reset();
    }

    bool complete() const { return frames_.none(); }

    // data
    aoo::metadata data_;
    aoo::time_tag tt_;
    int32_t sequence_;
    int32_t num_frames_;
    int32_t frame_size_;
private:
    aoo::time_tag next_time_;
    double resend_interval_;
    std::bitset<256> frames_;
};

//-------------------------- message_send_buffer -----------------------------//

class message_send_buffer {
public:
    message_send_buffer() = default;

    using iterator = std::list<sent_message>::iterator;
    using const_iterator = std::list<sent_message>::const_iterator;

    iterator begin() {
        return data_.begin();
    }
    const_iterator begin() const {
        return data_.begin();
    }
    iterator end() {
        return data_.end();
    }
    const_iterator end() const {
        return data_.end();
    }

    sent_message& front() {
        return data_.front();
    }
    const sent_message& front() const {
        return data_.front();
    }
    sent_message& back() {
        return data_.back();
    }
    const sent_message& back() const {
        return data_.back();
    }

    bool empty() const { return data_.empty(); }
    int32_t size() const { return data_.size(); }
    sent_message& push(sent_message&& msg);
    void pop();
    sent_message* find(int32_t seq);
private:
    std::list<sent_message> data_;
};

//-------------------------- received_message -----------------------------//

class received_message {
public:
    received_message(int32_t seq = kAooIdInvalid)
        : sequence_(seq) {}

    received_message(received_message&& other) noexcept;

    received_message& operator=(received_message&& other) noexcept;

    ~received_message() {
        if (data_) {
            aoo::deallocate(data_, size_);
        }
    }

    void init(AooDataType type, time_tag tt, int32_t num_frames,
              int32_t size);

    bool placeholder() const {
        return data_ == nullptr;
    }

    // methods
    bool has_frame(int32_t index) const {
        assert(!placeholder());
        return !frames_[index];
    }

    void add_frame(int32_t index, const AooByte *data, int32_t n);

    bool complete() const { return data_ && frames_.none(); }

    // data
    int32_t sequence_ = -1;
    int32_t size_ = 0;
    aoo::time_tag tt_;
    AooByte* data_ = nullptr;
    AooDataType type_;
    int32_t num_frames_ = 0;
protected:
    std::bitset<256> frames_ = 0;
};

//-------------------------- message_receive_buffer -----------------------------//

class message_receive_buffer {
public:
    message_receive_buffer() = default;

    using iterator = std::list<received_message>::iterator;
    using const_iterator = std::list<received_message>::const_iterator;

    iterator begin() {
        return data_.begin();
    }
    const_iterator begin() const {
        return data_.begin();
    }
    iterator end() {
        return data_.end();
    }
    const_iterator end() const {
        return data_.end();
    }

    received_message& front() {
        return data_.front();
    }
    const received_message& front() const {
        return data_.front();
    }
    received_message& back() {
        return data_.back();
    }
    const received_message& back() const {
        return data_.back();
    }

    bool empty() const { return data_.empty(); }
    bool full() const { return false; }
    int32_t size() const { return data_.size(); }

    received_message& push(received_message&& msg);
    void pop();

    received_message *find(int32_t seq);

    int32_t last_pushed() const {
        return last_pushed_;
    }
    int32_t last_popped() const {
        return last_popped_;
    }
private:
    std::list<received_message> data_;
    int32_t last_pushed_ = -1;
    int32_t last_popped_ = -1;
};

} // namespace net
} // namespace aoo
