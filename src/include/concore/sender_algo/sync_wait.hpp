/**
 * @file    sync_wait.hpp
 * @brief   Definition of @ref concore::v1::sync_wait "sync_wait"
 *
 * @see     @ref concore::v1::sync_wait "sync_wait"
 */
#pragma once

#include <concore/_cpo/_cpo_submit.hpp>
#include <concore/_concepts/_concepts_sender.hpp>
#include <concore/detail/sender_helpers.hpp>

#include <mutex>
#include <condition_variable>

namespace concore {

namespace detail {

template <typename Res, typename Sender>
struct sync_wait_data {
    Res res_{};
    std::exception_ptr eptr_{};
    int readyMode_{0}; // 0 == waiting, 1 == value set, 2 == exception
    std::mutex bottleneck_{};
    std::condition_variable cv_{};

    void notify(int type) {
        {
            std::unique_lock<std::mutex> lock{bottleneck_};
            readyMode_ = type;
        }
        cv_.notify_one();
    }
    int wait() {
        {
            std::unique_lock<std::mutex> lock{bottleneck_};
            cv_.wait(lock, [this] { return readyMode_ > 0; });
        }
        return readyMode_;
    }
};

template <typename Res, typename Sender>
struct sync_wait_receiver {
    using data_t = sync_wait_data<Res, Sender>;
    data_t* data_;

    explicit sync_wait_receiver(data_t* d)
        : data_(d) {}

    friend void tag_invoke(set_value_t, sync_wait_receiver&& self, Res res) noexcept {
        try {
            self.data_->res_ = (Res &&) res;
            self.data_->notify(1);
        } catch (...) {
            self.data_->eptr_ = std::current_exception(); // NOLINT
            self.data_->notify(2);
        }
    }
    friend void tag_invoke(set_done_t, sync_wait_receiver&& self) noexcept { std::terminate(); }
    friend void tag_invoke(
            set_error_t, sync_wait_receiver&& self, std::exception_ptr eptr) noexcept {
        self.data_->eptr_ = eptr;
        self.data_->notify(2);
    }
};

template <typename Res, CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
Res sync_wait_impl(Sender&& s) {
    sync_wait_data<Res, Sender> data;
    // Connect the sender and the receiver and submit the work
    concore::submit((Sender &&) s, sync_wait_receiver<Res, Sender>{&data});
    // Wait for the result and interpret it
    int type = data.wait();
    if (type == 1)
        return std::move(data.res_);
    else
        std::rethrow_exception(data.eptr_);
}

struct sync_wait_create_fun {
    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) const& {
        static_assert(typed_sender<Sender>, "Given object is not a `typed_sender`");
        using SenderPlain = remove_cvref_t<Sender>;
        using Res = remove_cvref_t<sender_single_return_type<SenderPlain>>;
        return sync_wait_impl<Res, Sender>((Sender &&) sender);
    }
};

template <typename Res>
struct sync_wait_r_create_fun {
    template <CONCORE_CONCEPT_OR_TYPENAME(sender) Sender>
    auto operator()(Sender&& sender) const& {
        static_assert(typed_sender<Sender>, "Given object is not a `typed_sender`");
        return sync_wait_impl<Res, remove_cvref_t<Sender>>((Sender &&) sender);
    }
};

} // namespace detail

inline namespace v1 {

template <CONCORE_CONCEPT_OR_TYPENAME(typed_sender) Sender>
inline auto sync_wait(Sender&& s) {
    return detail::sync_wait_create_fun{}((Sender &&) s);
}

inline auto sync_wait() { return detail::make_sender_algo_wrapper(detail::sync_wait_create_fun{}); }

template <typename Res, CONCORE_CONCEPT_OR_TYPENAME(typed_sender) Sender>
inline auto sync_wait_r(Sender&& s) {
    return detail::sync_wait_r_create_fun<Res>{}((Sender &&) s);
}

template <typename Res>
auto sync_wait_r() {
    return detail::make_sender_algo_wrapper(detail::sync_wait_r_create_fun<Res>{});
}

} // namespace v1
} // namespace concore
