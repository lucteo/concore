#include <concore/data/concurrent_dequeue.hpp>
#include <concore/data/concurrent_queue.hpp>
#include <concore/low_level/spin_backoff.hpp>
#include <concore/profiling.hpp>

#include "test_common/task_countdown.hpp"

#include <benchmark/benchmark.h>
#include <thread>
#include <functional>

//! Structure that is large enough for our purposes, when testing the speed
struct test_elem {
    test_elem() = default;
    ~test_elem() = default;
    explicit test_elem(int idx)
        : idx_(idx) {}

    test_elem(const test_elem& other) = default;
    test_elem& operator=(const test_elem& other) = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    test_elem(test_elem&&) = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    test_elem& operator=(test_elem&&) = default;

    int idx_{0};
    std::function<void()> t1_;
    std::function<void()> t2_;
};

bool operator==(const test_elem& lhs, int rhs) { return lhs.idx_ == rhs; }

int as_int(int x) { return x; }
int as_int(const test_elem& x) { return x.idx_; }

template <typename T>
struct simple_queue {
    using value_type = T;

    std::deque<T> elements_;
    std::mutex bottleneck_;

    void push_back(T&& elem) {
        std::lock_guard<std::mutex> lock{bottleneck_};
        elements_.push_back(std::forward<T>(elem));
    }
    void push_front(T&& elem) {
        std::lock_guard<std::mutex> lock{bottleneck_};
        elements_.push_front(std::forward<T>(elem));
    }
    bool try_pop_front(T& elem) {
        std::lock_guard<std::mutex> lock{bottleneck_};
        if (elements_.empty())
            return false;
        elem = std::move(elements_.front());
        elements_.pop_front();
        return true;
    }
    bool try_pop_back(T& elem) {
        std::lock_guard<std::mutex> lock{bottleneck_};
        if (elements_.empty())
            return false;
        elem = std::move(elements_.back());
        elements_.pop_back();
    }
    void unsafe_clear() { elements_.clear(); }
};

template <typename Q, typename T>
void push(Q& queue, T&& elem) {
    queue.push_back(std::forward<T>(elem));
}

template <typename T>
void push(concore::concurrent_queue<T>& queue, T&& elem) {
    queue.push(std::forward<T>(elem));
}

template <typename Q, typename T>
bool try_pop(Q& queue, T& elem) {
    return queue.try_pop_front(elem);
}

template <typename T>
bool try_pop(concore::concurrent_queue<T>& queue, T& elem) {
    return queue.try_pop(elem);
}

template <typename Q>
void clear(Q& queue) {
    using value_type = typename Q::value_type;
    value_type value;
    while (try_pop(queue, value))
        ;
}

template <typename Q>
static void test_push_back_latency(Q& queue, benchmark::State& state) {
    const int num_elements = state.range(0);

    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        clear(queue);

        auto start = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < num_elements; i++)
            push(queue, test_elem(i));

        auto end = std::chrono::high_resolution_clock::now();

        // Report the time
        auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
}
void BM_push_back_mtx_queue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    simple_queue<test_elem> queue;
    test_push_back_latency(queue, state);
}
void BM_push_back_concurrent_dequeue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    const int reserved = state.range(1);
    concore::concurrent_dequeue<test_elem> queue(reserved);
    test_push_back_latency(queue, state);
}
void BM_push_back_concurrent_queue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    const int reserved = state.range(1);
    concore::concurrent_queue<test_elem> queue(reserved);
    test_push_back_latency(queue, state);
}

template <typename Q>
static void test_pop_front_latency(Q& queue, benchmark::State& state) {
    const int num_elements = state.range(0);

    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        clear(queue);

        // Add the elements to the queue
        for (int i = 0; i < num_elements; i++)
            push(queue, test_elem(i));

        auto start = std::chrono::high_resolution_clock::now();

        test_elem value;
        for (int i = 0; i < num_elements; i++)
            try_pop(queue, value);

        auto end = std::chrono::high_resolution_clock::now();

        // Report the time
        auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
}
void BM_pop_front_mtx_queue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    simple_queue<test_elem> queue;
    test_pop_front_latency(queue, state);
}
void BM_pop_front_concurrent_dequeue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    const int reserved = state.range(1);
    concore::concurrent_dequeue<test_elem> queue(reserved);
    test_pop_front_latency(queue, state);
}
void BM_pop_front_concurrent_queue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    const int reserved = state.range(1);
    concore::concurrent_queue<test_elem> queue(reserved);
    test_pop_front_latency(queue, state);
}

