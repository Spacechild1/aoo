#pragma once

#include <type_traits>
#include <memory>
#include "SC_InterfaceTable.h"
#include "SC_World.h"

#ifndef DEBUG_RT_MEMORY
#define DEBUG_RT_MEMORY 0
#endif

namespace rt {
    template <class T>
    class allocator {
    public:
        using value_type = T;

        allocator(World *world) noexcept 
            : world_(world) {}

        template <typename U> 
        allocator(allocator<U> const& other) noexcept 
            : world_(other.world_) {}

        value_type* allocate(std::size_t n) {
            auto p = static_cast<value_type*>(world_->ft->fRTAlloc(world_, n * sizeof(T)));
        #if DEBUG_RT_MEMORY
            world_->ft->fPrint("allocate %d bytes at %p\n", n * sizeof(T), p);
        #endif
            return p;
        }

        void deallocate(value_type* p, std::size_t n) noexcept {
        #if DEBUG_RT_MEMORY
            world_->ft->fPrint("deallocate %d bytes at %p\n", n * sizeof(T), p);
        #endif
            world_->ft->fRTFree(world_, p);
        }

        World* world_; // must be public (see ctor)...
    };

    template <class T, class U>
    bool
        operator==(allocator<T> const&, allocator<U> const&) noexcept
    {
        return true;
    }

    template <class T, class U>
    bool
        operator!=(allocator<T> const& x, allocator<U> const& y) noexcept
    {
        return !(x == y);
    }

    template<typename T>
    class deleter {
    public:
        deleter(World* world)
            : world_(world) {}
        void operator()(T* ptr) {
            ptr->~T();
            world_->ft->fRTFree(world_, ptr);
        }
    private:
        World* world_;
    };

    template<typename T>
    using shared_ptr = std::shared_ptr<T>;

    template<typename T, typename... Args>
    shared_ptr<T> make_shared(World* world, Args&&... args) {
        return std::allocate_shared<T>(allocator<T>(world), std::forward<Args>(args)...);
    }

} // rt
