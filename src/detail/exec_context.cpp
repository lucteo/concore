#include "concore/detail/exec_context.hpp"
#include "concore/detail/exec_context_if.hpp"
#include "concore/detail/utils.hpp"
#include "concore/detail/library_data.hpp"
#include "concore/init.hpp"
#include "concore/task_group.hpp"

namespace concore {
namespace detail {

//! TLS pointer to the worker data. This way, if we are called from a worker thread we can interact
//! with the worker thread data
thread_local worker_thread_data* g_worker_data{nullptr};

int get_num_threads(int config_num_threads) {
    if (config_num_threads > 0)
        return config_num_threads;
    int res = static_cast<int>(std::thread::hardware_concurrency());
    return res > 0 ? res : 4;
}

exec_context::exec_context(const init_data& config)
    : count_(get_num_threads(config.num_workers_))
    , reserved_slots_(config.reserved_slots_)
    , workers_data_(static_cast<size_t>(count_))
    , reserved_worker_slots_(static_cast<size_t>(reserved_slots_)) {
    CONCORE_PROFILING_INIT();
    CONCORE_PROFILING_FUNCTION();
    // Mark all the extra slots as being idle
    for (auto& w : reserved_worker_slots_)
        w.state_.store(worker_thread_data::idle);
    // Start the worker threads
    std::function<void()> worker_start_fun = config.worker_start_fun_;
    for (int i = 0; i < count_; i++) {
        workers_data_[i].thread_ = std::thread([this, i, worker_start_fun]() {
            // Call the worker start function, to perform user-defined actions on this thread
            if (worker_start_fun)
                worker_start_fun();
            // now actually run the logic for the worker thread
            worker_run(workers_data_[i]);
        });
    }
}
exec_context::~exec_context() {
    CONCORE_PROFILING_FUNCTION();
    // Set the flag to mark shut down, and wake all the threads
    done_ = true;
    for (auto& worker_data : workers_data_)
        worker_data.has_data_.signal();
    for (auto& worker_data : reserved_worker_slots_)
        worker_data.has_data_.signal();
    // Wait for all the threads to finish
    for (auto& worker_data : workers_data_)
        worker_data.thread_.join();
    // Wait for all the extra threads to exit this exec_context
    spin_backoff spinner;
    while (num_active_extra_slots_.load() > 0)
        spinner.pause();
}

void exec_context::enqueue(task&& t, task_priority prio) {
    CONCORE_PROFILING_FUNCTION();
    auto p = static_cast<int>(prio);
    assert(p < num_priorities);

    // Push the task in the global queue, corresponding to the given prio
    on_task_added();
    enqueued_tasks_[p].push(std::move(t));
    num_global_tasks_++;
    wakeup_workers();
}

void exec_context::spawn(task&& t, bool wake_workers) {
    CONCORE_PROFILING_FUNCTION();
    worker_thread_data* data = g_worker_data;

    // If no worker thread data is stored on the current thread, this is not a worker thread, so we
    // should enqueue the task
    if (!data) {
        enqueue<int(task_priority::normal)>(std::forward<task>(t));
        return;
    }

    // Add the task to the worker's queue
    on_task_added();
    data->local_tasks_.push(std::forward<task>(t));

    // Wake up the workers
    if (wake_workers)
        wakeup_workers();
}

void exec_context::busy_wait_on(task_group& grp) {
    worker_thread_data* data = g_worker_data;

    on_worker_active();

    using namespace std::chrono_literals;
    auto min_pause = 1us;
    auto max_pause = 10'000us;
    auto cur_pause = min_pause;
    while (true) {
        // Did we reach our goal?
        if (!grp.is_active())
            break;

        // Try to execute a task -- if we have a worker data
        if (data && try_extract_execute_task(*data)) {
            cur_pause = min_pause;
            continue;
        }

        // Nothing to execute, try pausing
        // Grow the pause each time, so that we don't wake too often
        std::this_thread::sleep_for(cur_pause);
        cur_pause = std::min(cur_pause * 16 / 10, max_pause);
    }

    on_worker_inactive();
}

worker_thread_data* exec_context::enter_worker() {
    // Check if we already have an attached worker; if yes, no need for a new one
    if (g_worker_data)
        return nullptr;

    // Ok, so this is called from an external thread. Try to occupy a free slot
    if (++num_active_extra_slots_ <= reserved_slots_) {
        for (auto& data : reserved_worker_slots_) {
            int old = worker_thread_data::idle;
            if (data.state_.compare_exchange_strong(old, worker_thread_data::running)) {
                // Found an empty slot; use it
                data.state_.store(worker_thread_data::running, std::memory_order_relaxed);
                // It's important for us to store this in TLS
                g_worker_data = &data;
                set_context_in_current_thread(this);
                return &data;
            }
        }
    }
    // Couldn't find any empty slot; decrement the counter back and return
    num_active_extra_slots_--;

    return nullptr;
}

void exec_context::exit_worker(worker_thread_data* worker_data) {
    if (worker_data) {
        assert(worker_data->state_.load() == worker_thread_data::running);
        worker_data->state_.store(worker_thread_data::idle, std::memory_order_release);
        num_active_extra_slots_--;
        // Make sure to clear the TLS storage
        g_worker_data = nullptr;
        set_context_in_current_thread(nullptr);
    }
}

void exec_context::attach_worker() {
    CONCORE_PROFILING_FUNCTION();
    auto* wd = enter_worker();
    if (!wd)
        throw std::runtime_error("cannot attach; thread already a worker thread");

    // Run until we are done
    worker_run(*wd);
    exit_worker(wd);
}


void exec_context::worker_run(worker_thread_data& worker_data) {
    CONCORE_PROFILING_SETTHREADNAME("concore_worker");
    g_worker_data = &worker_data;
    set_context_in_current_thread(this);
    on_worker_active();
    while (true) {
        if (done_)
            return;

        if (!try_extract_execute_task(worker_data)) {
            try_sleep(worker_data);
        }
    }
    on_worker_inactive();
}

bool exec_context::try_extract_execute_task(worker_thread_data& worker_data) {
    CONCORE_PROFILING_FUNCTION();
    task t;

    // Attempt to consume tasks from the local queue
    if (worker_data.local_tasks_.try_pop(t)) {
        execute_task(t);
        return true;
    }

    // Try taking tasks from the global queue
    for (auto& q : enqueued_tasks_) {
        if (q.try_pop(t)) {
            num_global_tasks_--;
            execute_task(t);
            return true;
        }
    }

    // Try stealing a task from another worker
    for (int i = 0; i < count_; i++) {
        if (&workers_data_[i] != &worker_data) {
            if (workers_data_[i].local_tasks_.try_steal(t)) {
                execute_task(t);
                return true;
            }
        }
    }

    // If we have extra workers joining our task system, try stealing tasks from them too
    if (num_active_extra_slots_.load(std::memory_order_acquire) > 0) {
        for (auto& wd : reserved_worker_slots_) {
            if (wd.local_tasks_.try_steal(t)) {
                execute_task(t);
                return true;
            }
        }
    }

    return false;
}

void exec_context::try_sleep(worker_thread_data& worker_data) {
    on_worker_inactive();
    worker_data.state_.store(worker_thread_data::waiting);
    if (before_sleep(worker_data)) {
        worker_data.has_data_.wait();
    }
    on_worker_active();
    worker_data.state_.store(worker_thread_data::running);
}

bool exec_context::before_sleep(worker_thread_data& worker_data) {
    CONCORE_PROFILING_FUNCTION();

    worker_data.state_.store(worker_thread_data::waiting);

    // Spin for a bit, in the hope that new tasks are added to the system
    // We hope that we avoid going to sleep just to be woken up immediately
    spin_backoff spinner;
    constexpr int new_active_wait_iterations = 8;
    for (int i = 0; i < new_active_wait_iterations; i++) {
        if (num_global_tasks_.load() > 0 || done_)
            return false;
        spinner.pause();
    }

    // Ok, so now we have to go to sleep
    int old = worker_thread_data::waiting;
    if (!worker_data.state_.compare_exchange_strong(old, worker_thread_data::idle))
        return false; // somebody prevented us to go to sleep

    return true;
}

void exec_context::wakeup_workers() {
    CONCORE_PROFILING_FUNCTION();

    // First try to wake up any worker that is in waiting state
    int num_idle = 0;
    for (int i = 0; i < count_; i++) {
        int old = worker_thread_data::waiting;
        if (workers_data_[i].state_.compare_exchange_strong(old, worker_thread_data::running)) {
            // Put a worker from waiting to running. That should be enough
            return;
        }

        if (old == worker_thread_data::idle)
            num_idle++;
    }

    // Also try to wake up additional threads in in waiting state
    int num_other_idle = 0;
    for (int i = 0; i < reserved_slots_; i++) {
        int old = worker_thread_data::waiting;
        if (reserved_worker_slots_[i].state_.compare_exchange_strong(old, worker_thread_data::running)) {
            // Put a worker from waiting to running. That should be enough
            return;
        }

        if (old == worker_thread_data::idle)
            num_other_idle++;
    }

    // If we are here, it means that all our workers are either running or idle.
    // If a worker just switched from running to waiting, it should should be picking up the new
    // task. But we should still wake up one idle thread.
    if (num_idle > 0) {
        for (int i = 0; i < count_; i++) {
            int old = worker_thread_data::idle;
            if (workers_data_[i].state_.compare_exchange_strong(old, worker_thread_data::running)) {
                // CONCORE_PROFILING_SCOPE_N("waking")
                // CONCORE_PROFILING_SET_TEXT_FMT(32, "%d", i);
                workers_data_[i].has_data_.signal();
                return;
            }
        }
    }

    // Also try to wake up additional workers that are idle
    if (num_other_idle > 0) {
        for (int i = 0; i < reserved_slots_; i++) {
            int old = worker_thread_data::idle;
            if (reserved_worker_slots_[i].state_.compare_exchange_strong(old, worker_thread_data::running)) {
                // CONCORE_PROFILING_SCOPE_N("waking")
                // CONCORE_PROFILING_SET_TEXT_FMT(32, "%d", i);
                reserved_worker_slots_[i].has_data_.signal();
                return;
            }
        }
    }

    // If we are here, it means that all workers are woken up.
}

void exec_context::execute_task(task& t) const {
    CONCORE_PROFILING_FUNCTION();

    detail::execute_task(t);
    on_task_removed();
}

void exec_context::on_worker_active() const {
#if CONCORE_ENABLE_PROFILING
    int val = num_active_workers_++;
    CONCORE_PROFILING_PLOT("# concore active workers", int64_t(val));
    CONCORE_PROFILING_PLOT("# concore active workers", int64_t(val + 1));
#else
    num_active_workers_++;
#endif
}

void exec_context::on_worker_inactive() const {
#if CONCORE_ENABLE_PROFILING
    int val = num_active_workers_--;
    CONCORE_PROFILING_PLOT("# concore active workers", int64_t(val));
    CONCORE_PROFILING_PLOT("# concore active workers", int64_t(val - 1));
#else
    num_active_workers_--;
#endif
}

void exec_context::on_task_added() const {
#if CONCORE_ENABLE_PROFILING
    int val = num_tasks_++;
    CONCORE_PROFILING_PLOT("# concore sys tasks", int64_t(val));
    CONCORE_PROFILING_PLOT("# concore sys tasks", int64_t(val + 1));
#else
    num_tasks_++;
#endif
}

void exec_context::on_task_removed() const {
#if CONCORE_ENABLE_PROFILING
    int val = num_tasks_--;
    CONCORE_PROFILING_PLOT("# concore sys tasks", int64_t(val));
    CONCORE_PROFILING_PLOT("# concore sys tasks", int64_t(val - 1));
#else
    num_tasks_--;
#endif
}

void do_enqueue(exec_context& ctx, task&& t, task_priority prio) {
    ctx.enqueue(std::move(t), prio);
}
void do_spawn(exec_context& ctx, task&& t, bool wake_workers) {
    ctx.spawn(std::move(t), wake_workers);
}

void busy_wait_on(exec_context& ctx, task_group& grp) { ctx.busy_wait_on(grp); }
worker_thread_data* enter_worker(exec_context& ctx) { return ctx.enter_worker(); }
void exit_worker(exec_context& ctx, worker_thread_data* worker_data) {
    ctx.exit_worker(worker_data);
}

int num_worker_threads(const exec_context& ctx) { return ctx.num_worker_threads(); }
bool is_active(const exec_context& ctx) { return ctx.is_active(); }
int num_active_tasks(const exec_context& ctx) { return ctx.num_active_tasks(); }

} // namespace detail
} // namespace concore