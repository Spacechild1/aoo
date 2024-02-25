#pragma once

#include "detail.hpp"

#include <vector>

namespace aoo {

class dynamic_resampler {
public:
    ~dynamic_resampler() { free_buffer(); }

    void setup(int32_t nfrom, int32_t nto, bool fixed_n,
               int32_t srfrom, int32_t srto, bool fixed_sr,
               int32_t nchannels, AooResampleMethod mode);
    void reset();
    void update(double srfrom, double srto);

    bool write(const AooSample* data, int32_t nframes);
    bool read(AooSample* data, int32_t nframes);

    int32_t capacity() const { return size_; }
    double balance() const {
        return balance_;
    }
    double ratio() const { return ideal_ratio_; }
    int32_t latency() const { return latency_; } // in terms of the writer
    bool bypass() const { return bypass_; }
    bool fixed_sr() const { return fixed_sr_; }
private:
    void free_buffer();

    enum class resample_method : uint8_t {
        none,
        skip,
        hold,
        linear,
        cubic
    };

    static const char* method_to_string(resample_method method);

    static constexpr int32_t latency_linear = 1;
    static constexpr int32_t latency_cubic = 2;
    static constexpr size_t buffer_shift = 1;
    static constexpr size_t extra_space = 3;

    AooSample *buffer_ = nullptr;
    int32_t size_ = 0;
    int16_t nchannels_ = 0;
    int16_t latency_ = 0;
    resample_method method_ = resample_method::none;
    bool bypass_ = false;
    bool fixed_sr_ = false;
    int32_t wrpos_ = 0;
    double rdpos_ = 0;
    double balance_ = 0;
    double advance_ = 1.0;
    double ideal_ratio_ = 1.0;
};

}
