#pragma once

#include <mutex>

// User can specify a "profiling include" to specify how profiling needs to be done
#ifdef CONCORE_PROFILING_INCLUDE
#include CONCORE_PROFILING_INCLUDE
#endif

#if TRACY_ENABLE

#include "Tracy.hpp"

#define CONCORE_ENABLE_PROFILING 1
#define CONCORE_ENABLE_LOCK_PROFILING 0

#define CONCORE_PROFILING_INIT() static_cast<void>(tracy::GetProfiler())
#define CONCORE_PROFILING_FUNCTION() ZoneScopedN(__FUNCTION__)
#define CONCORE_PROFILING_SCOPE() ZoneScoped
#define CONCORE_PROFILING_SCOPE_N(staticName) ZoneScopedN(staticName)
#define CONCORE_PROFILING_SCOPE_C(color) ZoneScopedC(color)
#define CONCORE_PROFILING_SCOPE_NC(staticName, color) ZoneScopedNC(staticName, color)
#define CONCORE_PROFILING_SET_DYNNAME(name) ZoneName(name, strlen(name))
#define CONCORE_PROFILING_SET_TEXT(text) ZoneText(text, strlen(text))
#define CONCORE_PROFILING_SET_TEXT_FMT(max_len, fmt, args...)                                      \
    char __IMPL_CONCORE_CONCAT(__concore_profiling_buf, __LINE__)[max_len];                        \
    snprintf(__IMPL_CONCORE_CONCAT(__concore_profiling_buf, __LINE__), max_len, fmt, args);        \
    ZoneText(__IMPL_CONCORE_CONCAT(__concore_profiling_buf, __LINE__),                             \
            strlen(__IMPL_CONCORE_CONCAT(__concore_profiling_buf, __LINE__)))
#define CONCORE_PROFILING_MESSAGE(text) TracyMessage(text, strlen(text))
#define CONCORE_PROFILING_PLOT(staticName, val) TracyPlot(staticName, val)

#if CONCORE_ENABLE_LOCK_PROFILING
#define CONCORE_PROFILING_MUTEX_CONTEXT(ctx) tracy::LockableCtx ctx
#define CONCORE_PROFILING_MUTEX_INIT_CONTEXT(ctx, loc) : ctx(loc)
#define CONCORE_PROFILING_MUTEX_BEORE_LOCK(ctx) const auto run_after = (ctx).BeforeLock()
#define CONCORE_PROFILING_MUTEX_AFTER_LOCK(ctx)                                                    \
    if (run_after)                                                                                 \
    (ctx).AfterLock()
#define CONCORE_PROFILING_MUTEX_AFTER_UNLOCK(ctx) (ctx).AfterUnlock()
#define CONCORE_PROFILING_MUTEX_AFTER_TRY_LOCK(ctx, acquired)                                      \
    if (acquired) {                                                                                \
        if ((ctx).BeforeLock())                                                                    \
            (ctx).AfterLock();                                                                     \
    }
#define CONCORE_PROFILING_MUTEX_MARK_LOCATION(ctx, loc) (ctx).Mark(loc)
#else
#define CONCORE_PROFILING_MUTEX_CONTEXT(ctx)                  /*nothing*/
#define CONCORE_PROFILING_MUTEX_INIT_CONTEXT(ctx, loc)        /*nothing*/
#define CONCORE_PROFILING_MUTEX_BEORE_LOCK(ctx)               /*nothing*/
#define CONCORE_PROFILING_MUTEX_AFTER_LOCK(ctx)               /*nothing*/
#define CONCORE_PROFILING_MUTEX_AFTER_UNLOCK(ctx)             /*nothing*/
#define CONCORE_PROFILING_MUTEX_AFTER_TRY_LOCK(ctx, acquired) /*nothing*/
#define CONCORE_PROFILING_MUTEX_MARK_LOCATION(ctx, loc)       /*nothing*/
#endif

#define CONCORE_PROFILING_LOCATION_TYPE const tracy::SourceLocationData*
#define CONCORE_PROFILING_LOCATION()                                                               \
    []() -> const tracy::SourceLocationData* {                                                     \
        static const tracy::SourceLocationData loc{nullptr, __FUNCTION__, __FILE__, __LINE__, 0};  \
        return &loc;                                                                               \
    }()
