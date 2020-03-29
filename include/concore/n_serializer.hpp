#pragma once

#include "task.hpp"
#include "executor_type.hpp"

#include <memory>

namespace concore {

inline namespace v1 {

//! Executor type that allows maximum N tasks to be executed at a given time.
//!
//! This will be constructed on top of an existing executor, and can be considered an executor
//! itself.
//!
//! Guarantees:
//! - no more than N task are executed at once
//! - if N==1, behaves like the `serializer` class
class n_serializer : public std::enable_shared_from_this<n_serializer> {
public:
    explicit n_serializer(int N, executor_t base_executor = {}, executor_t cont_executor = {});

    //! The call operator that makes this an executor.
    //! Enqueues the given task in the base serializer, making sure that we don't execute too many
    //! tasks at the given time in parallel.
    void operator()(task t);

private:
    struct impl;
    //! The implementation object of this n_serializer.
    //! We need this to be shared pointer for lifetime issue, but also to be able to copy the
    //! serializer easily.
    std::shared_ptr<impl> impl_;
};

} // namespace v1
} // namespace concore