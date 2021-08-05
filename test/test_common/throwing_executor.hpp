#pragma once

struct throwing_executor {
    template <typename F>
    void execute(F&& f) const {
        throw std::logic_error("err");
    }
    void execute(concore::task&& t) const noexcept {
        auto cont = t.get_continuation();
        if (cont) {
            try {
                throw std::logic_error("err");
            } catch (...) {
                cont(std::current_exception());
            }
        }
    }
    void operator()(concore::task t) const { execute(std::move(t)); }

    friend inline bool operator==(throwing_executor, throwing_executor) { return true; }
    friend inline bool operator!=(throwing_executor, throwing_executor) { return false; }
};