template <typename Q>
static void test_push_pop(Q& queue, benchmark::State& state) {
    const int num_elements = state.range(0);
    using value_type = typename Q::value_type;

    std::thread pusher, popper;

    for (auto _ : state) {
        CONCORE_PROFILING_SCOPE_N("test iter");
        clear(queue);

        auto start = std::chrono::high_resolution_clock::now() - 1ms;
        auto end = start;

        // used for a synchronized start
        task_countdown barrier{2};

        auto push_work = [=, &queue, &start, &barrier]() {
            barrier.task_finished();
            barrier.wait_for_all();

            // Start measuring from this point
            start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < num_elements; i++) {
                push(queue, value_type(i));
            }
        };
        auto pop_work = [=, &queue, &end, &barrier]() {
            barrier.task_finished();
            barrier.wait_for_all();

            value_type value;
            while (true) {
                if (try_pop(queue, value)) {
                    if (value == num_elements - 1) {
                        end = std::chrono::high_resolution_clock::now();
                        return;
                    }

                    // Pause for a bit
                    concore::spin_backoff spinner;
                    for (int i = 0; i < 2; i++)
                        spinner.pause();
                }
            }
        };

        // Start the two threads, and wait for them to finish
        popper = std::thread(pop_work);
        pusher = std::thread(push_work);
        popper.join();
        pusher.join();

        // Report the time
        auto elapsed_seconds =
                std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        state.SetIterationTime(elapsed_seconds.count());
    }
}
void BM_push_pop_par_small_mtx_queue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    simple_queue<int> queue;
    test_push_pop(queue, state);
}
void BM_push_pop_par_small_concurrent_dequeue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    const int reserved = state.range(1);
    concore::concurrent_dequeue<int> queue(reserved);
    test_push_pop(queue, state);
}
void BM_push_pop_par_small_concurrent_queue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    const int reserved = state.range(1);
    concore::concurrent_queue<int> queue(reserved);
    test_push_pop(queue, state);
}

void BM_push_pop_par_large_mtx_queue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    simple_queue<test_elem> queue;
    test_push_pop(queue, state);
}
void BM_push_pop_par_large_concurrent_dequeue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    const int reserved = state.range(1);
    concore::concurrent_dequeue<test_elem> queue(reserved);
    test_push_pop(queue, state);
}
void BM_push_pop_par_large_concurrent_queue(benchmark::State& state) {
    CONCORE_PROFILING_FUNCTION();
    const int reserved = state.range(1);
    concore::concurrent_queue<test_elem> queue(reserved);
    test_push_pop(queue, state);
}

#define BENCHMARK_CASE1(fun)                                                                       \
    BENCHMARK(fun)->UseManualTime()->Unit(benchmark::kMicrosecond)->Args({10'000, 11'000});
#define BENCHMARK_CASE2(fun)                                                                       \
    BENCHMARK(fun)->UseManualTime()->Unit(benchmark::kMicrosecond)->Args({1'000, 11'000});
#define BENCHMARK_CASE3(fun)                                                                       \
    BENCHMARK(fun)->UseManualTime()->Unit(benchmark::kMicrosecond)->Args({10'000, 1024});
#define BENCHMARK_CASE4(fun)                                                                       \
    BENCHMARK(fun)->UseManualTime()->Unit(benchmark::kMicrosecond)->Args({10'000, 16});

BENCHMARK_CASE1(BM_push_back_mtx_queue);
BENCHMARK_CASE1(BM_push_back_concurrent_dequeue);
BENCHMARK_CASE1(BM_push_back_concurrent_queue);
BENCHMARK_CASE2(BM_push_back_mtx_queue);
BENCHMARK_CASE2(BM_push_back_concurrent_dequeue);
BENCHMARK_CASE2(BM_push_back_concurrent_queue);
BENCHMARK_CASE3(BM_push_back_mtx_queue);
BENCHMARK_CASE3(BM_push_back_concurrent_dequeue);
BENCHMARK_CASE3(BM_push_back_concurrent_queue);
BENCHMARK_CASE4(BM_push_back_mtx_queue);
BENCHMARK_CASE4(BM_push_back_concurrent_dequeue);
BENCHMARK_CASE4(BM_push_back_concurrent_queue);

BENCHMARK_CASE1(BM_pop_front_mtx_queue);
BENCHMARK_CASE1(BM_pop_front_concurrent_dequeue);
BENCHMARK_CASE1(BM_pop_front_concurrent_queue);

BENCHMARK_CASE1(BM_push_pop_par_small_mtx_queue);
BENCHMARK_CASE1(BM_push_pop_par_small_concurrent_dequeue);
BENCHMARK_CASE1(BM_push_pop_par_small_concurrent_queue);
BENCHMARK_CASE1(BM_push_pop_par_large_mtx_queue);
BENCHMARK_CASE1(BM_push_pop_par_large_concurrent_dequeue);
BENCHMARK_CASE1(BM_push_pop_par_large_concurrent_queue);

BENCHMARK_MAIN();