#define CONCORE_PROFILING_LOCATION_N(staticName)                                                   \
    [](const char* name) -> const tracy::SourceLocationData* {                                     \
        static const tracy::SourceLocationData loc{name, __FUNCTION__, __FILE__, __LINE__, 0};     \
        return &loc;                                                                               \
    }(staticName)
#define CONCORE_PROFILING_MUTEX_NAME(staticName)                                                   \
    [](const char* name) -> const tracy::SourceLocationData* {                                     \
        static const tracy::SourceLocationData loc{nullptr, name, __FILE__, __LINE__, 0};          \
        return &loc;                                                                               \
    }(staticName)
#define CONCORE_PROFILING_MUTEX_NAME0()                                                            \
    []() -> const tracy::SourceLocationData* {                                                     \
        static const tracy::SourceLocationData loc{nullptr, nullptr, __FILE__, __LINE__, 0};       \
        return &loc;                                                                               \
    }()

#define __IMPL_CONCORE_EXPAND2(x) __IMPL_CONCORE_EXPAND1(x)
#define __IMPL_CONCORE_EXPAND1(x) #x
#define __IMPL_CONCORE_CONCAT2(x, y) x##y
#define __IMPL_CONCORE_CONCAT(x, y) __IMPL_CONCORE_CONCAT2(x, y)

#define CONCORE_PROFILING_COLOR_WHITE 0xFFFFFF
#define CONCORE_PROFILING_COLOR_SILVER 0xC0C0C0
#define CONCORE_PROFILING_COLOR_GRAY 0x808080
#define CONCORE_PROFILING_COLOR_BLACK 0x000000
#define CONCORE_PROFILING_COLOR_RED 0xFF0000
#define CONCORE_PROFILING_COLOR_MAROON 0x800000
#define CONCORE_PROFILING_COLOR_YELLOW 0xFFFF00
#define CONCORE_PROFILING_COLOR_OLIVE 0x808000
#define CONCORE_PROFILING_COLOR_LIME 0x00FF00
#define CONCORE_PROFILING_COLOR_GREEN 0x008000
#define CONCORE_PROFILING_COLOR_AQUA 0x00FFFF
#define CONCORE_PROFILING_COLOR_TEAL 0x008080
#define CONCORE_PROFILING_COLOR_BLUE 0x0000FF
#define CONCORE_PROFILING_COLOR_NAVY 0x000080
#define CONCORE_PROFILING_COLOR_FUCHSIA 0xFF00FF
#define CONCORE_PROFILING_COLOR_PURPLE 0x800080

#define CONCORE_PROFILING_SETTHREADNAME(staticName) tracy::SetThreadName(staticName)

#endif

#if !CONCORE_ENABLE_PROFILING

#define CONCORE_PROFILING_INIT()                              /*nothing*/
#define CONCORE_PROFILING_FUNCTION()                          /*nothing*/
#define CONCORE_PROFILING_SCOPE()                             /*nothing*/
#define CONCORE_PROFILING_SCOPE_N(staticName)                 /*nothing*/
#define CONCORE_PROFILING_SCOPE_C(color)                      /*nothing*/
#define CONCORE_PROFILING_SCOPE_NC(staticName, color)         /*nothing*/
#define CONCORE_PROFILING_SET_DYNNAME(name)                   /*nothing*/
#define CONCORE_PROFILING_SET_TEXT(text)                      /*nothing*/
#define CONCORE_PROFILING_SET_TEXT_FMT(max_len, fmt, args...) /*nothing*/
#define CONCORE_PROFILING_MESSAGE(text)                       /*nothing*/
#define CONCORE_PROFILING_PLOT(staticName, val)               /*nothing*/

#define CONCORE_PROFILING_LOCATION_TYPE void*
#define CONCORE_PROFILING_LOCATION() nullptr
#define CONCORE_PROFILING_COLOR_IDLE 0

#define CONCORE_PROFILING_MUTEX_CONTEXT(ctx)                  /*nothing*/
#define CONCORE_PROFILING_MUTEX_INIT_CONTEXT(ctx, loc)        /*nothing*/
#define CONCORE_PROFILING_MUTEX_BEORE_LOCK(ctx)               /*nothing*/
#define CONCORE_PROFILING_MUTEX_AFTER_LOCK(ctx)               /*nothing*/
#define CONCORE_PROFILING_MUTEX_AFTER_UNLOCK(ctx)             /*nothing*/
#define CONCORE_PROFILING_MUTEX_AFTER_TRY_LOCK(ctx, acquired) /*nothing*/
#define CONCORE_PROFILING_MUTEX_MARK_LOCATION(ctx, loc)       /*nothing*/

