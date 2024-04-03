cmake_minimum_required(VERSION 3.17.0)

# Define the concore project
project(concore-dev LANGUAGES CXX)

# Set the version of the project
project(concore-dev VERSION "${CONAN_PACKAGE_VERSION}")

# Print CMake configuration
message(STATUS "System           : ${CMAKE_SYSTEM}")
message(STATUS "System name      : ${CMAKE_SYSTEM_NAME}")
message(STATUS "System ver       : ${CMAKE_SYSTEM_VERSION}")
message(STATUS)

# The main concore library
add_subdirectory(src)

# Prevent linking errors with CXX11 or older ABI (visible when linking with rapidcheck)
target_compile_definitions(concore PUBLIC _GLIBCXX_USE_CXX11_ABI=1)

find_package(Catch2 REQUIRED)
find_package(rapidcheck REQUIRED)
find_package(benchmark REQUIRED)

# Testing code
enable_testing()
add_subdirectory(test)

# Basic CPack support
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_VENDOR "Lucian Radu Teodorescu")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CMAKE_PROJECT_HOMEPAGE_URL "https://github.com/lucteo/concore")
include(CPack)
