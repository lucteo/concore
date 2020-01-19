#pragma once

#include "../detail/platform.hpp"

#include <thread>

#if !defined(CONCORE_LOW_LEVEL_SHORT_PAUSE)

// x86 architecture
#if CONCORE_CPU_ARCH_x86_64 || CONCORE_CPU_ARCH_x86_32 || CONCORE_CPU_ARCH_ia64

#if __GNUC__
#define CONCORE_LOW_LEVEL_SHORT_PAUSE_ONE_IMPL() asm("pause;")
#elif _MSC_VER
#include <intrin.h>
#define CONCORE_LOW_LEVEL_SHORT_PAUSE_ONE_IMPL() _mm_pause()
#else
#define CONCORE_LOW_LEVEL_SHORT_PAUSE() std::this_thread::yield()
#endif

// arm architecture
#elif CONCORE_CPU_ARCH_arm

#if __GNUC__
#define CONCORE_LOW_LEVEL_SHORT_PAUSE_ONE_IMPL() __asm__ __volatile__("yield" ::: "memory")
#else
#define CONCORE_LOW_LEVEL_SHORT_PAUSE() std::this_thread::yield()
#endif

// neither x86 nor arm architecture
#else

#define CONCORE_LOW_LEVEL_SHORT_PAUSE() std::this_thread::yield()

#endif
#endif

namespace concore {
namespace detail {

#if !defined(CONCORE_LOW_LEVEL_SHORT_PAUSE)

//! Issue a very short pause instruction to the CPU; try to keep the CPU in low-energy state
inline void short_pause(int count) {
    while (count-- > 0) {
        CONCORE_LOW_LEVEL_SHORT_PAUSE_ONE_IMPL();
    }
}

#define CONCORE_LOW_LEVEL_SHORT_PAUSE(count) concore::detail::short_pause(count)
#endif

#if !defined(CONCORE_LOW_LEVEL_YIELD_PAUSE)

#define CONCORE_LOW_LEVEL_YIELD_PAUSE() std::this_thread::yield()

#endif

} // namespace detail

inline namespace v1 {

/**
 * @brief      Class that can spin with exponential backoff
 *
 * This is intended to be used for implement spin-wait algorithms. It is assumed that the thread
 * that is calling this will wait on some resource from another thread, and the other thread should
 * release that resource shortly. Instead of giving up the CPU quanta, we prefer to spin a bit until
 * we can get the resource
 *
 * This will spin with an exponential long pause; after a given threshold this will just yield the
 * CPU quanta.
 *
 * @see concore::spin_mutex
 */
class spin_backoff {
public:
    //! Pause for a bit; calling this multiple times will pause for longer and longer periods.
    void pause() {
        constexpr int pause_threshold = 16;
        if (count_ < pause_threshold) {
            CONCORE_LOW_LEVEL_SHORT_PAUSE(count_);
            count_ *= 2; // exponential backoff
        } else {
            CONCORE_LOW_LEVEL_YIELD_PAUSE();
        }
    }

private:
    //! The count of 'pause' instructions we should make
    int count_{1};
};

} // namespace v1
} // namespace concore