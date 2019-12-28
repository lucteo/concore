#pragma once

#include <deque>
#include <mutex>
#include <memory>

namespace concore {

inline namespace v1 {

//! Queued that can be used concurrently by multiple threads.
//!
//! TODO: optimize this; at this point, the implementation is pretty poor
template <typename T, typename A = std::allocator<T>>
class concurrent_queue {
public:
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using size_type = size_t;
    using difference_type = ptrdiff_t;
    using allocator_type = A;
    using iterator = typename std::deque<T>::iterator;
    using const_iterator = typename std::deque<T>::const_iterator;

    explicit concurrent_queue(const allocator_type& alloc = allocator_type())
        : queue_(alloc) {}

    template <typename It>
    concurrent_queue(It begin, It end, const allocator_type& alloc = allocator_type())
        : queue_(begin, end, alloc) {}

    concurrent_queue(const concurrent_queue& other, const allocator_type& alloc = allocator_type())
        : queue_(other.queue_, alloc) {}

    concurrent_queue(concurrent_queue&& other)
        : queue_(std::move(other.queue_)) {}
    concurrent_queue(concurrent_queue&& other, const allocator_type& alloc)
        : queue_(std::move(other.queue_), alloc) {}

    concurrent_queue(std::initializer_list<T> init, const allocator_type& alloc = allocator_type())
        : queue_(init, alloc) {}

    ~concurrent_queue() = default;

    void operator=(const concurrent_queue&) = delete;
    void operator=(concurrent_queue&&) = delete;

    void push(T val) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.emplace_back(std::move(val));
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        push(T(std::forward<Args>(args)...));
    }

    bool try_pop(T& result) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty())
            return false;
        result = std::move(queue_.front());
        queue_.pop_front();
        return true;
    }

    size_type unsafe_size() const { return queue_.size(); }

    bool unsafe_empty() const { return queue_.empty(); }

    void unsafe_clear() { queue_.clear(); }

    iterator unsafe_begin() { return queue_.begin(); }
    iterator unsafe_end() { return queue_.end(); }
    const_iterator unsafe_begin() const { return queue_.begin(); }
    const_iterator unsafe_end() const { return queue_.end(); }

private:
    std::deque<T> queue_;
    std::mutex mutex_;
};

} // namespace v1
} // namespace concore