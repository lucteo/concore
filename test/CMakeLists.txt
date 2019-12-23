# The source files
set(sourceFiles
    "test_main.cpp"
    "smoke.cpp"
)

# Project settings
include_directories(".")
include_directories("../include")

# Add the tests executable
add_executable(test.concore ${sourceFiles})
target_link_libraries(test.concore concore)

# The tests depend on Catch2
find_package(Catch2 REQUIRED)
target_link_libraries(test.concore CONAN_PKG::Catch2)

# Discover the Catch2 test built by the application
include(CTest)
include(Catch)
catch_discover_tests(test.concore)