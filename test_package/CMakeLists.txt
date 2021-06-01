cmake_minimum_required(VERSION 3.15.0)
project(test_package)

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

# Test conan package
add_executable(concore_test_conan test_package.cpp)
target_compile_features(concore_test_conan PUBLIC cxx_std_17)
target_link_libraries(concore_test_conan CONAN_PKG::concore)

# Test cmake target
find_package(concore CONFIG REQUIRED)
find_package(Threads REQUIRED)
add_executable(concore_test_cmake test_package.cpp)
target_link_libraries(concore_test_cmake Concore::concore)
