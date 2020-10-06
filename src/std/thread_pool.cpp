#include <concore/std/thread_pool.hpp>
#include <concore/global_executor.hpp>
#include <concore/task_group.hpp>
#include <concore/spawn.hpp>

namespace concore {
namespace std_execution {
namespace detail {

struct pool_data {
    //! The group we use to control the items that were added in this pool
    task_group grp_;

    explicit pool_data(size_t num_threads) { grp_ = task_group::create(); }
    ~pool_data() {
        // Wait for all the tasks to complete
        grp_.cancel();
        wait(grp_);
    }

    // Copy is disabled
    pool_data(const pool_data&) = delete;
    pool_data& operator=(const pool_data&) = delete;

    pool_data(pool_data&&) = default;
    pool_data& operator=(pool_data&&) = default;
};

task_group get_associated_group(const static_thread_pool& pool) { return pool.impl_->grp_; }

thread_pool_scheduler::thread_pool_scheduler(pool_data* impl) noexcept
    : impl_(impl) {}
thread_pool_scheduler::thread_pool_scheduler(const thread_pool_scheduler& r) noexcept = default;
thread_pool_scheduler& thread_pool_scheduler::operator=(
        const thread_pool_scheduler& r) noexcept = default;
thread_pool_scheduler::thread_pool_scheduler(thread_pool_scheduler&& r) noexcept = default;
thread_pool_scheduler& thread_pool_scheduler::operator=(
        thread_pool_scheduler&& r) noexcept = default;

bool thread_pool_scheduler::running_in_this_thread() const noexcept {
    return false; // TODO
}

bool operator==(const thread_pool_scheduler& l, const thread_pool_scheduler& r) noexcept {
    return l.impl_ == r.impl_;
}
bool operator!=(const thread_pool_scheduler& l, const thread_pool_scheduler& r) noexcept {
    return l.impl_ != r.impl_;
}

thread_pool_executor::thread_pool_executor(pool_data* impl) noexcept
    : impl_(impl) {}

bool thread_pool_executor::running_in_this_thread() const noexcept {
    return false; // TODO
}

void thread_pool_executor::internal_execute(task_function&& t) const {
    // TODO: fake, but it would work for now
    global_executor(task{std::move(t), impl_->grp_});
}

bool operator==(const thread_pool_executor& l, const thread_pool_executor& r) noexcept {
    return l.impl_ == r.impl_;
}
bool operator!=(const thread_pool_executor& l, const thread_pool_executor& r) noexcept {
    return l.impl_ != r.impl_;
}

} // namespace detail

inline namespace v1 {

static_thread_pool::static_thread_pool(std::size_t num_threads)
    : impl_(std::make_unique<detail::pool_data>(num_threads)) {}

static_thread_pool::~static_thread_pool() = default;

void static_thread_pool::attach() {
    // TODO
}
void static_thread_pool::join() {
    // TODO
}
void static_thread_pool::stop() {
    // TODO
}
void static_thread_pool::wait() {
    // TODO
}
static_thread_pool::scheduler_type static_thread_pool::scheduler() noexcept {
    return detail::thread_pool_scheduler{impl_.get()};
}
static_thread_pool::executor_type static_thread_pool::executor() noexcept {
    return detail::thread_pool_executor{impl_.get()};
}

} // namespace v1

} // namespace std_execution
} // namespace concore