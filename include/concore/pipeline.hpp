#pragma once

#include "concore/task_group.hpp"
#include "concore/any_executor.hpp"

#include <memory>
#include <functional>

namespace concore {

inline namespace v1 {

//! The possible ordering constraints for a stage in a pipeline.
enum class stage_ordering {
    in_order,     //!< All the items are processed in the order they were started, one at a time.
    out_of_order, //!< Items are processed one at a time, but not necessarily in a specific order.
    concurrent,   //!< No constraints; all items can be processed concurrently.
};

} // namespace v1

namespace detail {
// Forward declaration; see .cpp file
struct pipeline_data;

//! Main attributes for a line; keeps track of the execution of the linee.
struct line_base {
    int stage_idx_ : 31; //!< The stage in which the line is/
    int stopped_ : 1;    //!< Set to true if exceptions appear and later stages should be skipped
    int order_idx_{0};   //!< The order index of the line; used for ordered stages.

    line_base()
        : stage_idx_(0)
        , stopped_(0) {}
};

//! A types line data, to contain the actual data passed in by the user.
template <typename T>
struct typed_line : line_base {
    //! The line data passed in by the user
    T data_;

    explicit typed_line(T line_data)
        : data_(std::move(line_data)) {}
};

//! Function describing the processing to be done for a stage.
//! Such a function is constructed from user's supplied functor.
using stage_fun = std::function<void(line_base*)>;

//! Create a `stage_fun` object from the given work function
//! The work function `F` must have the type: void(T)
template <typename T, typename F>
inline stage_fun create_stage_fun(F&& work) {
    return [work = std::forward<F>(work)](detail::line_base* line) {
        auto l = static_cast<detail::typed_line<T>*>(line);
        work(l->data_);
    };
}

//! Untyped implementation details for the pipeline.
//! Using this to perform type erasure.
struct pipeline_impl {
    //! The private data for the pipeline
    std::shared_ptr<pipeline_data> data_;

    explicit pipeline_impl(int max_concurrency);
    pipeline_impl(int max_concurrency, task_group grp);
    pipeline_impl(int max_concurrency, task_group grp, any_executor exe);
    pipeline_impl(int max_concurrency, any_executor exe);
    ~pipeline_impl();

    pipeline_impl(pipeline_impl&&) noexcept;
    pipeline_impl& operator=(pipeline_impl&&) noexcept;

    pipeline_impl(const pipeline_impl&);
    pipeline_impl& operator=(const pipeline_impl&);

    //! Called to add a stage into the pipeline.
    void do_add_stage(stage_ordering ord, stage_fun&& f);

