cmake_minimum_required(VERSION 3.17.0)

project(concore VERSION 0.6.0 LANGUAGES CXX)

# Set the version and current build date
set(concore_VERSION "${PROJECT_VERSION}")
set(concore_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
string(TIMESTAMP concore_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP concore_BUILD_YEAR "%Y")

# Library options
option(concore.use2020 "Use 2020 preview features" OFF)
option(concore.no_tbb "Don't use TBB, even if found on the system" OFF)
option(concore.profiling "Add profiling info the the library" OFF)
option(concore.profiling.tracy "Use Tracy library for profiling" OFF)
option(concore.profiling.include "The header to include for profiling" "")
option(concore.profiling.path "Path to be used for searching the profiling code" "")

message(STATUS "Library ver      : ${concore_VERSION}")
message(STATUS "Build date       : ${concore_BUILD_DATE}")
message(STATUS "Build year       : ${concore_BUILD_YEAR}")
message(STATUS "Use C++ 2020     : ${concore.use2020}")
message(STATUS "Use profiling    : ${concore.profiling}")
message(STATUS "Use Tracy        : ${concore.profiling.tracy}")
message(STATUS "Profiling include: ${concore.profiling.include}")
message(STATUS "Profiling path   : ${concore.profiling.path}")
message(STATUS)


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

# Set library version
set_target_properties(concore PROPERTIES
                      VERSION "${concore_VERSION}"
                      SOVERSION "${concore_VERSION_MAJOR}")

# Write the version information to 'version.hpp'
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/concore/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/concore/version.hpp)

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

# Check if Intel TBB can be found
if (NOT concore.no_tbb)
    find_package(tbb QUIET)
    if(${TBB_tbb_FOUND})
        message(STATUS "TBB was also found on the system")
        message(STATUS)
        target_compile_definitions(concore PUBLIC CONCORE_USE_TBB=1)
    endif()
endif()

set(TARGETS_TO_INSTALL concore)

# Configure profiling
if(concore.profiling.tracy)
    find_path(TRACY_PATH "Tracy.hpp" PATH "${concore.profiling.path}")
    if(NOT EXISTS "${TRACY_PATH}")
        message(FATAL_ERROR "Cannot find Tracy folder; try setting the concore.profiling.path variable to point to Tracy directory")
    endif()
    message(STATUS "Tracy path   : ${TRACY_PATH}")
    message(STATUS)
    set(concore.profiling ON)
    set(concore.profiling.include "Tracy.hpp")

    # Add a library around TracyClient.cpp
    add_library(concore_profiling STATIC "${TRACY_PATH}/TracyClient.cpp")
    target_compile_features(concore_profiling PRIVATE cxx_std_17)
    target_include_directories(concore_profiling PUBLIC "${TRACY_PATH}")
    target_compile_definitions(concore_profiling PUBLIC TRACY_ENABLE=1)

    target_link_libraries(concore PUBLIC concore_profiling)
    set(TARGETS_TO_INSTALL ${TARGETS_TO_INSTALL} concore_profiling)
endif()

add_library(Concore::concore ALIAS concore)


# Define installation settings for concore
install(TARGETS ${TARGETS_TO_INSTALL} EXPORT concoreTargets
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
        INCLUDES DESTINATION include
)
install(EXPORT concoreTargets
        FILE concoreTargets.cmake
        NAMESPACE Concore::
        DESTINATION lib/cmake/concore
)

# Ensure that all the include files are installed (but not the .in file)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/concore
        DESTINATION include
        USE_SOURCE_PERMISSIONS
        FILES_MATCHING PATTERN "*.hpp"
)

# Write the concoreConfig.cmake and concoreConfigVersion.cmake files
include(CMakePackageConfigHelpers)
write_basic_package_version_file("concoreConfigVersion.cmake"
        VERSION "${concore_VERSION}"
        COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/concoreConfig.cmake
        INSTALL_DESTINATION lib/cmake/concore
)
install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/concoreConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/concoreConfigVersion.cmake"
        DESTINATION lib/cmake/concore
)

