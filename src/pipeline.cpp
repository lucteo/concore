
#include "concore/pipeline.hpp"
#include "concore/global_executor.hpp"
#include "concore/serializer.hpp"
#include "concore/data/concurrent_queue.hpp"

namespace concore {

namespace detail {

/**
 * @brief      A queue used to allow processing a limited amount of items.
 *
 * This similar to @ref n_serializer, but can be used to implement other types of concurrent
 * structures. Given a limit on how many items we can actively process at once, this class helps
 * keep track when items can become active and when it's the best time to execute them.
 *
 * When adding a new item, one of the two can happen:
 * a) we haven't reached our concurrency limit, so we can start processing an item
 * b) we reached the limit, and the item needs to be added to a waiting list
 *
 * Whenever an item is considered released (done processing it), if there are non-active queued
 * items, then we can continue to process another item.
 *
 * @tparam     T     The types of items we are keeping track of
 *
 * @see        n_serializer, pipeline
 */
template <typename T>
struct consumer_bounded_queue {
    /**
     * @brief      Construct the consume bounded queue with the maximum allowed concurrency for
     *             processing the items
     *
     * @param      max_active  The maximum active items
     */
    explicit consumer_bounded_queue(int max_active)
        : max_active_(max_active) {}

    /**
     * @brief      Pushes a new item and try acquire the processing of an item.
     *
     * @param      elem  The element to be pushed to our structure
     *
     * @return     True if we can start processing a new item
     *
     * This will add the given element to the list of items that need to be processed. If there are
     * active slots, this will return true indicating that it's safe to process one item.
     *
     * The item to be processed is NOT the item that is just pushed here. Instead, the user must
     * call @ref extract_one() to get the item to be processed. The elements to be processed would
     * be extracted in the order in which they were pushed; we push to the back, and extract from
     * the front.
     *
     * @warning    If this method returns true, the user should have exactly one call to
     *             @ref extract_one(), and then exactly one call to @ref release_and_acquire()
     *
     */
    bool push_and_try_acquire(T&& elem) {
        waiting_.push(std::move(elem));

        // Increase the number of items we keep track of
        // Check if we can increase the "active" items; if so, acquire an item
        count_bits old, desired;
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.total++;
            if (desired.fields.active < max_active_)
                desired.fields.active++;
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        return desired.fields.active != old.fields.active;
    }

    /**
     * @brief      Extracts one item to be processed
     *
     * @return     The item to be processed
     *
     * This should be called each time @ref push_and_try_acquire() or @ref release_and_acquire()
     * returns true. For each call to this function, one need to call @ref release_and_acquire().
     */
    T extract_one() {
        // In the vast majority of cases there is a task ready to be executed; but there can be some
        // race conditions that will prevent the first pop from extracting a task from the queue. In
        // this case, try to spin for a bit
        T elem;
        spin_backoff spinner;
        while (!waiting_.try_pop(elem))
            spinner.pause();
        // TODO: check & try to fix this
        return elem;
    }

    /**
     * @brief      Called when an item is done processing
     *
     * @return     True if the caller needs to start processing another item
     *
     * This should be called each time we are done processing an item (got with @ref extract_one()).
     * If this returns true, it means that we should immediately start processing another item. For
     * this one needs to call @ref extract_one() and then @ref release_and_acquire() again.
     */
    bool release_and_acquire() {
        // Decrement the number of elements
        // Check if we need to acquire a new element
        count_bits old, desired;
        old.int_value = combined_count_.load();
        do {
            desired.int_value = old.int_value;
            desired.fields.total--;
            desired.fields.active = std::min(desired.fields.active, desired.fields.total);
        } while (!combined_count_.compare_exchange_weak(old.int_value, desired.int_value));

        // If the number of active items remains the same, it means that we just acquired a slot
        return desired.fields.active == old.fields.active;
    }

private:
    //! The maximum number of active items
    int max_active_;
    //! Queue of items that are waiting (or not yet extracted)
    concurrent_queue<T, queue_type::multi_prod_multi_cons> waiting_;
    //! The count of items in our queue (active & total)
    std::atomic<uint64_t> combined_count_{0};

    union count_bits {
        uint64_t int_value;
        struct {
            uint32_t active;
            uint32_t total;
        } fields;
    };
};

//! The type of data needed to keep track of a line
using line_ptr = std::shared_ptr<line_base>;

struct stage_data {
    //! The ordering to be applied in this stage
    const stage_ordering ord_;
    //! The function to be called for this stage (taking a line as parameter)
    const stage_fun fun_;
    //! The serializer to be used in the case of in_order and out_of_order execution
    serializer ser_;

    //! A vector of pending lines, used in the case of in_order execution
    //! The second element in the pair is the order_idx_ of the line.
    std::vector<std::pair<line_ptr, int>> pending_lines_;
    //! The next expected order_idx; used in in_order stages to ensure ordering
    int expected_order_idx_{0};

    stage_data(stage_ordering ord, stage_fun&& f, executor_t exe)
        : ord_(ord)
        , fun_(std::move(f))
        , ser_(exe) {}

