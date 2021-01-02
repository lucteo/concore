
#include "concore/pipeline.hpp"
#include "concore/global_executor.hpp"
#include "concore/serializer.hpp"
#include "concore/detail/consumer_bounded_queue.hpp"

#include <vector>

namespace concore {

namespace detail {

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

    stage_data(stage_ordering ord, stage_fun&& f, any_executor exe)
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
    any_executor executor_;

    //! All the stages in the pipeline
    std::vector<stage_data> stages_;

    //! A queue with a maximum number of consumers; used to ensure that we don't process too many
    //! lines at a time
    consumer_bounded_queue<line_ptr> processing_items_;

    //! The current order index; used to assign each line a unique number
    std::atomic<int> cur_order_idx_{0};

    pipeline_data(int max_concurrency, task_group grp, any_executor exe)
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

pipeline_impl::~pipeline_impl() = default;

pipeline_impl::pipeline_impl(pipeline_impl&&) noexcept = default;
pipeline_impl& pipeline_impl::operator=(pipeline_impl&&) noexcept = default;

pipeline_impl::pipeline_impl(const pipeline_impl&) = default;
pipeline_impl& pipeline_impl::operator=(const pipeline_impl&) = default;

pipeline_impl::pipeline_impl(int max_concurrency)
    : data_(std::make_shared<pipeline_data>(max_concurrency, task_group{}, global_executor{})) {}
pipeline_impl::pipeline_impl(int max_concurrency, task_group grp)
    : data_(std::make_shared<pipeline_data>(max_concurrency, std::move(grp), global_executor{})) {}
pipeline_impl::pipeline_impl(int max_concurrency, task_group grp, any_executor exe)
    : data_(std::make_shared<pipeline_data>(max_concurrency, std::move(grp), std::move(exe))) {}
pipeline_impl::pipeline_impl(int max_concurrency, any_executor exe)
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