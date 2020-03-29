#pragma once

#include "task.hpp"
#include "executor_type.hpp"
#include "data/concurrent_queue.hpp"
#include "detail/utils.hpp"

#include <memory>

namespace concore {

inline namespace v1 {

//! Similar to a serializer but allows two types of tasks: READ and WRITE tasks.
//!
//! The READ tasks can be run in parallel with other READ tasks, but not with WRITE tasks.
//! The WRITE tasks cannot be run in parallel nether with READ nor with other WRITE tasks.
//!
//! Unlike `serializer` and `n_serializer` this is not an executor. Instead it provides two methods:
//! `reader()` and `writer()` that will return task executors. The two executors returned by these
//! two methods are, of course, related by the READ/WRITE incompatibility.
//!
//! This implementation slightly favors the WRITEs: if there are multiple pending WRITEs and
//! multiple pending READs, this will execute all the WRITEs before executing the READs. The
//! rationale is twofold:
//!     - it's expected that the number of WRITEs is somehow smaller than the number of READs
//!     (otherwise a simple serializer would probably work too)
//!     - it's expected that the READs would want to "read" the latest data published by the WRITEs
class rw_serializer {
    struct impl;
    //! Implementation detail shared by both reader and writer types
    std::shared_ptr<impl> impl_;

public:
    //! The type of the executor used for READ tasks.
    class reader_type {
        std::shared_ptr<impl> impl_;

    public:
        reader_type(std::shared_ptr<impl> impl);
        void operator()(task t);
    };

    //! The type of the executor used for WRITE tasks.
    class writer_type {
        std::shared_ptr<impl> impl_;

    public:
        writer_type(std::shared_ptr<impl> impl);
        void operator()(task t);
    };

    explicit rw_serializer(executor_t base_executor = {}, executor_t cont_executor = {});

    //! Returns an executor that will apply treat all the given tasks as READ tasks.
    reader_type reader() const { return reader_type(impl_); }
    //! Returns an executor that will apply treat all the given tasks as WRITE tasks.
    writer_type writer() const { return writer_type(impl_); }
};

} // namespace v1
} // namespace concore