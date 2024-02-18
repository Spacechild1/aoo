#include "message_buffer.hpp"

namespace aoo {
namespace net {

//------------------------ sent_message -----------------------------//

#define AOO_MAX_RESEND_INTERVAL 1.0
#define AOO_RESEND_INTERVAL_BACKOFF 2.0

sent_message::sent_message(const metadata& data, aoo::time_tag tt, int32_t sequence,
                          int32_t num_frames, int32_t frame_size, float resend_interval)
    : data_(data), tt_(tt), sequence_(sequence), num_frames_(num_frames),
      frame_size_(frame_size), resend_interval_(resend_interval) {
    // LATER support messages with arbitrary number of frames
    assert(num_frames <= (int32_t)frames_.size());
    for (int i = 0; i < num_frames; ++i){
        frames_[i] = true;
    }
}

bool sent_message::need_resend(aoo::time_tag now) {
    if (next_time_.is_empty()) {
        next_time_ = now + aoo::time_tag::from_seconds(resend_interval_);
    } else {
        if (now >= next_time_) {
            resend_interval_ *= AOO_RESEND_INTERVAL_BACKOFF;
            if (resend_interval_ > AOO_MAX_RESEND_INTERVAL) {
                resend_interval_ = AOO_MAX_RESEND_INTERVAL;
            }
            next_time_ += aoo::time_tag::from_seconds(resend_interval_);
            return true;
        }
    }
    return false;
}

void sent_message::get_frame(int32_t index, const AooByte *& data, int32_t& size) {
    if (num_frames_ == 1) {
        // single-frame message
        data = data_.data();
        size = data_.size();
    } else {
        // multi-frame message
        if (index == (num_frames_ - 1)) {
            // last frame
            auto onset = (index - 1) * frame_size_;
            data = data_.data() + onset;
            size = data_.size() - onset;
        } else {
            data = data_.data() + index * frame_size_;
            size = frame_size_;
        }
    }
}

//------------------------ message_send_buffer ----------------------//

sent_message& message_send_buffer::push(sent_message&& msg) {
    data_.push_back(std::move(msg));
    return data_.back();
}

void message_send_buffer::pop() {
    data_.pop_front();
}

sent_message* message_send_buffer::find(int32_t seq) {
    for (auto& item : data_) {
        if (item.sequence_ ==  seq) {
            return &item;
        }
    }
    return nullptr;
}


//------------------------ received_message ------------------------//

received_message::received_message(received_message&& other) noexcept
    : sequence_(other.sequence_), size_(other.size_),
      tt_(other.tt_), data_(other.data_), type_(other.type_),
      num_frames_(other.num_frames_), frames_(other.frames_) {
    other.data_ = nullptr;
    other.size_ = 0;
}

received_message& received_message::operator=(received_message&& other) noexcept {
    sequence_ = other.sequence_;
    size_ = other.size_;
    tt_ = other.tt_;
    data_ = other.data_;
    type_ = other.type_;
    num_frames_ = other.num_frames_;
    frames_ = other.frames_;
    other.data_ = nullptr;
    other.size_ = 0;
    return *this;
}

void received_message::init(AooDataType type, time_tag tt,
                            int32_t num_frames, int32_t size) {
    assert(data_ == nullptr);
    tt_ = tt;
    size_ = size;
    data_ = (AooByte*)aoo::allocate(size);
    type_ = type;
    num_frames_ = num_frames;
    // TODO: support messages with arbitrary number of frames
    assert(num_frames <= frames_.size());
    for (int i = 0; i < num_frames; ++i){
        frames_[i] = true;
    }
}

void received_message::add_frame(int32_t index, const AooByte *data, int32_t n) {
    assert(data_ != nullptr && !complete());
    assert(index < num_frames_);
    // TODO: allow varying frame sizes!
    if (index == num_frames_ - 1){
        std::copy(data, data + n, data_ + size_ - n);
    } else {
        std::copy(data, data + n, data_ + (index * n));
    }
    frames_[index] = 0;
}

//------------------------- message_receive_buffer ------------------//

received_message& message_receive_buffer::push(received_message&& msg) {
    last_pushed_ = msg.sequence_;
    data_.push_back(std::move(msg));
    return data_.back();
}

void message_receive_buffer::pop() {
    last_popped_ = data_.front().sequence_;
    data_.pop_front();
}

received_message* message_receive_buffer::find(int32_t seq) {
    for (auto& item : data_) {
        if (item.sequence_ ==  seq) {
            return &item;
        }
    }
    return nullptr;
}

} // namespace net
} // namespace aoo
