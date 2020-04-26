#include <catch2/catch.hpp>
#include <concore/low_level/semaphore.hpp>

#include <thread>
#include <atomic>
#include <chrono>

using namespace std::chrono_literals;

template <typename Sem>
void test_signal_and_wait(Sem& sem) {
    constexpr int repetion_count = 100;
    int count = 0;
    for (int i = 0; i < repetion_count; i++) {
        sem.signal();
        count++;
        sem.wait();
    }
    REQUIRE(count == repetion_count);
}

TEST_CASE("semaphore wait doesn't block after signal") {
    CONCORE_PROFILING_FUNCTION();
    concore::semaphore sem;
    test_signal_and_wait(sem);
}
TEST_CASE("binary_semaphore wait doesn't block after signal") {
    CONCORE_PROFILING_FUNCTION();
    concore::binary_semaphore sem;
    test_signal_and_wait(sem);
}

template <typename Sem>
void test_exclusive_access(Sem& sem) {
    // the given semaphore is assumed to be notified once (and only once)

    constexpr int repetion_count = 100;
    constexpr int num_threads = 8;

    // Variable that will be continuously incremented by threads
    int counter = 0;

    std::vector<std::thread> threads{num_threads};
    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread([&counter, &sem]() {
            for (int i = 0; i < repetion_count; i++) {
                // take the exclusive access
                sem.wait();
                // increment the counter
                counter++;
                // release the exclusive access
                sem.signal();
            }
        });
    }

    // Wait for all the threads to finish
    for (int i = 0; i < num_threads; i++)
        threads[i].join();

    // Check that we don't have data races
    REQUIRE(counter == num_threads * repetion_count);
}

TEST_CASE("semaphore can be used to synchronize access") {
    CONCORE_PROFILING_FUNCTION();
    concore::semaphore sem;
    sem.signal();
    test_exclusive_access(sem);

    // Also test with a semaphore with initial count
    concore::semaphore sem2{1};
    test_exclusive_access(sem2);
}

TEST_CASE("binary_semaphore can be used to synchronize access") {
    CONCORE_PROFILING_FUNCTION();
    concore::binary_semaphore sem;
    sem.signal();
    test_exclusive_access(sem);
}

TEST_CASE("semaphore can be accessed from multiple threads") {
    CONCORE_PROFILING_FUNCTION();

    constexpr int allowed_conc_access = 5;
    constexpr int num_threads = 10;

    concore::semaphore sem{allowed_conc_access};
    std::atomic<int> num_entries{0};

    std::vector<std::thread> threads{num_threads};
    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread([&sem, &num_entries]() {
            // Enter protected area
            sem.wait();
            // We are in; mark this
            num_entries++;
        });
    }

    // Wait a bit for the threads to start
    std::this_thread::sleep_for(5ms);

    // We should have exactly allowed_conc_access threads pass the wait() barrier
    REQUIRE(num_entries.load() == allowed_conc_access);

    // Unblock the rest of the threads by signaling the semaphore
    for (int i = allowed_conc_access; i < num_threads; i++)
        sem.signal();

    // Wait for all the threads to finish
    // They should all be unblocked, and able to join
    for (int i = 0; i < num_threads; i++)
        threads[i].join();

    // Check that we don't have data races
    REQUIRE(num_entries.load() == num_threads);
}
