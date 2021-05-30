# The source files for the functional tests
set(func_sourceFiles
    "func/test_main.cpp"
    "func/low_level/test_semaphore.cpp"
    "func/low_level/test_mutexes.cpp"
    "func/low_level/test_concurrent_dequeue.cpp"
    "func/data/test_concurrent_queue.cpp"
    "func/detail/test_worker_tasks.cpp"
    "func/test_inline_executor.cpp"
    "func/test_init.cpp"
    "func/test_global_executor.cpp"
    "func/test_spawn.cpp"
    "func/test_finish_task.cpp"
    "func/test_serializers.cpp"
    "func/test_task_graph.cpp"
    "func/test_task_group.cpp"
    "func/test_wait.cpp"
    "func/test_conc_for.cpp"
    "func/test_conc_reduce.cpp"
    "func/test_conc_scan.cpp"
    "func/test_conc_sort.cpp"
    "func/test_pipeline.cpp"
    "func/test_any_executor.cpp"
    "func/test_dispatch_executor.cpp"
    "func/test_tbb_executor.cpp"
    "func/test_executors_match_concept.cpp"
    "func/test_task_continuation.cpp"

    "func/std/test_thread_pool.cpp"
    "func/std/test_concepts.cpp"
    "func/std/test_concept_wrappers.cpp"
    "func/std/test_cpo_execute.cpp"
    "func/std/test_cpo_start.cpp"
    "func/std/test_cpo_schedule.cpp"
    "func/std/test_cpo_receiver.cpp"
    "func/std/test_cpo_connect.cpp"
    "func/std/test_cpo_submit.cpp"
    "func/std/test_sender_algo.cpp"

    "func/profiling_helper_test.cpp" # Must be last
)

# Add the functional tests executable
add_executable(test.concore ${func_sourceFiles})
target_link_libraries(test.concore concore)
target_include_directories(test.concore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

# The tests depend on catch2, rapidcheck
target_link_libraries(test.concore CONAN_PKG::catch2)
target_link_libraries(test.concore CONAN_PKG::rapidcheck)

# Discover the Catch2 test built by the application
include(CTest)
include(Catch)
catch_discover_tests(test.concore)

# Turn all warnings
target_compile_options(test.concore PUBLIC
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall>
     $<$<CXX_COMPILER_ID:MSVC>:
          /W4>)

set(EXTRA_DEFS "")

# Check for OpenMP
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP was also found on the system -- will be used for testing")
    message(STATUS)
    set(EXTRA_DEFS ${EXTRA_DEFS} CONCORE_USE_OPENMP=1)
endif()

# Check if glm (OpenGL Mathematics) can be found
find_package(glm QUIET)
if(${glm_FOUND})
    find_path(glm_inc_dir NAMES "glm/vec4.hpp" HINTS ${GLM_INCLUDE_DIRS})
    if(EXISTS ${glm_inc_dir})
        message(STATUS "glm was also found on the system -- will be used for testing")
        message(STATUS)
        set(EXTRA_DEFS ${EXTRA_DEFS} CONCORE_USE_GLM=1)
    elseif()
        set(${glm_FOUND} 0)
    endif()
    
endif()


###################
# Performance tests

function(def_perf_test target sourceFile)
    add_executable(${target} ${sourceFile})
    target_link_libraries(${target} concore CONAN_PKG::benchmark)
    target_compile_definitions(concore PUBLIC ${EXTRA_DEFS})
endfunction()

def_perf_test(perf.executors "perf/perf_executors.cpp")
def_perf_test(perf.latency "perf/perf_latency.cpp")
def_perf_test(perf.queue "perf/perf_queue.cpp")
def_perf_test(perf.conc_for "perf/perf_conc_for.cpp")
def_perf_test(perf.conc_reduce "perf/perf_conc_reduce.cpp")
def_perf_test(perf.conc_scan "perf/perf_conc_scan.cpp")
def_perf_test(perf.conc_sort "perf/perf_conc_sort.cpp")

if(${glm_FOUND} AND EXISTS ${glm_inc_dir})
    target_link_libraries(perf.conc_for glm::glm)
    target_include_directories(perf.conc_for PRIVATE ${glm_inc_dir})
endif()
if(${OpenMP_CXX_FOUND})
    target_link_libraries(perf.conc_for OpenMP::OpenMP_CXX)
    target_link_libraries(perf.conc_reduce OpenMP::OpenMP_CXX)
    target_link_libraries(perf.conc_scan OpenMP::OpenMP_CXX)
    target_link_libraries(perf.conc_sort OpenMP::OpenMP_CXX)
endif()

