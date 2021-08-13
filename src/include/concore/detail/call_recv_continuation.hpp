#pragma once

#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>
#include <concore/task_cancelled.hpp>

#include <exception>

namespace concore {
namespace detail {

template <typename Recv, typename... ValT> // zero or one set_value parameters
void call_recv_on_continuation(std::exception_ptr ex, Recv r, ValT... vals) noexcept {
    if (ex) {
        try {
            std::rethrow_exception(ex);
        } catch (const task_cancelled&) {
            concore::set_done((Recv &&) r);
        } catch (...) {
            concore::set_error((Recv &&) r, std::move(ex));
        }
    } else {
        try {
            concore::set_value((Recv &&) r, (ValT &&) vals...);
        } catch (...) {
            concore::set_error((Recv &&) r, std::current_exception());
        }
    }
}

template <typename Recv, typename ValT>
struct call_recv_continuation {
    Recv recv_;
    ValT val_;

    void operator()(std::exception_ptr ex) noexcept {
        call_recv_on_continuation(ex, (Recv &&) recv_, (ValT &&) val_);
    }
};

template <typename Recv>
struct call_recv_continuation<Recv, void> {
    Recv recv_;

    void operator()(std::exception_ptr ex) noexcept {
        call_recv_on_continuation(ex, (Recv &&) recv_);
    }
};

template <typename Recv, typename ValT>
struct call_prev_and_recv_continuation {
    task_continuation_function prev_;
    Recv recv_;
    ValT val_;

    void operator()(std::exception_ptr ex) noexcept {
        prev_(ex);
        call_recv_on_continuation(ex, (Recv &&) recv_, (ValT &&) val_);
    }
};

template <typename Recv>
struct call_prev_and_recv_continuation<Recv, void> {
    task_continuation_function prev_;
    Recv recv_;

    void operator()(std::exception_ptr ex) noexcept {
        prev_(ex);
        call_recv_on_continuation(ex, (Recv &&) recv_);
    }
};

// template <typename Recv>
// void call_recv_from_cont(Recv&& r, std::exception_ptr ex) {
//     if (ex) {
//         try {
//             std::rethrow_exception(ex);
//         } catch (const task_cancelled&) {
//             concore::set_done((Recv &&) r);
//         } catch (...) {
//             concore::set_error((Recv &&) r, std::move(ex));
//         }
//     } else
//         concore::set_value((Recv &&) r);
// }
//
// template <typename Recv>
// void set_recv_continuation(task& t, Recv&& r) {
//     auto inner_cont = t.get_continuation();
//     if (inner_cont) {
//         auto cont = [inner_cont, r = (Recv &&) r](std::exception_ptr ex) {
//             inner_cont(ex);
//             call_recv_from_cont((Recv &&) r, ex);
//         };
//         t.set_continuation(std::move(cont));
//     } else {
//         auto cont = [r = (Recv &&) r](
//                             std::exception_ptr ex) { call_recv_from_cont((Recv &&) r, ex); };
//         t.set_continuation(std::move(cont));
//     }
// }

} // namespace detail
} // namespace concore