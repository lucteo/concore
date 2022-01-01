#pragma once

#include <concore/execution.hpp>

#if CONCORE_USE_CXX2020 && CONCORE_CPP_VERSION >= 20

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

        friend impulse_scheduler tag_invoke(
                concore::get_completion_scheduler_t<concore::set_value_t>, my_sender) {
            return {};
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
    template <typename R>
    struct oper {
        R recv_;
        friend void tag_invoke(concore::start_t, oper& self) noexcept {
            try {
                concore::set_value((R &&) self.recv_);
            } catch (...) {
                concore::set_error((R &&) self.recv_, std::current_exception());
            }
        }
    };

    struct my_sender {
        template <template <class...> class Tuple, template <class...> class Variant>
        using value_types = Variant<Tuple<>>;
        template <template <class...> class Variant>
        using error_types = Variant<std::exception_ptr>;
        static constexpr bool sends_done = false;

        template <typename R>
        friend oper<R> tag_invoke(concore::connect_t, my_sender self, R&& r) {
            return {(R &&) r};
        }

        friend inline_scheduler tag_invoke(
                concore::get_completion_scheduler_t<concore::set_value_t>, my_sender) {
            return {};
        }
    };

    friend my_sender tag_invoke(concore::schedule_t, inline_scheduler) { return {}; }

    friend bool operator==(inline_scheduler, inline_scheduler) noexcept { return true; }
    friend bool operator!=(inline_scheduler, inline_scheduler) noexcept { return false; }
};

//! Scheduler that returns a sender that always completes with error.
template <typename E = std::exception_ptr>
struct error_scheduler {
    template <typename R>
    struct oper {
        R receiver_;
        E err_;

        friend void tag_invoke(concore::start_t, oper& self) noexcept {
            concore::set_error((R &&) self.recv_, self.err_);
        }
    };

    struct my_sender {
        E err_;

        template <template <class...> class Tuple, template <class...> class Variant>
        using value_types = Variant<Tuple<>>;
        template <template <class...> class Variant>
        using error_types = Variant<E>;
        static constexpr bool sends_done = false;

        template <typename R>
        friend oper<R> tag_invoke(concore::connect_t, my_sender self, R&& r) {
            return {(R &&) r, (E &&) self.err_};
        }

        friend error_scheduler tag_invoke(
                concore::get_completion_scheduler_t<concore::set_value_t>, my_sender) {
            return {};
        }
    };

    E err_;

    friend my_sender tag_invoke(concore::schedule_t, error_scheduler self) {
        return {(E &&) self.err_};
    }

    friend bool operator==(error_scheduler, error_scheduler) noexcept { return true; }
    friend bool operator!=(error_scheduler, error_scheduler) noexcept { return false; }
};

//! Scheduler that returns a sender that always completes with cancellation.
struct done_scheduler {
    template <typename R>
    struct oper {
        R receiver_;
        friend void tag_invoke(concore::start_t, oper& self) noexcept {
            concore::set_error((R &&) self.recv_, self.err_);
        }
    };

    struct my_sender {
        template <template <class...> class Tuple, template <class...> class Variant>
        using value_types = Variant<Tuple<>>;
        template <template <class...> class Variant>
        using error_types = Variant<>;
        static constexpr bool sends_done = true;

        template <typename R>
        friend oper<R> tag_invoke(concore::connect_t, my_sender self, R&& r) {
            return {(R &&) r};
        }

        template <typename CPO>
        friend done_scheduler tag_invoke(concore::get_completion_scheduler_t<CPO>, my_sender) {
            return {};
        }
    };

    friend my_sender tag_invoke(concore::schedule_t, done_scheduler) { return {}; }

    friend bool operator==(done_scheduler, done_scheduler) noexcept { return true; }
    friend bool operator!=(done_scheduler, done_scheduler) noexcept { return false; }
};

#endif