    //! Called to start processing a new line.
    void do_start_line(std::shared_ptr<line_base>&& line);
};

} // namespace detail

inline namespace v1 {

template <typename T>
class pipeline_builder;

/**
 * @brief      Implements a pipeline, used to add concurrency to processing of items that need
 *             several stages of processing with various ordering constraints.
 *
 * @tparam     T  The type of items that flow through the pipeline.
 *
 * A pipeline is a sequence of stages in a the processing of a items (lines). All items share the
 * same stages. All the stages operate on the same type (line type).
 *
 * The pipeline is built with the help of the @ref pipeline_builder class.
 *
 * Even if the stages need to be run in sequence, we might get concurrency out of this structure as
 * we can execute multiple lines concurrently. However, most of the pipelines also add more
 * constraints on the execution of lines.
 *
 * The first constraint that one might add is the number of lines that can be processed
 * concurrently. This is done by a parameter passed to the constructor.
 *
 * The other way to constraint the pipeline is to add various ordering constraints for the stages.
 * We might have multiple ordering constraints for stages:
 *      - `in_order`: at most one item can be processed in the stage, and the items are executed in
 *          the order in which they are added to the pipeline
 *      - `out_of_order`: at most one item can be processed in the stage, but the order of
 *          processing doesn't matter
 *      - `concurrent`: no constraints are set; the items can run concurrently on the stage
 *
 * The `in_order` type adds the more constraints to a stage; `out_of_order` is a bit more flexible,
 * but still limits the concurrency a lot. The most relaxed mode is `concurrent`.
 *
 * If a pipeline has only `in_order` stages, then the concurrency of the pipeline grows to the
 * number of stages it has; but typically the concurrency is very limited. We gain concurrency if
 * we can make some of the stages `concurrent`. A pipeline scales well with respect to concurrency
 * if most of its processing is happening in `concurrent` stages.
 *
 * If a stage processing throws an exception, then, for that particular line, the next stages will
 * not be run. If some of the next stages are `in_order` then the next items will not be blocked
 * by not executing this stage; i.e., the processing in the stage is just skipped.
 *
 * Example of building a pipeline:
 *      auto my_pipeline = concore::pipeline_builder<int>()
 *          | concore::stage_ordering::concurrent
 *          | [&](int idx) {
 *              work1(idx);
 *          }
 *          | concore::stage_ordering::out_of_order
 *          | [&](int idx) {
 *              work2(idx);
 *          }
 *          | concore::pipeline_end;
 *      for ( int i=0; i<100; i++)
 *          my_pipeline.push(i);
 *
 * @see        pipeline_builder, stage_ordering, serializer, n_serializer
 */
template <typename T>
class pipeline {
public:
    /**
     * @brief      Pushes a new item (line) through the pipeline
     *
     * @param      line_data  The data associated with the line
     *
     * This will start processing from the first stage and will iteratively pass through all the
     * stages of the pipeline. The same line data is passed to the functors registered with each
     * stage of the pipeline; i.e., all the stages of the pipeline work on the same line.
     */
    void push(T line_data) {
        impl_.do_start_line(std::make_shared<detail::typed_line<T>>(line_data));
    }

private:
    //! Implementation details of the pipeline; with type erasure
    detail::pipeline_impl impl_;

    friend pipeline_builder<T>;

    pipeline(detail::pipeline_impl&& impl)
        : impl_(std::move(impl)) {}
};

//! Tag type to help end the expression of building a pipeline
struct pipeline_end_t {};

//! Tag value to help end the expression of building a pipeline
constexpr auto pipeline_end = pipeline_end_t{};

/**
 * @brief      Front-end to create pipeline objects by adding stages, step by step.
 *
 * @tparam     T     The type of the data corresponding to a line.
 *
 * This tries to extract the building of the pipeline stages from the @ref pipeline class.
 *
 * It just knows how to configure a pipeline and then to create an actual @ref pipeline object.
 *
 * After we get a pipeline object, this builder cannot be used anymore.
 *
 * Example of building a pipeline:
 *      auto my_pipeline = concore::pipeline_builder<MyT>()
 *          | concore::stage_ordering::concurrent
 *          | [&](MyT data) {
 *              work1(data);
 *          }
 *          | concore::stage_ordering::out_of_order
 *          | [&](MyT data) {
 *              work2(data);
 *          }
 *          | concore::pipeline_end;
 *
 */
template <typename T>
class pipeline_builder {
public:
    /**
     * @brief      Constructs a pipeline object
     *
     * @param      max_concurrency  The concurrency limit for the pipeline
     */
    explicit pipeline_builder(int max_concurrency = 0xffff)
        : impl_(max_concurrency) {}
    /**
     * @brief      Constructs a pipeline object
     *
     * @param      max_concurrency  The concurrency limit for the pipeline
     * @param      grp              The group in which tasks need to be executed
     */
    pipeline_builder(int max_concurrency, task_group grp)
        : impl_(max_concurrency, std::move(grp)) {}
    /**
     * @brief      Constructs a pipeline object
     *
     * @param      max_concurrency  The concurrency limit for the pipeline
     * @param      grp              The group in which tasks need to be executed
     * @param      exe              The executor to be used by the pipeline
     */
    pipeline_builder(int max_concurrency, task_group grp, any_executor exe)
        : impl_(max_concurrency, std::move(grp), std::move(exe)) {}
    /**
     * @brief      Constructs a pipeline object
     *
     * @param      max_concurrency  The concurrency limit for the pipeline
     * @param      exe              The executor to be used by the pipeline
     */
    pipeline_builder(int max_concurrency, any_executor exe)
        : impl_(max_concurrency, std::move(exe)) {}

    /**
     * @brief      Adds a stage to the pipeline
     *
     * @param      ord   The ordering for the stage
     * @param      work  The work to be done in this stage
     *
     * @tparam     F     The type of the work
     *
     * This takes a functor of type `void (T)` and an ordering and
     * constructs a stage in the pipeline with them.
     *
     * @see        stage_ordering
     */
    template <typename F>
    pipeline_builder& add_stage(stage_ordering ord, F&& work) {
        impl_.do_add_stage(ord, detail::create_stage_fun<T>(std::forward<F>(work)));
        return *this;
    }

    /**
     * @brief      Creates the actual pipeline object, ready to process items
     *
     * After calling this, we can no longer own any pipeline data, and we cannot add stages any
     * longer. The returned pipeline object is ready to process items with the stages defined by
     * this class.
     */
    operator pipeline<T>() { return pipeline<T>(std::move(impl_)); }

    /**
     * @brief      Creates the actual pipeline object, ready to process items
     *
     * @return     Resulting @ref pipeline object.
     *
     * After calling this, we can no longer own any pipeline data, and we cannot add stages any
     * longer. The returned pipeline object is ready to process items with the stages defined by
     * this class.
     */
    pipeline<T> build() { return pipeline<T>(std::move(impl_)); }

    /**
     * @brief      Pipe operator to specify the ordering for the next stages
     *
     * @param      ord   The ordering to be applied to next stages
     *
     * @return     The same pipeline_builder object
     *
     * This allows easily constructing pipelines by using the '|' operator.
     */
    pipeline_builder& operator|(stage_ordering ord) {
        next_ordering_ = ord;
        return *this;
    }

    /**
     * @brief      Pipe operator to add new stages to the pipeline
     *
     * @param      work  The work corresponding to the stage; needs to be of type `void(T)`
     *
     * @tparam     F     The type of the functor
     *
     * @return     The same pipeline_builder object
     *
     * This adds a new stage to the pipeline, using the latest specified stage ordering. If no stage
     * ordering is specified, before adding this stage, the `in_order` is used.
     */
    template <typename F>
    pipeline_builder& operator|(F&& work) {
        impl_.do_add_stage(next_ordering_, detail::create_stage_fun<T>(std::forward<F>(work)));
        return *this;
    }

    /**
     * @brief      Pipe operator to a tag that tells us that we are done building the pipeline
     *
     * @param      <unnamed>  A tag value
     *
     * @return     The @ref pipeline object built by this @ref pipeline_builder object.
     *
     * This will actually finalize the building process and return the corresponding @ref pipeline
     * object. After this is called, any other operations on the builder are illegal.
     */
    pipeline<T> operator|(pipeline_end_t) {
        return pipeline<T>(std::move(impl_));
        ;
    }

private:
    //! Implementation details of the pipeline; with type erasure
    detail::pipeline_impl impl_;
    //! The next stage ordering to apply
    stage_ordering next_ordering_{stage_ordering::in_order};
};

} // namespace v1
} // namespace concore