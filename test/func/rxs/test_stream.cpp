#include <catch2/catch.hpp>

#include <concore/rxs/stream.hpp>

#include <string>

template <typename S, typename R>
void ensure_stream() {
    static_assert(concore::rxs::stream<S>, "Given type is not a stream");
    static_assert(std::is_same_v<typename S::value_type, R>, "Invalid value_type in stream");
}

template <typename S>
void ensure_not_stream() {
    static_assert(!concore::rxs::stream<S>, "Given type is a stream, when we expect it not to be");
}

template <typename R>
struct simple_stream {
    using value_type = R;

    template <typename Recv>
    void start_with(Recv recv) noexcept {
        recv.set_done();
    }
};

struct bad_stream1 {
    template <typename Recv>
    void start_with(Recv recv) noexcept {
        recv.set_done();
    }
};

struct bad_stream2 {
    using value_type = int;
};

struct stream_one_val {
    using value_type = int;

    template <typename Recv>
    void start_with(Recv r) {
        r.set_value(10);
        ((Recv &&) r).set_done();
    }
};

struct stream_with_tag_invoke {
    using value_type = int;
};
template <typename Recv>
void tag_invoke(concore::rxs::start_with_t, stream_with_tag_invoke, Recv r) {
    r.set_value(11);
    r.set_done();
}

struct stream_with_outer_fun {
    using value_type = int;
};

template <typename Recv>
void start_with(stream_with_outer_fun, Recv r) {
    r.set_value(12);
    r.set_done();
}

struct test_int_receiver {
    int* res;
    bool* done_called;
    void set_value(int v) { *res = v; }
    void set_done() noexcept { *done_called = true; }
    void set_error(std::exception) noexcept { FAIL("set_error() called"); }
};

TEST_CASE("a simple type that matches the stream concept", "[stream]") {
    // Check that some simple stream objects match the concept
    ensure_stream<simple_stream<void>, void>();
    ensure_stream<simple_stream<int>, int>();
    ensure_stream<simple_stream<std::string>, std::string>();

    // Check a few classes that do not match the concept
    ensure_not_stream<bad_stream1>();
    ensure_not_stream<bad_stream2>();
}

TEST_CASE("some types do not match stream concept", "[stream]") {
    ensure_not_stream<bad_stream1>();
    ensure_not_stream<bad_stream2>();
}

TEST_CASE("stream using inner method CPO", "[stream]") {
    ensure_stream<stream_one_val, int>();

    int res{0};
    bool done_called{false};
    concore::rxs::start_with(stream_one_val{}, test_int_receiver{&res, &done_called});
    REQUIRE(res == 10);
    REQUIRE(done_called);
}
TEST_CASE("stream using tag_invoke CPO", "[stream]") {
    ensure_stream<stream_with_tag_invoke, int>();

    int res{0};
    bool done_called{false};
    concore::rxs::start_with(stream_with_tag_invoke{}, test_int_receiver{&res, &done_called});
    REQUIRE(res == 11);
    REQUIRE(done_called);
}
TEST_CASE("stream using outer function CPO", "[stream]") {
    ensure_stream<stream_with_outer_fun, int>();

    int res{0};
    bool done_called{false};
    concore::rxs::start_with(stream_with_outer_fun{}, test_int_receiver{&res, &done_called});
    REQUIRE(res == 12);
    REQUIRE(done_called);
}
