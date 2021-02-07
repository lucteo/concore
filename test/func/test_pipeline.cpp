#include <catch2/catch.hpp>
#include <concore/pipeline.hpp>
#include <concore/serializer.hpp>
#include <concore/spawn.hpp>
#include <concore/delegating_executor.hpp>

#include "rapidcheck_utils.hpp"
#include "test_common/task_utils.hpp"

#include <thread>
#include <atomic>
#include <chrono>
#include <array>

using namespace std::chrono_literals;

TEST_CASE("simple pipeline", "[pipeline]") {
    constexpr int num_items = 50;
    std::array<int, num_items> items{};
    items.fill(0);

    // Construct the pipeline
    // clang-format off
    auto my_pipeline = concore::pipeline_builder<int*>(num_items)
        | concore::stage_ordering::concurrent
        | [](int* data) { REQUIRE((*data)++ == 0); }
        | [](int* data) { REQUIRE((*data)++ == 1); }
        | [](int* data) { REQUIRE((*data)++ == 2); }
        | [](int* data) { REQUIRE((*data)++ == 3); }
        | concore::pipeline_end;
    // clang-format on

    // Push items through the pipeline
    for (int i = 0; i < num_items; i++)
        my_pipeline.push(&items[i]);

    // Wait for all the tasks to complete
    REQUIRE(bounded_wait());

    // Check that all the items went through all the all the stages
    for (int i = 0; i < num_items; i++)
        REQUIRE(items[i] == 4);
}

TEST_CASE("simple pipeline, without the builder", "[pipeline]") {
    constexpr int num_items = 50;
    std::array<int, num_items> items{};
    items.fill(0);

    // Construct the pipeline
    // clang-format off
    concore::pipeline<int*> my_pipeline{num_items};
    my_pipeline.add_stage(concore::stage_ordering::concurrent, [](int* data) { REQUIRE((*data)++ == 0); });
    my_pipeline.add_stage(concore::stage_ordering::concurrent, [](int* data) { REQUIRE((*data)++ == 1); });
    my_pipeline.add_stage(concore::stage_ordering::concurrent, [](int* data) { REQUIRE((*data)++ == 2); });
    my_pipeline.add_stage(concore::stage_ordering::concurrent, [](int* data) { REQUIRE((*data)++ == 3); });

    // Push items through the pipeline
    for (int i = 0; i < num_items; i++)
        my_pipeline.push(&items[i]);

    // Wait for all the tasks to complete
    REQUIRE(bounded_wait());

    // Check that all the items went through all the all the stages
    for (int i = 0; i < num_items; i++)
        REQUIRE(items[i] == 4);
}

TEST_CASE("pipeline's max concurrency is enforced", "[pipeline]") {
    constexpr int num_items = 50;
    std::array<int, num_items> items{};
    items.fill(0);

    constexpr int max_concurrency = 2;

    std::atomic<int> par{0};

    // Construct the pipeline
    // clang-format off
    auto my_pipeline = concore::pipeline_builder<int*>(max_concurrency)
        | concore::stage_ordering::concurrent
        | [](int* data) { REQUIRE((*data) == 0); }
        | [&](int* data) {
            REQUIRE((*data) == 0);
            par++;
            std::this_thread::sleep_for(3ms);
            *data = par--;
        }
        | concore::pipeline_end;
    // clang-format on

    // Push items through the pipeline
    for (int i = 0; i < num_items; i++)
        my_pipeline.push(&items[i]);

    // Wait for all the tasks to complete
    REQUIRE(bounded_wait(10s));
    std::this_thread::sleep_for(100ms);

    // Check that all the items went through , and we don't have too much parallelism
    for (int i = 0; i < num_items; i++) {
        REQUIRE(items[i] > 0);
        REQUIRE(items[i] <= max_concurrency);
    }
}

TEST_CASE("pipeline can have out of order stages", "[pipeline]") {
    PROPERTY(([]() {
        constexpr int num_items = 50;

        std::array<int, num_items> dur1{}, dur2{};
        dur1.fill(0);
        dur2.fill(0);
        for (int i = 0; i < num_items; i++) {
            dur1[i] = *rc::gen::inRange(0, 5);
            dur2[i] = *rc::gen::inRange(0, 5);
        }
        std::atomic<int> cur_idx{0};

        // Construct the pipeline
        // clang-format off
        auto my_pipeline = concore::pipeline_builder<int>()
            | concore::stage_ordering::concurrent
            | [&](int idx) {
                // Random wait here, to change the order in which second stage tasks can run
                std::this_thread::sleep_for(dur1[idx] * 100us);
            }
            | concore::stage_ordering::out_of_order
            | [&](int idx) {
                int start_idx = cur_idx.load();
                std::this_thread::sleep_for(dur2[idx] * 100us);
                int end_idx = cur_idx++;
                // If some this task is not serialized, the start and end idices will be different
                REQUIRE(start_idx == end_idx);
            }
            | concore::pipeline_end;
        // clang-format on

        // Push items through the pipeline
        for (int i = 0; i < num_items; i++)
            my_pipeline.push(i);

        // Wait for all the tasks to complete
        std::this_thread::sleep_for(1ms);
        REQUIRE(bounded_wait());

        // Check that we've executed the right number of items
        REQUIRE(cur_idx == num_items);
    }));
}

