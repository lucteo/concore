#pragma once

#include <concore/task_group.hpp>
#include <concore/executor_type.hpp>

#include <chrono>

using namespace std::chrono_literals;

//! Wait for all the task in a group to complete.
//! Returns true if we all tasks are complete; return false if we timeout
inline bool bounded_wait(concore::task_group& grp, std::chrono::milliseconds timeout = 1000ms) {
    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + timeout;
    auto sleep_dur = timeout / 1000;
    while (std::chrono::high_resolution_clock::now() < end) {
        if (!grp.is_active())
            return true;
        std::this_thread::sleep_for(sleep_dur);
        sleep_dur = sleep_dur * 16 / 10;
    }
    return false;
}

//! Enqueue N tasks in the executor, and wait for them to be executed
inline bool enqueue_and_wait(concore::executor_t e, concore::task_function f, int num_tasks = 10) {
    auto grp = concore::task_group::create();
    for (int i = 0; i < num_tasks; i++)
        e(concore::task{f, grp});
    return bounded_wait(grp);
}
