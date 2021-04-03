#include <catch2/catch.hpp>
#include <concore/sender_algo/just.hpp>

struct expect_receiver_base {
    void set_done() noexcept {}
    void set_error(std::exception_ptr e) { std::rethrow_exception(e); }
};

template <typename T>
struct expect_receiver : expect_receiver_base {
    T val_;

    expect_receiver(T val)
        : val_(std::move(val)) {}

    //! Called whenever the sender completed the work with success
    void set_value(const T& val) { REQUIRE(val == val_); }
};

template <typename T>
expect_receiver<T> make_expect_receiver(T val) {
    expect_receiver<T> res = {std::move(val)};
    return res;
}

TEST_CASE("Simple test for just", "[sender_algo]") {
    concore::just(1).connect(make_expect_receiver(1)).start();
    concore::just(2).connect(make_expect_receiver(2)).start();
    concore::just(3).connect(make_expect_receiver(3)).start();

    concore::just("this").connect(make_expect_receiver(std::string("this"))).start();
    concore::just("that").connect(make_expect_receiver(std::string("that"))).start();
}