#define CONCORE_PROFILING_LOCATION_TYPE void*
#define CONCORE_PROFILING_LOCATION() nullptr
#define CONCORE_PROFILING_LOCATION_N(staticName) nullptr
#define CONCORE_PROFILING_MUTEX_NAME(staticName) nullptr
#define CONCORE_PROFILING_MUTEX_NAME0() nullptr

#define CONCORE_PROFILING_COLOR_WHITE   /*nothing*/
#define CONCORE_PROFILING_COLOR_SILVER  /*nothing*/
#define CONCORE_PROFILING_COLOR_GRAY    /*nothing*/
#define CONCORE_PROFILING_COLOR_BLACK   /*nothing*/
#define CONCORE_PROFILING_COLOR_RED     /*nothing*/
#define CONCORE_PROFILING_COLOR_MAROON  /*nothing*/
#define CONCORE_PROFILING_COLOR_YELLOW  /*nothing*/
#define CONCORE_PROFILING_COLOR_OLIVE   /*nothing*/
#define CONCORE_PROFILING_COLOR_LIME    /*nothing*/
#define CONCORE_PROFILING_COLOR_GREEN   /*nothing*/
#define CONCORE_PROFILING_COLOR_AQUA    /*nothing*/
#define CONCORE_PROFILING_COLOR_TEAL    /*nothing*/
#define CONCORE_PROFILING_COLOR_BLUE    /*nothing*/
#define CONCORE_PROFILING_COLOR_NAVY    /*nothing*/
#define CONCORE_PROFILING_COLOR_FUCHSIA /*nothing*/
#define CONCORE_PROFILING_COLOR_PURPLE  /*nothing*/

#define CONCORE_PROFILING_SETTHREADNAME(staticName) /*nothing*/

#endif

namespace concore {
inline namespace v1 {

template <class T>
class profiling_lockable_wrapper {
public:
    //! This should always be constructed with a location
    profiling_lockable_wrapper(CONCORE_PROFILING_LOCATION_TYPE loc)
            CONCORE_PROFILING_MUTEX_INIT_CONTEXT(ctx_, loc) {}
    ~profiling_lockable_wrapper() = default;

    //! Disabled copy constructor
    profiling_lockable_wrapper(const profiling_lockable_wrapper&) = delete;
    //! Disabled assignment operator
    profiling_lockable_wrapper& operator=(const profiling_lockable_wrapper&) = delete;

    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    profiling_lockable_wrapper(profiling_lockable_wrapper&&) = default;
    // NOLINTNEXTLINE(performance-noexcept-move-constructor)
    profiling_lockable_wrapper& operator=(profiling_lockable_wrapper&&) = default;

    //! Getter for the base lockable object
    T& base() { return base_; }

    //! Lock the lockable object
    void lock() {
        CONCORE_PROFILING_MUTEX_BEORE_LOCK(ctx_);
        base_.lock();
        CONCORE_PROFILING_MUTEX_AFTER_LOCK(ctx_);
    }

    //! Unlock the lockable object
    void unlock() {
        base_.unlock();
        CONCORE_PROFILING_MUTEX_AFTER_UNLOCK(ctx_);
    }

    //! Try to lock the underlying lockable object; returns true if it was locked
    bool try_lock() {
        const auto acquired = base_.try_lock();
        CONCORE_PROFILING_MUTEX_AFTER_TRY_LOCK(ctx_, acquired);
        return acquired;
    }

    //! For better profiling, allows the user of the lockable to report the location after locking
    void mark_location(CONCORE_PROFILING_LOCATION_TYPE loc) {
        CONCORE_PROFILING_MUTEX_MARK_LOCATION(ctx_, loc);
    }

private:
    //! The underlying lockable object
    T base_;
    //! Optional context, used to implement profiling
    CONCORE_PROFILING_MUTEX_CONTEXT(ctx_);
};

using profiling_mutex = profiling_lockable_wrapper<std::mutex>;

} // namespace v1
} // namespace concore
