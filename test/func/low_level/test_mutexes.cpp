#include <catch2/catch.hpp>
#include <concore/low_level/spin_mutex.hpp>
#include <concore/low_level/shared_spin_mutex.hpp>
#include <concore/profiling.hpp>

#include <thread>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ctime>

using namespace std::chrono_literals;

template <typename Mtx>
void test_repeated_locks(Mtx& mutex) {
    constexpr int repetion_count = 100;
    int count = 0;
    for (int i = 0; i < repetion_count; i++) {
        mutex.lock();
        count++;
        mutex.unlock();
    }
    REQUIRE(count == repetion_count);
}

TEST_CASE("repeated lock/unlocks on spin_mutex") {
    CONCORE_PROFILING_FUNCTION();
    concore::spin_mutex mutex;
    test_repeated_locks(mutex);
}
TEST_CASE("repeated lock/unlocks on shared_spin_mutex") {
    CONCORE_PROFILING_FUNCTION();
    concore::shared_spin_mutex mutex;
    test_repeated_locks(mutex);
}

template <typename Mtx>
void test_exclusive_access_between_threads(Mtx& mutex, bool try_lock = false) {
    // the given semaphore is assumed to be notified once (and only once)

    constexpr int repetion_count = 100;
    constexpr int num_threads = 8;

    // Variable that will be continuously incremented by threads
    int counter = 0;

    std::vector<std::thread> threads{num_threads};
    for (int i = 0; i < num_threads; i++) {
        threads[i] = std::thread([&counter, &mutex, try_lock]() {
            for (int i = 0; i < repetion_count; i++) {
                // take the exclusive access
                if (try_lock) {
                    while (!mutex.try_lock())
                        ;
                } else
                    mutex.lock();
                // increment the counter
                counter++;
                // release the exclusive access
                mutex.unlock();
            }
        });
    }

    // Wait for all the threads to finish
    for (int i = 0; i < num_threads; i++)
        threads[i].join();

    // Check that we don't have data races
    REQUIRE(counter == num_threads * repetion_count);
}

TEST_CASE("spin_mutex can be used to synchronize access") {
    CONCORE_PROFILING_FUNCTION();
    concore::spin_mutex mutex;
    test_exclusive_access_between_threads(mutex);
    SECTION("using try_lock") {
        concore::spin_mutex mutex2;
        test_exclusive_access_between_threads(mutex2, true);
    }
}

TEST_CASE("shared_spin_mutex can be used to synchronize access") {
    CONCORE_PROFILING_FUNCTION();
    concore::shared_spin_mutex mutex;
    test_exclusive_access_between_threads(mutex);
    SECTION("using try_lock") {
        concore::shared_spin_mutex mutex2;
        test_exclusive_access_between_threads(mutex2, true);
    }
}

TEST_CASE("shared_spin_mutex: multiple readers can acquire the mutex at the same time") {
    CONCORE_PROFILING_FUNCTION();

    concore::shared_spin_mutex mutex;

    constexpr int num_readers = 10;

    // Take the shared lock serveral times
    for (int i = 0; i < num_readers; i++)
        mutex.lock_shared();
    REQUIRE(true); // If we didn't block so far, the test is ok

    // At this point we cannot take an exclusive lock
    REQUIRE(!mutex.try_lock());

    // Unlock now all the readers
    for (int i = 0; i < num_readers; i++)
        mutex.unlock_shared();

    // At this point, there are no readers; we should be able to get exclusive access
    {
        std::lock_guard<concore::shared_spin_mutex> lock{mutex};
        REQUIRE(true); // Not blocking at the previous line is a form of success
    }

    // Now try again with try_lock_shared()
    for (int i = 0; i < num_readers; i++)
        REQUIRE(mutex.try_lock_shared());

    // At this point we can't take an exclusive lock
    REQUIRE(!mutex.try_lock());
}

TEST_CASE("shared_spin_mutex: if we have a writer, we can't acquire any reader") {
    CONCORE_PROFILING_FUNCTION();

    concore::shared_spin_mutex mutex;

    {
        // Acquire the WRITE ownership
        std::lock_guard<concore::shared_spin_mutex> lock{mutex};

        // An attempt to acquire readers will fail
        REQUIRE(!mutex.try_lock_shared());

        // But so does an attempt to acquire the writer
        REQUIRE(!mutex.try_lock());
    }

    // Now we can obtain a reader
    REQUIRE(mutex.try_lock_shared());
    mutex.unlock_shared();
}