    //! Add a line in our pending list.
    //! Keep the list sorted by order_idx.
    //! Note: access to this needs to be serialized
    void add_pending(line_ptr line) {
        int order_idx = line->order_idx_;
        auto comp = [](const std::pair<line_ptr, int>& p, int order_idx) {
            return p.second < order_idx;
        };
        auto it = std::lower_bound(pending_lines_.begin(), pending_lines_.end(), order_idx, comp);
        pending_lines_.insert(it, std::make_pair(std::move(line), order_idx));
    }
};

//! All the data needed for the pipeline to run
struct pipeline_data : std::enable_shared_from_this<pipeline_data> {
    //! The group to be used for all the tasks that we create here
    task_group group_;
    //! The executor to be used for executing tasks
    executor_t executor_;

    //! All the stages in the pipeline
    std::vector<stage_data> stages_;

    //! A queue with a maximum number of consumers; used to ensure that we don't process too many
    //! lines at a time
    consumer_bounded_queue<line_ptr> processing_items_;

    //! The current order index; used to assign each line a unique number
    std::atomic<int> cur_order_idx_{0};

    pipeline_data(int max_concurrency, task_group grp, executor_t exe)
        : group_(std::move(grp))
        , executor_(std::move(exe))
        , processing_items_(max_concurrency) {}

    //! Start processing a line; from the first stage, and go up to the last one
    void start(line_ptr&& line);
    //! Run the current needed task for a line; i.e., for the current stage
    void run(line_ptr&& line);

    //! Called to execute the work item for the given stage onto the given line
    void execute_stage_task(stage_data& stage, line_ptr&& line);
    //! Called whenever the task is complete to trigger the next stage (and maybe next line)
    void on_task_done(stage_data& stage, line_ptr&& line);
};

void pipeline_data::start(line_ptr&& line) {
    assert(line->stage_idx_ == 0);
    if (processing_items_.push_and_try_acquire(std::move(line))) {
        run(processing_items_.extract_one());
    }
}

void pipeline_data::run(line_ptr&& line) {
    assert(line->stage_idx_ < int(stages_.size()));
    auto& stage = stages_[line->stage_idx_];
    if (stage.ord_ == stage_ordering::concurrent) {
        auto fun = [this, line = std::move(line)]() mutable {
            execute_stage_task(stages_[line->stage_idx_], std::move(line));
        };
        executor_(task{std::move(fun), group_});
    } else if (stage.ord_ == stage_ordering::out_of_order) {
        auto fun = [this, line = std::move(line)]() mutable {
            execute_stage_task(stages_[line->stage_idx_], std::move(line));
        };
        stage.ser_(task{std::move(fun), group_});
    } else if (stage.ord_ == stage_ordering::in_order) {
        auto push_task_fun = [this, line = std::move(line)]() mutable {
            auto& stage = stages_[line->stage_idx_];
            if (line->order_idx_ == stage.expected_order_idx_) {
                stage.expected_order_idx_++;
                execute_stage_task(stages_[line->stage_idx_], std::move(line));
            } else {
                // We cannot run this line yet; we have to wait for other lines first
                stage.add_pending(std::move(line));
            }
        };
        stage.ser_(task{std::move(push_task_fun), group_});
    }
}

void pipeline_data::execute_stage_task(stage_data& stage, line_ptr&& line) {
    try {
        if (!line->stopped_)
            stage.fun_(line.get());
        on_task_done(stage, std::move(line));
    } catch (...) {
        line->stopped_ = 1;
        on_task_done(stage, std::move(line));
        throw;
    }
}

void pipeline_data::on_task_done(stage_data& stage, line_ptr&& line) {
    // If we are in an `in_orde` stage, check if we can start the next line
    if (stage.ord_ == stage_ordering::in_order) {
        if (!stage.pending_lines_.empty() &&
                stage.pending_lines_.front().second == stage.expected_order_idx_) {
            line_ptr next_line = std::move(stage.pending_lines_.front().first);
            stage.pending_lines_.erase(stage.pending_lines_.begin());
            run(std::move(next_line));
        }
    }

    // Move to the next stage
    if (++line->stage_idx_ < int(stages_.size())) {
        run(std::move(line)); // run the next stage
    } else {
        // If we are at maximum capacity, try to start a new line (from first stage)
        if (processing_items_.release_and_acquire())
            run(processing_items_.extract_one());
    }
}

pipeline_impl::pipeline_impl(pipeline_impl&& other)
    : data_(std::move(other.data_)) {}
pipeline_impl::pipeline_impl(int max_concurrency)
    : data_(std::make_shared<pipeline_data>(max_concurrency, task_group{}, global_executor)) {}
pipeline_impl::pipeline_impl(int max_concurrency, task_group grp)
    : data_(std::make_shared<pipeline_data>(max_concurrency, std::move(grp), global_executor)) {}
pipeline_impl::pipeline_impl(int max_concurrency, task_group grp, executor_t exe)
    : data_(std::make_shared<pipeline_data>(max_concurrency, std::move(grp), std::move(exe))) {}
pipeline_impl::pipeline_impl(int max_concurrency, executor_t exe)
    : data_(std::make_shared<pipeline_data>(max_concurrency, task_group{}, std::move(exe))) {}

void pipeline_impl::do_add_stage(stage_ordering ord, stage_fun&& f) {
    assert(data_);
    data_->stages_.emplace_back(ord, std::move(f), data_->executor_);
}

void pipeline_impl::do_start_line(std::shared_ptr<line_base>&& line) {
    assert(data_);
    line->order_idx_ = data_->cur_order_idx_++;
    data_->start(std::move(line));
}

} // namespace detail
} // namespace concore