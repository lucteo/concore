#pragma once

#include <concore/sender_algo/just.hpp>
#include <concore/sender_algo/just_error.hpp>
#include <concore/sender_algo/just_done.hpp>
#include <concore/_cpo/_cpo_schedule.hpp>
#include <concore/_cpo/_cpo_connect.hpp>
#include <concore/_cpo/_cpo_start.hpp>
#include <concore/_cpo/_cpo_set_value.hpp>
#include <concore/_cpo/_cpo_set_error.hpp>
#include <concore/_cpo/_cpo_set_done.hpp>

#include <functional>
#include <vector>

//! Scheduler that will send impulses on user's request.
//! One can obtain senders from this, connect them to receivers and start the operation states.
//! Until the scheduler is told to start the next operation, the actions in the operation states are
//! not executed. This is similar to a task scheduler, but it's single threaded. Doesn't have any
//! thread safety built in.
struct impulse_scheduler {
private:
    //! Command type that can store the action of firing up a sender
    using oper_command_t = std::function<void()>;
    using cmd_vec_t = std::vector<oper_command_t>;
    //! All the commands that we need to somehow
    std::shared_ptr<cmd_vec_t> all_commands_{};

    template <typename R>
    struct oper {
        cmd_vec_t* all_commands_;
        R receiver_;

        oper(cmd_vec_t* all_commands, R&& recv)
            : all_commands_(all_commands)
            , receiver_((R &&) recv) {}

        friend void tag_invoke(concore::start_t, oper& self) noexcept {
            // Enqueue another command to the list of all commands
            // The scheduler will start this, whenever start_next() is called
            self.all_commands_->emplace_back([recv = (R &&) self.receiver_]() {
                try {
                    concore::set_value((R &&) recv);
                } catch (...) {
                    concore::set_error((R &&) recv, std::current_exception());
                }
            });
        }
    };

    struct my_sender {
        cmd_vec_t* all_commands_;

        template <template <class...> class Tuple, template <class...> class Variant>
        using value_types = Variant<Tuple<>>;
        template <template <class...> class Variant>
        using error_types = Variant<std::exception_ptr>;
        static constexpr bool sends_done = true;

        template <typename R>
        friend oper<R> tag_invoke(concore::connect_t, my_sender self, R&& r) {
            return {self.all_commands_, (R &&) r};
        }
    };

public:
    impulse_scheduler()
        : all_commands_(std::make_shared<cmd_vec_t>(cmd_vec_t{})) {}
    ~impulse_scheduler() = default;

    //! Actually start the command from the last started operation_state
    void start_next() {
        if (!all_commands_->empty()) {
            // Pop one command from the queue
            auto cmd = std::move(all_commands_->front());
            all_commands_->erase(all_commands_->begin());
            // Execute the command, i.e., send an impulse to the connected sender
            cmd();
        }
    }

    friend my_sender tag_invoke(concore::schedule_t, const impulse_scheduler& self) {
        return my_sender{self.all_commands_.get()};
    }

    friend bool operator==(impulse_scheduler, impulse_scheduler) noexcept { return true; }
    friend bool operator!=(impulse_scheduler, impulse_scheduler) noexcept { return false; }
};

//! Scheduler that executes everything inline, i.e., on the same thread
struct inline_scheduler {
    friend auto tag_invoke(concore::schedule_t, inline_scheduler) { return concore::just(); }

    friend bool operator==(inline_scheduler, inline_scheduler) noexcept { return true; }
    friend bool operator!=(inline_scheduler, inline_scheduler) noexcept { return false; }
};

//! Scheduler that returns a sender that always completes with error.
template <typename E = std::exception_ptr>
struct error_scheduler {
    E err_;

    friend auto tag_invoke(concore::schedule_t, const error_scheduler& self) {
        return concore::just_error(self.err_);
    }

    friend bool operator==(error_scheduler, error_scheduler) noexcept { return true; }
    friend bool operator!=(error_scheduler, error_scheduler) noexcept { return false; }
};

//! Scheduler that returns a sender that always completes with cancellation.
struct done_scheduler {
    friend auto tag_invoke(concore::schedule_t, const done_scheduler& self) {
        return concore::just_done();
    }

    friend bool operator==(done_scheduler, done_scheduler) noexcept { return true; }
    friend bool operator!=(done_scheduler, done_scheduler) noexcept { return false; }
};
