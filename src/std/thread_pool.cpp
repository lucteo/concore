#include <concore/std/thread_pool.hpp>
#include <concore/global_executor.hpp>
#include <concore/task_group.hpp>
#include <concore/spawn.hpp>
#include <concore/init.hpp>
#include <concore/detail/library_data.hpp>
#include <concore/detail/exec_context.hpp>

namespace concore {
namespace std_execution {
namespace detail {

init_data get_init_data_for_pool(size_t num_threads) {
    // Ensure the library was initialized
    concore::detail::get_exec_context();

    // Get the init_data object used, and change the number of worker threads
    init_data res = concore::detail::get_current_init_data();
    res.num_workers_ = static_cast<int>(num_threads);
    res.reserved_slots_ = res.num_workers_ * 2; // reasonable number of reserved slots
    return res;
}

struct pool_data {
    //! The execution context object that manages the pool
    concore::detail::exec_context* ctx_;
    //! The group we use to control the items that were added in this pool
    task_group grp_;

    explicit pool_data(size_t num_threads)
        : ctx_(new concore::detail::exec_context(get_init_data_for_pool(num_threads)))
        , grp_(task_group::create()) {}
    ~pool_data() { destroy(); }

    // Copy is disabled
    pool_data(const pool_data&) = delete;
    pool_data& operator=(const pool_data&) = delete;

    pool_data(pool_data&& rhs) noexcept
        : ctx_(rhs.ctx_)
        , grp_(std::move(rhs.grp_)) {
        rhs.ctx_ = nullptr;
    }
    pool_data& operator=(pool_data&& rhs) noexcept {
        destroy();
        ctx_ = rhs.ctx_;
        rhs.ctx_ = nullptr;
        grp_ = std::move(rhs.grp_);
        return *this;
    }

private:
    void destroy() {
        if (ctx_) {
            // Wait for all the tasks to complete
            grp_.cancel();
            wait(grp_);
            delete ctx_;
            ctx_ = nullptr;
        }
    }
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
    return &concore::detail::get_exec_context() == impl_->ctx_;
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
    return &concore::detail::get_exec_context() == impl_->ctx_;
}

void thread_pool_executor::internal_execute(task_function&& t) const {
    impl_->ctx_->enqueue(task{std::move(t), impl_->grp_});
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

void static_thread_pool::attach() { impl_->ctx_->attach_worker(); }
void static_thread_pool::join() { attach(); }
void static_thread_pool::stop() { impl_->grp_.cancel(); }
void static_thread_pool::wait() {
    // Wait for all the existing tasks to complete
    concore::wait(impl_->grp_);
    // Ensure that no more tasks are added to the pool
    impl_->grp_.cancel();
    // Wait for any tasks that were added in the meantime
    concore::wait(impl_->grp_);
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