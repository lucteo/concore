#pragma once

#include "rapidcheck_utils.hpp"
#include "concore/partition_hints.hpp"

inline rc::Gen<concore::partition_method> arb_partition_method() {
    using namespace rc;
    return gen::element(concore::partition_method::auto_partition,
            concore::partition_method::upfront_partition,
            concore::partition_method::iterative_partition,
            concore::partition_method::naive_partition);
}
DEFINE_RC_ARBITRARY(concore::partition_method, arb_partition_method())

namespace concore {
inline namespace v1 {
inline std::ostream& operator<<(std::ostream& os, partition_method method) {
    switch (method) {
    case partition_method::auto_partition:
        os << "auto_partition";
        break;
    case partition_method::upfront_partition:
        os << "upfront_partition";
        break;
    case partition_method::iterative_partition:
        os << "iterative_partition";
        break;
    case partition_method::naive_partition:
        os << "naive_partition";
        break;
    default:
        os << "unknown(" << static_cast<int>(method) << ")";
        break;
    }
    return os;
}

} // namespace v1
} // namespace concore

inline rc::Gen<concore::partition_hints> arb_partition_hints() {
    using namespace rc;
    return gen::exec([](concore::partition_method method) {
        concore::partition_hints hints;
        hints.method_ = method;
        hints.granularity_ = *gen::inRange(-1, 20);
        return hints;
    });
}
DEFINE_RC_ARBITRARY(concore::partition_hints, arb_partition_hints())

namespace concore {
inline namespace v1 {
inline std::ostream& operator<<(std::ostream& os, partition_hints hints) {
    os << "(" << hints.method_ << ", " << hints.granularity_ << ")";
    return os;
}
} // namespace v1
} // namespace concore