# The source files for the concore library
set(concore_sourceFiles
    "lib/detail/exec_context.cpp"
    "lib/low_level/semaphore.cpp"
    "lib/task.cpp"
    "lib/init.cpp"
    "lib/n_serializer.cpp"
    "lib/pipeline.cpp"
    "lib/rw_serializer.cpp"
    "lib/serializer.cpp"
    "lib/task_graph.cpp"
    "lib/task_group.cpp"
    "lib/std/thread_pool.cpp"
)

# The concore library target
add_library(concore STATIC ${concore_sourceFiles})

# Declare the public include directories
target_include_directories(concore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:include>
)

# Set the right C++ standard
if (concore.use2020)
    target_compile_definitions(concore PUBLIC CONCORE_USE_CXX2020=1)
    target_compile_features(concore PUBLIC cxx_std_20)
else()
    target_compile_features(concore PUBLIC cxx_std_17)
endif()

# Ensure that we link with the threading library
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
target_link_libraries(concore PUBLIC Threads::Threads)


# Targets for installation
install(TARGETS concore
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/concore
  DESTINATION include
  USE_SOURCE_PERMISSIONS
  FILES_MATCHING PATTERN "*.hpp"
)
