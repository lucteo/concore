cmake_minimum_required(VERSION 3.9.0)

# Project information
project(concore LANGUAGES CXX)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

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

# Code Coverage Configuration
add_library(coverage_config INTERFACE)

option(CODE_COVERAGE "Enable coverage reporting" OFF)
if(CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Add required flags (GCC & LLVM/Clang)
    target_compile_options(coverage_config INTERFACE
        -O0        # no optimization
        -g         # generate debug info
        --coverage # sets all required flags
    )
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.13)
        target_link_options(coverage_config INTERFACE --coverage)
    else()
        target_link_libraries(coverage_config INTERFACE --coverage)
    endif()
endif()

# Library options
option(concore.use2020 "Use 2020 preview features" OFF)
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
message(STATUS "Use C++ 2020     : ${concore.use2020}")
message(STATUS "Use testing      : ${concore.testing}")
message(STATUS "Use profiling    : ${concore.profiling}")
message(STATUS "Use Tracy        : ${concore.profiling.tracy}")
message(STATUS "Profiling include: ${concore.profiling.include}")
message(STATUS "Profiling path   : ${concore.profiling.path}")
message(STATUS "Code coverage    : ${CODE_COVERAGE}")
message(STATUS)

# Write the version information to 'version.hpp'
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/concore/version.hpp.in ${CMAKE_CURRENT_SOURCE_DIR}/include/concore/version.hpp)

# The source files for the concore library
set(concore_sourceFiles
    "src/detail/exec_context.cpp"
    "src/low_level/semaphore.cpp"
    "src/init.cpp"
    "src/n_serializer.cpp"
    "src/pipeline.cpp"
    "src/rw_serializer.cpp"
    "src/serializer.cpp"
    "src/task_group.cpp"
    "src/std/thread_pool.cpp"
)

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

# The concore library target
add_library(concore STATIC ${concore_sourceFiles})

# Set the right C++ standard
if (concore.use2020)
    add_definitions(-DUSE_CXX2020=1)
    target_compile_features(concore PUBLIC cxx_std_20)

    # Additional flags to turn on experimental features
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines")
    endif()
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 10.0 AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11.0)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fmodules")
    endif()
else()
    target_compile_features(concore PUBLIC cxx_std_17)
endif()

# Prevent linking errors with CXX11 or older ABI (visible when linking with rapidcheck)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=1")

# Turn all warnings
target_compile_options(concore PRIVATE
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
          -Wall>
     $<$<CXX_COMPILER_ID:MSVC>:
          /W4>)


# Declare the public include directories
target_include_directories(concore PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/>
  $<INSTALL_INTERFACE:include>
)

# Ensure that we link with the threading library
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
target_link_libraries(concore PUBLIC Threads::Threads)

# Do we have to link with profiling?
if (TARGET concore_profiling)
    target_link_libraries(concore PUBLIC concore_profiling)
endif()

# Coverage config
target_link_libraries(concore PUBLIC coverage_config)


# Check if Intel TBB can be found
find_package(tbb QUIET)
if(${TBB_tbb_FOUND})
    message(STATUS "TBB was also found on the system")
    message(STATUS)
    target_link_libraries(concore PUBLIC TBB::tbb)
    add_definitions(-DCONCORE_USE_TBB=1)
endif()

# Add target for clang-format
find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
    add_custom_target(
        clang-format
        COMMAND
            find ../include/ ../src/ ../test/ -name '*.c' -o -name '*.cpp' -o -name '*.h' -o -name '*.hpp'
            | xargs ${CLANG_FORMAT} -i --style=file
        # COMMAND
        #     ${CLANG_FORMAT} --dump-config --style=file
    )
endif()

# Add target for clang-tidy
find_program(CLANG_TIDY "clang-tidy")
if(CLANG_TIDY)
    add_custom_target(
        clang-tidy
        COMMAND
            find ../include/ ../src/ ../test/ -name '*.c' -o -name '*.cpp'
            | xargs ${CLANG_TIDY}
    )
endif()


if(ENABLE_TIDY)
    # Search for clang-tidy and clang-apply-replacements
    find_program(CLANG_TIDY "clang-tidy")

    message(STATUS "Tidy checks are enabled while building")

    set(TIDY_HEADER_FILTER ${CMAKE_SOURCE_DIR}/*)

    # Set extra arguments
    set(TIDY_FIX 1)
    set(TIDY_EXTRA_ARGS -quiet;-format-style=file)
    if (TIDY_FIX)
        set(TIDY_EXTRA_ARGS ${TIDY_EXTRA_ARGS};-fix)
    endif()

    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY};${TIDY_EXTRA_ARGS}"
        CACHE STRING "" FORCE)
else()
    set(CMAKE_CXX_CLANG_TIDY "")
endif()

# Ensure generation of compile_commands.json; needed for clang-tidy
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# If testing is enabled, dive in the corresponding subdirectory
if(concore.testing)
    enable_testing()
    add_subdirectory(test)
endif()

# Targets for installation
install(TARGETS concore
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib)
if(concore.profiling.tracy)
    install(TARGETS concore_profiling
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
endif()
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/concore
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
