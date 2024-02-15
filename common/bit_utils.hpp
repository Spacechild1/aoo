#pragma once

#include <cstdint>

#ifdef _MSC_VER
#include <intrin.h>
#endif

namespace aoo {

inline uint32_t clz(uint32_t i) {
#if defined(_MSC_VER)
    unsigned long result = 0;
    if (_BitScanReverse(&result, i) != 0) {
        return 31 - result;
    } else {
        return 32;
    }
#elif defined(__GNUC__)
    if (i != 0) {
        return __builtin_clz(i);
    } else {
        return 32;
    }
#else
#error "compiler not supported"
#endif
}

} // aoo
