#pragma once

#include <memory>
#include <atomic>

namespace aoo {

//---------------- allocator -----------------------//

#if AOO_CUSTOM_ALLOCATOR || AOO_DEBUG_MEMORY

void * allocate(size_t size);

template<class T, class... U>
T * construct(U&&... args){
    auto ptr = allocate(sizeof(T));
    return new (ptr) T(std::forward<U>(args)...);
}

void deallocate(void *ptr, size_t size);

template<typename T>
void destroy(T *x){
    if (x) {
        x->~T();
        deallocate(x, sizeof(T));
    }
}

template<class T>
class allocator {
public:
    using value_type = T;

    allocator() noexcept = default;

    template<typename U>
    allocator(const allocator<U>&) noexcept {}

    template<typename U>
    allocator& operator=(const allocator<U>&) noexcept {}

    value_type* allocate(size_t n) {
        return (value_type *)aoo::allocate(sizeof(T) * n);
    }

    void deallocate(value_type* p, size_t n) noexcept {
        aoo::deallocate(p, sizeof(T) * n);
    }
};

template <class T, class U>
bool operator==(const allocator<T>&, const allocator<U>&) noexcept
{
    return true;
}

template <class T, class U>
bool operator!=(const allocator<T>& x, const allocator<U>& y) noexcept
{
    return !(x == y);
}

#else

inline void * allocate(size_t size){
    return operator new(size);
}

template<class T, class... U>
T * construct(U&&... args){
    return new T(std::forward<U>(args)...);
}

inline void deallocate(void *ptr, size_t size){
    operator delete(ptr);
}

template<typename T>
void destroy(T *x){
    delete x;
}

template<typename T>
using allocator = std::allocator<T>;

#endif

template<typename T>
class deleter {
public:
    void operator() (void *p) const {
        aoo::deallocate(p, sizeof(T));
    }
};

//------------- RT memory allocator ---------------//

void * rt_allocate(size_t size);

template<class T, class... U>
T * rt_construct(U&&... args){
    auto ptr = rt_allocate(sizeof(T));
    return new (ptr) T(std::forward<U>(args)...);
}

void rt_deallocate(void *ptr, size_t size);

template<typename T>
void rt_destroy(T *x){
    if (x) {
        x->~T();
        rt_deallocate(x, sizeof(T));
    }
}

template<class T>
class rt_allocator {
public:
    using value_type = T;

    rt_allocator() noexcept = default;

    template<typename U>
    rt_allocator(const rt_allocator<U>&) noexcept {}

    template<typename U>
    rt_allocator& operator=(const rt_allocator<U>&) noexcept {}

    value_type* allocate(size_t n) {
        return (value_type *)aoo::rt_allocate(sizeof(T) * n);
    }

    void deallocate(value_type* p, size_t n) noexcept {
        aoo::rt_deallocate(p, sizeof(T) * n);
    }
};

template <class T, class U>
bool operator==(const rt_allocator<T>&, const rt_allocator<U>&) noexcept
{
    return true;
}

template <class T, class U>
bool operator!=(const rt_allocator<T>& x, const rt_allocator<U>& y) noexcept
{
    return !(x == y);
}

template<typename T>
class rt_deleter {
public:
    void operator() (void *p) const {
        aoo::rt_deallocate(p, sizeof(T));
    }
};

void rt_memory_pool_ref();
void rt_memory_pool_unref();

class rt_memory_pool_client {
public:
    rt_memory_pool_client() {
        rt_memory_pool_ref();
    }

    ~rt_memory_pool_client() {
        rt_memory_pool_unref();
    }
};

} // namespace aoo
