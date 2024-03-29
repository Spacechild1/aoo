#pragma once

#include <algorithm>
#include <stdint.h>
#include <vector>

namespace aoo {

template<typename T, typename Comp = std::less<T>, typename Alloc = std::allocator<T>>
class priority_queue
{
public:
    bool empty() const {
        return queue_.empty();
    }

    size_t size() const {
        return queue_.size();
    }

    void clear() {
        queue_.clear();
        counter_ = 0;
    }

    const T& top() const {
        return queue_.front().object_;
    }

    void push(const T& x) {
        queue_.push_back(proxy{ x, counter_++ });
        std::push_heap(queue_.begin(), queue_.end(), proxy_comp{});
    }

    void push(T&& x) {
        queue_.push_back(proxy{ std::move(x), counter_++ });
        std::push_heap(queue_.begin(), queue_.end(), proxy_comp{});
    }

    template<typename... Args>
    void emplace(Args&&... args) {
        push(T(std::forward<Args>(args)...));
    }

    void pop() {
        std::pop_heap(queue_.begin(), queue_.end(), proxy_comp{});
        queue_.pop_back();
        if (queue_.empty()) {
            counter_ = 0;
        }
    }
private:
    struct proxy
    {
        proxy(T&& o, size_t c)
            : object_(std::forward<T>(o))
            , order_(c) {}

        T object_;
        size_t order_;
    };

    struct proxy_comp
    {
        bool operator()(const proxy& l, const proxy& r) const
        {
            Comp comp;
            if (comp(l.object_, r.object_))
                return true;
            if (comp(r.object_, l.object_))
                return false;
            // NB: higher order means lower priority!
            return l.order_ > r.order_;
        }
    };
    using rebind_alloc = typename std::allocator_traits<Alloc>::template rebind_alloc<proxy>;
    std::vector<proxy, rebind_alloc> queue_;
    size_t counter_ = 0;
};

} // aoo
