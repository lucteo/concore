#pragma once

namespace concore {
namespace detail {

/**
 * @brief Helper class that defines the types needed in a sender
 *
 * @tparam SendsDone = false Indicates whether this sender can send the "done" signal
 *
 * @details
 *
 * Used to simplify the definition of senders.
 */
template <bool SendsDone = false>
struct sender_types_base {
    //! The value types that defines the values that this sender sends to receivers
    template <template <typename...> class Tuple, template <typename...> class Variant>
    using value_types = Variant<Tuple<>>;

    //! The type of error that this sender sends to receiver
    template <template <typename...> class Variant>
    using error_types = Variant<std::exception_ptr>;

    //! Indicates whether this sender can send "done" signal
    static constexpr bool sends_done = SendsDone;
};
} // namespace detail
} // namespace concore