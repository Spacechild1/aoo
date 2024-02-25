#include "resampler.hpp"
#include "common/utils.hpp"

#include <algorithm>

namespace aoo {

void dynamic_resampler::free_buffer() {
    if (buffer_) {
        auto alloc_size = (size_ * nchannels_) * sizeof(AooSample);
        aoo::deallocate(buffer_, alloc_size);
        buffer_ = nullptr;
        size_ = 0;
    }
}

const char* dynamic_resampler::method_to_string(resample_method method) {
    switch (method) {
    case resample_method::none:
        return "none";
    case resample_method::skip:
        return "skip";
    case resample_method::hold:
        return "hold";
    case resample_method::linear:
        return "linear";
    default:
        return "?";
    }
}

// extra space for samplerate fluctuations and non-pow-of-2 blocksizes.
// must be larger than 2!
#define AOO_RESAMPLER_SPACE 2.5

void dynamic_resampler::setup(int32_t nfrom, int32_t nto, bool fixed_n,
                              int32_t srfrom, int32_t srto, bool fixed_sr,
                              int32_t nchannels, AooResampleMethod mode) {
    ideal_ratio_ = (double)srto / (double)srfrom;
    if (fixed_sr) {
        if (srfrom == srto) {
            method_ = resample_method::none; // no resampling required
        } else if (srto < srfrom && (srfrom % srto) == 0) {
            // downsampling with (fixed) integer ratio
            method_ = resample_method::skip;
        } else {
            switch (mode) {
            case kAooResampleHold:
                method_ = resample_method::hold;
                break;
            case kAooResampleLinear:
                method_ = resample_method::linear;
                break;
            case kAooResampleCubic:
                method_ = resample_method::cubic;
                break;
            default:
                LOG_ERROR("bad resample method");
                method_ = resample_method::linear;
            }
        }
    } else {
        method_ = resample_method::linear;
    }
    fixed_sr_ = fixed_sr;
    bool reblock = !fixed_n || nfrom != nto;
    bypass_ = (method_ == resample_method::none) && !reblock;
    if (bypass_) {
    #if AOO_DEBUG_RESAMPLER
        LOG_DEBUG("resampler setup: bypass");
    #endif
        free_buffer();
    } else {
        int32_t size;
        if (ideal_ratio_ < 1.0) {
            // downsampling
            size = std::max<int32_t>(nfrom, (double)nto / ideal_ratio_ + 0.5);
        } else {
            // upsampling
            size = std::max<int32_t>(nfrom, nto);
        }
        size *= AOO_RESAMPLER_SPACE;
    #if AOO_DEBUG_RESAMPLER
        LOG_DEBUG("resampler setup: reblock from " << nfrom << " to " << nto
                  << (fixed_n ? " (fixed)" : "") << ", resample from " << srfrom
                  << " to " << srto << (fixed_sr ? " (fixed)" : "") << ", method: "
                  << method_to_string(method_) << ", capacity: " << size);
    #endif
        auto old_nsamples = size_ * nchannels_;
        auto nsamples = size * nchannels;
        if (old_nsamples != nsamples) {
            free_buffer();
            buffer_ = (AooSample*)aoo::allocate(nsamples * sizeof(AooSample));
        }
    #if 1
        std::fill(buffer_, buffer_ + nsamples, 0);
    #endif
        size_ = size;
    }
    nchannels_ = nchannels;

    if (method_ != resample_method::none) {
        update(srfrom, srto);
    }

    reset();
}

void dynamic_resampler::reset() {
    if (method_ == resample_method::cubic) {
        assert(buffer_ != nullptr);
        // write two frame of zero(s) and set the last frame to zero.
        std::fill(buffer_, buffer_ + latency_cubic * nchannels_, 0);
        auto end = buffer_ + size_ * nchannels_;
        std::fill(end - nchannels_, end, 0);
        wrpos_ = latency_cubic;
        rdpos_ = 0.0;
        balance_ = latency_cubic;
        latency_ = latency_cubic;
    } else if (method_ == resample_method::linear) {
        assert(buffer_ != nullptr);
        // write one frame of zero(s)
        // TODO: the latency could be reduced further.
        // For example, with a (fixed?) upsampling factor of 2
        // we can actually start reading at 0.5.
        std::fill(buffer_, buffer_ + latency_linear * nchannels_, 0);
        wrpos_ = latency_linear;
        rdpos_ = 0.0;
        balance_ = latency_linear;
        latency_ = latency_linear;
    } else {
        wrpos_ = 0;
        rdpos_ = 0;
        balance_ = 0;
        latency_ = 0;
    }
}

void dynamic_resampler::update(double srfrom, double srto) {
    advance_ = srfrom / srto;
#if AOO_DEBUG_RESAMPLER
    auto ratio = srto / srfrom;
    LOG_ALL("srfrom: " << srfrom << ", srto: " << srto << ", ratio: " << ratio);
    LOG_ALL("balance: " << balance() << ", capacity: " << capacity());
#endif
}

bool dynamic_resampler::write(const AooSample *data, int32_t nframes) {
    auto balance = balance_;
    auto space = (int32_t)((double)size_ - balance);
    // leave extra space for cubic interpolation
    if ((space - 1) < nframes) {
        return false;
    }
    auto pos = wrpos_;
    auto end = wrpos_ + nframes;
    if (end > size_) {
        auto split = size_ - pos;
        std::copy(data, data + (split * nchannels_), buffer_ + (pos * nchannels_));
        std::copy(data + (split * nchannels_), data + (nframes * nchannels_), buffer_);
        wrpos_ = end - size_;
    } else {
        std::copy(data, data + (nframes * nchannels_), buffer_ + (pos * nchannels_));
        wrpos_ = (end == size_) ? 0 : end;
    }
    balance_ = balance + nframes;
    return true;
}

bool dynamic_resampler::read(AooSample *data, int32_t nframes) {
    switch (method_) {
    case resample_method::cubic: {
        // linear interpolation
        auto fadvance = advance_;
        auto balance = balance_;
        auto readframes = (double)nframes * fadvance;
        if ((balance - (double)latency_cubic) < readframes) {
            return false;
        }
        auto size = size_;
        auto nchannels = (int32_t)nchannels_;
        auto pos = rdpos_;
        auto start = pos;
        auto limit = (double)size;
        auto nsamples = nframes * nchannels;
        const AooSample one_over_six = 1./6.;

        for (int i = 0; i < nsamples; i += nchannels) {
            auto index1 = (int32_t)pos;
            auto index0 = index1 - 1;
            if (index0 < 0) {
                index0 += size;
            }
            auto index2 = index1 + 1;
            if (index2 >= size) {
                index2 -= size;
            }
            auto index3 = index1 + 2;
            if (index3 >= size) {
                index3 -= size;
            }
            auto fract = (AooSample)(pos - (double)index1);
            for (int j = 0; j < nchannels; ++j) {
                auto a = buffer_[index0 * nchannels + j];
                auto b = buffer_[index1 * nchannels + j];
                auto c = buffer_[index2 * nchannels + j];
                auto d = buffer_[index3 * nchannels + j];
                // taken from Pd's [tabread4~]
                auto cminusb = c - b;
                data[i + j] = b + fract * (
                    cminusb - one_over_six * ((AooSample)1.0 - fract) * (
                        (d - a - (AooSample)3.0 * cminusb) * fract +
                        (d + a * (AooSample)2.0 - b * (AooSample)3.0)
                    )
                );
            }
            pos += fadvance;
            if (pos >= limit) {
                pos -= limit;
            }
        }
#if 1
        // avoid cumulative floating point error
        pos = start + readframes;
        if (pos >= limit) {
            pos -= limit;
        }
#endif
        rdpos_ = pos;
        balance_ = balance - readframes;
        break;
    }
    case resample_method::linear: {
        // linear interpolation
        auto fadvance = advance_;
        auto balance = balance_;
        auto readframes = (double)nframes * fadvance;
        if ((balance - (double)latency_linear) < readframes) {
            return false;
        }
        auto size = size_;
        auto nchannels = (int32_t)nchannels_;
        auto pos = rdpos_;
        auto start = pos;
        auto limit = (double)size;
        auto nsamples = nframes * nchannels;
        for (int i = 0; i < nsamples; i += nchannels) {
            auto index = (int32_t)pos;
            auto index2 = index + 1;
            if (index2 >= size) {
                index2 -= size;
            }
            auto fract = (AooSample)(pos - (double)index);
            for (int j = 0; j < nchannels; ++j) {
                auto a = buffer_[index * nchannels + j];
                auto b = buffer_[index2 * nchannels + j];
                data[i + j] = a + (b - a) * fract;
            }
            pos += fadvance;
            if (pos >= limit) {
                pos -= limit;
            }
        }
    #if 1
        // avoid cumulative floating point error
        pos = start + readframes;
        if (pos >= limit) {
            pos -= limit;
        }
    #endif
        rdpos_ = pos;
        balance_ = balance - readframes;
        break;
    }
    case resample_method::hold: {
        // duplicate/skip samples
        auto fadvance = advance_;
        auto balance = balance_;
        auto readframes = (double)nframes * fadvance;
        if (balance < readframes) {
            return false;
        }
        auto nchannels = (int32_t)nchannels_;
        auto pos = rdpos_;
        auto start = pos;
        auto limit = (double)size_;
        auto nsamples = nframes * nchannels;
        for (int i = 0; i < nsamples; i += nchannels) {
            auto ipos = (int32_t)pos;
            for (int j = 0; j < nchannels; ++j) {
                data[i + j] = buffer_[ipos * nchannels + j];
            }
            pos += fadvance;
            if (pos >= limit) {
                pos -= limit;
            }
        }
    #if 1
        // avoid cumulative floating point error
        pos = start + readframes;
        if (pos >= limit) {
            pos -= limit;
        }
    #endif
        rdpos_ = pos;
        balance_ = balance - readframes;
        break;
    }
    case resample_method::skip: {
        // just skip samples
        auto iadvance = (int32_t)advance_;
        auto ibalance = (int32_t)balance_;
        auto ireadframes = nframes * iadvance;
        if (ibalance < ireadframes) {
            return false;
        }
        auto limit = size_;
        auto nchannels = (int32_t)nchannels_;
        auto nsamples = nframes * nchannels;
        auto ipos = (int32_t)rdpos_;
        for (int i = 0; i < nsamples; i += nchannels) {
            for (int j = 0; j < nchannels; ++j) {
                data[i + j] = buffer_[ipos * nchannels + j];
            }
            ipos += iadvance;
            if (ipos >= limit) {
                ipos -= limit;
            }
        }
        rdpos_ = ipos;
        balance_ = ibalance - ireadframes;
        break;
    }
    default: {
        // only reblocking -> just copy samples
        // NB: balance is never fractional!
        auto ibalance = (int32_t)balance_;
        if (ibalance < nframes) {
            return false;
        }
        auto size = size_;
        auto nchannels = (int32_t)nchannels_;
        auto pos = (int32_t)rdpos_;
        auto end = pos + nframes;
        if (end > size_) {
            auto n1 = size_ - pos;
            auto n2 = end - size;
            std::copy(buffer_ + (pos * nchannels), buffer_ + (size * nchannels), data);
            std::copy(buffer_, buffer_ + (n2 * nchannels), data + (n1 * nchannels));
            rdpos_ = n2;
        } else {
            std::copy(buffer_ + (pos * nchannels), buffer_ + (end * nchannels), data);
            rdpos_ = (end == size) ? 0 : end;
        }
        balance_ = ibalance - nframes;
        break;
    }
    }
    return true;
}

} // aoo
