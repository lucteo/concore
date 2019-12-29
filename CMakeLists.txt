cmake_minimum_required(VERSION 3.9.0)

# Project information
project(concore LANGUAGES CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# The concore library target
add_library(concore INTERFACE)

# Get the library version
set(concore_BUILD_VERSION 0)
set(concore_MAJOR_VERSION 0)
set(concore_MINOR_VERSION 0)
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/.git)
    find_package(Git)
    if(GIT_FOUND)
        execute_process(
            COMMAND ${GIT_EXECUTABLE} describe --long --tags --dirty --always
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            OUTPUT_VARIABLE "concore_BUILD_VERSION"
            ERROR_QUIET
            OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
endif()
if(${concore_BUILD_VERSION} MATCHES "v([0-9]+)\\.([0-9]+).*")
    string(REGEX REPLACE
        "v([0-9]+)\\.([0-9]+).*"
        "\\1"
        concore_MAJOR_VERSION ${concore_BUILD_VERSION})
    string(REGEX REPLACE
        "v([0-9]+)\\.([0-9]+).*"
        "\\2"
        concore_MINOR_VERSION ${concore_BUILD_VERSION})
endif()
string(TIMESTAMP concore_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP concore_BUILD_YEAR "%Y")

# Library options
option(concore.testing "Compile the concore tests" ON)
option(concore.profiling "Add profiling info the the library" OFF)
option(concore.profiling.tracy "Use Tracy library for profiling" OFF)
option(concore.profiling.include "The header to include for profiling" "")
option(concore.profiling.path "Path to be used for searching the profiling code" "")

# Print configuration
message(STATUS "System           : ${CMAKE_SYSTEM}")
message(STATUS "System name      : ${CMAKE_SYSTEM_NAME}")
message(STATUS "System ver       : ${CMAKE_SYSTEM_VERSION}")
message(STATUS "Library ver      : ${concore_BUILD_VERSION}")
message(STATUS "  major ver      : ${concore_MAJOR_VERSION}")
message(STATUS "  minor ver      : ${concore_MINOR_VERSION}")
message(STATUS "Build date       : ${concore_BUILD_DATE}")
message(STATUS "Build year       : ${concore_BUILD_YEAR}")
message(STATUS "Use testing      : ${concore.testing}")
message(STATUS "Use profiling    : ${concore.profiling}")
message(STATUS "Use Tracy        : ${concore.profiling.tracy}")
message(STATUS "Profiling include: ${concore.profiling.include}")
message(STATUS "Profiling path   : ${concore.profiling.path}")
message(STATUS)

# Write the version information to 'version.hpp'
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/concore/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/concore/version.hpp)

# Use C++17
target_compile_features(concore INTERFACE cxx_std_17)

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
    add_library(concore_profiling STATIC "${TRACY_PATH}/TracyClient.cpp")
    target_compile_features(concore_profiling PRIVATE cxx_std_17)
    target_include_directories(concore_profiling PUBLIC "${TRACY_PATH}")
    add_definitions(-DTRACY_ENABLE=1)
endif()

# Declare the public include directories
target_include_directories(concore INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:include>
)

# Ensure that we link with the threading library
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
target_link_libraries(concore INTERFACE Threads::Threads)

# Check if Intel TBB can be found
find_package(tbb QUIET)
if(${TBB_tbb_FOUND})
    message(STATUS "TBB was also found on the system")
    message(STATUS)
    target_link_libraries(concore INTERFACE TBB::tbb)
    add_definitions(-DCONCORE_USE_TBB=1)
endif()

# If testing is enabled, dive in the corresponding subdirectory
if(concore.testing)
    enable_testing()
    add_subdirectory(test)
endif()

# Targets for installation
install(TARGETS concore EXPORT concoreTargets)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
  DESTINATION include
  USE_SOURCE_PERMISSIONS
  FILES_MATCHING PATTERN "*.hpp"
)

# Also install cmake helper
# include(CMakePackageConfigHelpers)
# write_basic_package_version_file(
#   ${CMAKE_CURRENT_BINARY_DIR}/concoreConfigVersion.cmake
#   VERSION ${concore_BUILD_VERSION}
#   COMPATIBILITY SameMajorVersion )
# install(EXPORT concoreTargets
#   FILE concoreTargets.cmake
#   NAMESPACE concore::
#   DESTINATION share/cmake/concore
# )
# install(FILES
#   "${concore_BINARY_DIR}/concoreConfigVersion.cmake"
#   DESTINATION share/cmake/concore
# )

# Basic CPack support
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_VENDOR "Lucian Radu Teodorescu")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CMAKE_PROJECT_HOMEPAGE_URL "https://github.com/lucteo/concore")
include(CPack)
