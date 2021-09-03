/**
 * @file    as_scheduler.hpp
 * @brief   Definition of @ref concore::v1::as_scheduler "as_scheduler"
 *
 * @see     @ref concore::v1::as_scheduler "as_scheduler"
 */
#pragma once

#include <concore/as_sender.hpp>
#include <concore/_cpo/_cpo_schedule.hpp>

namespace concore {
inline namespace v1 {

/**
 * @brief   Wrapper that transforms an executor into a scheduler
 *
 * @tparam  E The type of the executor
 *
 * @details
 *
 * The executor should model `executor<>`.
 *
 * An instance of this class will be able to create one-shot senders that will execute on the
 * executor stored in the instance.
 *
 * @see scheduler, executor, as_sender
 */
template <typename E>
struct as_scheduler {
    //! Constructor
    explicit as_scheduler(E e) noexcept
        : ex_((E &&) e) {
#if CONCORE_CXX_HAS_CONCEPTS
        static_assert(executor<E>, "Type needs to match executor concept");
#endif
    }

    //! The schedule CPO that returns a sender which will execute on our executor.
    concore::as_sender<E> schedule() noexcept { return concore::as_sender<E>{ex_}; }

    friend inline bool operator==(as_scheduler lhs, as_scheduler rhs) { return lhs.ex_ == rhs.ex_; }
    friend inline bool operator!=(as_scheduler lhs, as_scheduler rhs) { return !(lhs == rhs); }

private:
    //! The wrapped executor
    E ex_;

    friend auto tag_invoke(schedule_t, const as_scheduler<E>& sched) {
        return concore::as_sender<E>{sched.ex_};
    }
};

} // namespace v1
} // namespace concore
