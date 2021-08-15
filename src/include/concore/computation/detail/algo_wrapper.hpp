#pragma once

namespace concore {
namespace computation {
namespace detail {

//! Curring functor to help create computations with lazy "previous computation"
template <typename CreateFun, typename T>
struct currying_create_fun {
    CreateFun createFun_;
    T arg1_;

    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp>
    auto operator()(Comp&& comp) const& {
        return createFun_((Comp &&) comp, arg1_);
    }

    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp>
    auto operator()(Comp&& comp) && {
        return ((CreateFun &&) createFun_)((Comp &&) comp, (T &&) arg1_);
    }
};

template <typename CreateFun, typename T>
currying_create_fun<CreateFun, T> make_currying_create_fun(CreateFun createFun, T arg1) {
    return {(CreateFun &&) createFun, (T &&) arg1};
}

template <typename F>
struct algo_wrapper;

template <typename F1, typename F2>
struct concat_algo_wrapper_fun {
    algo_wrapper<F1> w1_;
    algo_wrapper<F2> w2_;

    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp>
    auto operator()(Comp&& c) const& {
        return w2_(w1_(c));
    }

    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp>
    auto operator()(Comp&& c) && {
        return std::move(w2_)(std::move(w1_)(c));
    }
};

//! Wrapper for a computation algorithm. This allows the input computation to be passed in later to
//! a computation algorithm. It allows the computation algorithm to be piped in.
template <typename CreateFun>
struct algo_wrapper {
    //! The functor used to create the actual computation instance
    CreateFun createFun_;

    explicit algo_wrapper(CreateFun&& createFun)
        : createFun_((CreateFun &&) createFun) {}

    //! Call operator that will create the final computation given the input computation
    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp>
    auto operator()(Comp&& comp) const& {
        return createFun_((Comp &&) comp);
    }
    //! Call operator that will create the final computation given the input computation -- move
    //! version
    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp>
    auto operator()(Comp&& comp) && {
        return ((CreateFun &&) createFun_)((Comp &&) comp);
    }

    //! Pipe operator, allowing us to pipe the computation algorithms
    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp>
    friend auto operator|(Comp&& comp, algo_wrapper&& self) {
        return std::move(self.createFun_)((Comp &&) comp);
    }
    template <CONCORE_CONCEPT_OR_TYPENAME(computation) Comp>
    friend auto operator|(Comp&& comp, const algo_wrapper& self) {
        return self.createFun_((Comp &&) comp);
    }

    //! Pipe operator, allowing us to concatenate two wrappers
    template <typename F1>
    friend auto operator|(algo_wrapper<F1>&& w1, algo_wrapper&& self) {
        return make_algo_wrapper(
                concat_algo_wrapper_fun<F1, CreateFun>{std::move(w1), std::move(self)});
    }
    template <typename F1>
    friend auto operator|(const algo_wrapper<F1>& w1, const algo_wrapper& self) {
        return make_algo_wrapper(concat_algo_wrapper_fun<F1, CreateFun>{w1, self});
    }
};

template <typename F>
inline auto make_algo_wrapper(F&& f) {
    return algo_wrapper<F>((F &&) f);
}

template <typename F, typename T>
inline auto make_algo_wrapper(F&& f, T&& arg1) {
    auto cf = make_currying_create_fun((F &&) f, (T &&) arg1);
    return algo_wrapper<decltype(cf)>(std::move(cf));
}

} // namespace detail
} // namespace computation
} // namespace concore