TEST_CASE("pipeline can have ordered stages", "[pipeline]") {
    PROPERTY(([]() {
        constexpr int num_items = 50;

        std::array<int, num_items> dur{};
        dur.fill(0);
        for (int i = 0; i < num_items; i++)
            dur[i] = *rc::gen::inRange(0, 5);
        std::atomic<int> cur_idx{0};

        // Construct the pipeline
        // clang-format off
        auto my_pipeline = concore::pipeline_builder<int>()
            | concore::stage_ordering::concurrent
            | [&](int idx) {
                // Random wait here, to change the order in which second stage tasks can run
                std::this_thread::sleep_for(dur[idx] * 100us);
            }
            | concore::stage_ordering::in_order
            | [&](int idx) {
                RC_ASSERT(idx == cur_idx++); // The core of the test: items are executed in order
            }
            | concore::pipeline_end;
        // clang-format on

        // Push items through the pipeline
        for (int i = 0; i < num_items; i++)
            my_pipeline.push(i);

        // Wait for all the tasks to complete
        std::this_thread::sleep_for(1ms);
        RC_ASSERT(bounded_wait());

        // Check that we've executed the right number of items
        RC_ASSERT(cur_idx == num_items);
    }));
}

TEST_CASE("pipeline with multiple stages of different ordering", "[pipeline]") {
    // cppcheck-suppress multiCondition
    PROPERTY(([]() {
        constexpr int max_stages = 20;
        int num_items = *rc::gen::inRange(0, 50);
        int num_stages = *rc::gen::inRange(1, max_stages);

        int num_tasks = num_items * num_stages;

        std::vector<int> dur(num_tasks);
        for (int i = 0; i < num_tasks; i++)
            dur[i] = *rc::gen::inRange(0, 5);

        std::array<std::atomic<int>, max_stages> cur_idx{};
        std::atomic<int> completed_tasks = {0};

        // Construct the pipeline
        concore::pipeline_builder<int> builder(num_items);
        for (int stage = 0; stage < num_stages; stage++) {

            auto ord = *rc::gen::element(concore::stage_ordering::in_order,
                    concore::stage_ordering::out_of_order, concore::stage_ordering::concurrent);
            builder.add_stage(ord, [&, stage, ord](int idx) {
                // Random wait here, to have tasks finish at different times
                int start_idx = cur_idx[stage].load();
                std::this_thread::sleep_for(dur[stage * num_items + idx] * 100us);
                int end_idx = cur_idx[stage]++;
                if (ord == concore::stage_ordering::in_order) {
                    REQUIRE(start_idx == end_idx);
                    REQUIRE(start_idx == idx);
                } else if (ord == concore::stage_ordering::in_order)
                    REQUIRE(start_idx == end_idx);
                completed_tasks++;
            });
        }
        auto my_pipeline = builder.build();

        // Push items through the pipeline
        for (int i = 0; i < num_items; i++)
            my_pipeline.push(i);

        // Wait for all the tasks to complete
        std::this_thread::sleep_for(1ms);
        REQUIRE(bounded_wait(10s));

        // Ensure that we executed as much tasks as needed
        REQUIRE(completed_tasks.load() == num_tasks);
    }));
}

TEST_CASE("pipeline can throw exceptions", "[pipeline]") {
    constexpr int num_items = 100;

    std::atomic<int> cnt1{0};
    std::atomic<int> cnt2{0};
    std::atomic<int> cnt3{0};
    std::atomic<int> cnt4{0};

    // Construct the pipeline
    using concore::stage_ordering;
    concore::pipeline_builder<int> builder(num_items);
    builder.add_stage(stage_ordering::concurrent, [&](int idx) {
        if (idx % 2 == 1)
            throw std::runtime_error("test");
        cnt1++;
    });
    builder.add_stage(stage_ordering::in_order, [&](int idx) {
        REQUIRE(idx % 2 == 0);
        REQUIRE(idx == cnt2.load() * 2);
        cnt2++;
    });
    builder.add_stage(stage_ordering::out_of_order, [&](int idx) {
        REQUIRE(idx % 2 == 0);
        cnt3++;
    });
    builder.add_stage(stage_ordering::concurrent, [&](int idx) {
        REQUIRE(idx % 2 == 0);
        cnt4++;
    });
    auto my_pipeline = builder.build();

    // Push items through the pipeline
    for (int i = 0; i < num_items; i++)
        my_pipeline.push(i);

    // Wait for all the tasks to complete
    std::this_thread::sleep_for(1ms);
    REQUIRE(bounded_wait());

    // Check that only half of the items go through the pipeline
    // The rests are stopped by throwing exceptions
    REQUIRE(cnt1.load() == num_items / 2);
    REQUIRE(cnt2.load() == num_items / 2);
    REQUIRE(cnt3.load() == num_items / 2);
    REQUIRE(cnt4.load() == num_items / 2);
}

