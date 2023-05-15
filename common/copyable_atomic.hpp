#pragma once

#include <atomic>

namespace aoo {

template<class T>
class copyable_atomic : public std::atomic<T>
{
public:
    copyable_atomic() = default;

    constexpr copyable_atomic(T desired) :
          std::atomic<T>(desired) {}

    constexpr copyable_atomic(const copyable_atomic<T>& other) :
          copyable_atomic(other.load(std::memory_order_relaxed)) {}

    copyable_atomic& operator=(const copyable_atomic<T>& other) {
        this->store(other.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return *this;
    }
};

} // aoo
