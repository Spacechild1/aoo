#pragma once

#include "aoo.h"

#include <cstdint>
#include <cstring>
#include <ostream>

#define DO_LOG(level, msg) do { aoo::Log(level) << msg; } while (false)

#define LOG_ALL(msg) DO_LOG(kAooLogLevelNone, msg)

#if AOO_LOG_LEVEL >= kAooLogLevelError
# define LOG_ERROR(msg) DO_LOG(kAooLogLevelError, msg)
#else
# define LOG_ERROR(msg)
#endif

#if AOO_LOG_LEVEL >= kAooLogLevelWarning
# define LOG_WARNING(msg) DO_LOG(kAooLogLevelWarning, msg)
#else
# define LOG_WARNING(msg)
#endif

#if AOO_LOG_LEVEL >= kAooLogLevelVerbose
# define LOG_VERBOSE(msg) DO_LOG(kAooLogLevelVerbose, msg)
#else
# define LOG_VERBOSE(msg)
#endif

#if AOO_LOG_LEVEL >= kAooLogLevelDebug
# define LOG_DEBUG(msg) DO_LOG(kAooLogLevelDebug, msg)
#else
# define LOG_DEBUG(msg)
#endif

namespace aoo {

class Log final : std::streambuf, public std::ostream {
public:
    static const int32_t buffer_size = 256;

    Log(AooLogLevel level = kAooLogLevelNone)
        : std::ostream(this), level_(level) {}
    ~Log() {
        buffer_[pos_] = 0;
        aoo_logMessage(level_, buffer_);
    }
private:
    using int_type = std::streambuf::int_type;
    using char_type = std::streambuf::char_type;

    int_type overflow(int_type c) override;

    std::streamsize xsputn(const char_type *s, std::streamsize n) override;

    AooLogLevel level_;
    int32_t pos_ = 0;
    char buffer_[buffer_size];
};

inline Log::int_type Log::overflow(int_type c) {
    if (pos_ < buffer_size - 1) {
        buffer_[pos_++] = c;
        return 0;
    } else {
        return std::streambuf::traits_type::eof();
    }
}

inline std::streamsize Log::xsputn(const char_type *s, std::streamsize n) {
    auto limit = buffer_size - 1;
    if (pos_ < limit) {
        if (pos_ + n > limit) {
            n = limit - pos_;
        }
        memcpy(buffer_ + pos_, s, n);
        pos_ += n;
        return n;
    } else {
        return 0;
    }
}

}