TEST_CASE("pipeline exceptions are passed to the task_group", "[pipeline]") {
    constexpr int num_items = 100;

    std::atomic<int> num_exceptions{0};

    auto grp = concore::task_group::create();
    grp.set_exception_handler([&](std::exception_ptr) { num_exceptions++; });

    // Construct the pipeline
    using concore::stage_ordering;
    concore::pipeline_builder<int> builder(num_items, grp);
    builder.add_stage(stage_ordering::concurrent, [&](int idx) {
        if (idx % 2 == 1)
            throw std::runtime_error("test");
    });
    builder.add_stage(stage_ordering::concurrent, [&](int idx) {
        REQUIRE(idx % 2 == 0);
        if (idx % 2 == 0)
            throw std::runtime_error("test");
    });
    auto my_pipeline = builder.build();

    // Push items through the pipeline
    for (int i = 0; i < num_items; i++)
        my_pipeline.push(i);

    // Wait for all the tasks to complete
    std::this_thread::sleep_for(1ms);
    REQUIRE(bounded_wait());

    // Check that we've caught all the exceptions: half + half
    REQUIRE(num_exceptions.load() == num_items);
}

TEST_CASE("pipeline uses the given executor", "[pipeline]") {
    constexpr int num_items = 100;

    std::atomic<int> num_executed{0};
    std::atomic<int> cnt1{0};
    std::atomic<int> cnt2{0};

    auto fun = [&num_executed](concore::task t) {
        num_executed++;
        concore::spawn(std::move(t));
    };
    concore::delegating_executor my_executor{fun};

    // Construct the pipeline
    using concore::stage_ordering;
    concore::pipeline_builder<int> builder(num_items, my_executor);
    builder.add_stage(stage_ordering::concurrent, [&](int idx) { cnt1++; });
    builder.add_stage(stage_ordering::concurrent, [&](int idx) { cnt2++; });
    auto my_pipeline = builder.build();

    // Push items through the pipeline
    for (int i = 0; i < num_items; i++)
        my_pipeline.push(i);

    // Wait for all the tasks to complete
    std::this_thread::sleep_for(1ms);
    REQUIRE(bounded_wait());

    // Check that we used our executor for all items, and all stages
    REQUIRE(cnt1.load() == num_items);
    REQUIRE(cnt2.load() == num_items);
    REQUIRE(num_executed.load() == 2 * num_items);
}

TEST_CASE("pipeline stages can modify the line data", "[pipeline]") {
    constexpr int num_items = 50;
    std::array<int, num_items> items{};
    items.fill(0);

    // Construct the pipeline
    // clang-format off
    auto my_pipeline = concore::pipeline_builder<int>(num_items)
        | concore::stage_ordering::concurrent
        | [](int& data) { REQUIRE(data++ == 0); }
        | [](int& data) { REQUIRE(data++ == 1); }
        | [](int& data) { REQUIRE(data++ == 2); }
        | [](int& data) { REQUIRE(data++ == 3); }
        | concore::pipeline_end;
    // clang-format on

    // Push items through the pipeline
    for (int i = 0; i < num_items; i++)
        my_pipeline.push(0);

    // Wait for all the tasks to complete
    REQUIRE(bounded_wait());
}

TEST_CASE("pipeline stages can be decomposed into other tasks", "[pipeline]") {
    constexpr int num_items = 350;
    std::array<int, num_items> items{};
    items.fill(0);

    auto stage_fun = [](int& data) {
        // Get the current continuation
        auto* cur_task = concore::task::current_task();
        REQUIRE(cur_task != nullptr);
        auto cur_cont = cur_task->get_continuation();

        // Add 10 tasks into a serializer, all of them incrementing the data
        // The last task ensures it has the proper continuation
        auto ser = std::make_shared<concore::serializer>();
        auto f = [&data] { data++; };
        for (int i = 0; i < 9; i++)
            concore::execute(*ser, concore::task{f});
        concore::execute(*ser, concore::task{f, {}, std::move(cur_cont)});

        // Clear the continuation from the current task
        cur_task->set_continuation({});

        // The stage is done; no direct increment
    };
    auto stage_check_fun = [](const int& data) {
        // Ensure that we've executed all the tasks for each stage
        REQUIRE(data == 30);
    };

    // Construct the pipeline, with a stage of each type
    // clang-format off
    auto my_pipeline = concore::pipeline_builder<int>(num_items)
        | concore::stage_ordering::concurrent
        | stage_fun
        | concore::stage_ordering::out_of_order
        | stage_fun
        | concore::stage_ordering::in_order
        | stage_fun
        | concore::stage_ordering::concurrent
        | stage_check_fun
        | concore::pipeline_end;
    // clang-format on

    // Push items through the pipeline
    for (int i = 0; i < num_items; i++)
        my_pipeline.push(0);

    // Wait for all the tasks to complete
    REQUIRE(bounded_wait());
}
