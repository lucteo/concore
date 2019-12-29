# Project settings
include_directories(".")
include_directories("../include")

# The source files for the functional tests
set(func_sourceFiles
    "func/test_main.cpp"
    "func/test_immediate_executor.cpp"
    "func/test_global_executor.cpp"
    "func/test_serializers.cpp"
    "func/test_task_graph.cpp"
    "func/test_dispatch_executor.cpp"
    "func/test_tbb_executor.cpp"
    "func/profiling_helper_test.cpp" # Must be last
)

# Add the functional tests executable
add_executable(test.concore ${func_sourceFiles})
target_link_libraries(test.concore concore)
if (TARGET concore_profiling)
    target_link_libraries(test.concore concore_profiling)
endif()

# The tests depend on Catch2
find_package(Catch2 REQUIRED)
target_link_libraries(test.concore CONAN_PKG::Catch2)

# Discover the Catch2 test built by the application
include(CTest)
include(Catch)
catch_discover_tests(test.concore)

###################
# Performance tests

function(def_perf_test target sourceFile)
    add_executable(${target} ${sourceFile})
    target_link_libraries(${target} concore CONAN_PKG::benchmark)
    if (TARGET concore_profiling)
        target_link_libraries(${target} concore_profiling)
    endif()
endfunction()

def_perf_test(perf.executors "perf/perf_executors.cpp")
def_perf_test(perf.latency "perf/perf_latency.cpp")

