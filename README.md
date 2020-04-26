# concore
Core abstractions for dealing with concurrency in C++

[![CI](https://github.com/lucteo/concore/workflows/CI/badge.svg)](https://github.com/lucteo/concore/actions)
[![codecov](https://codecov.io/gh/lucteo/concore/branch/master/graph/badge.svg)](https://codecov.io/gh/lucteo/concore)
[![Documentation Status](https://readthedocs.org/projects/concore/badge/?version=latest)](https://concore.readthedocs.io/en/latest/?badge=latest)

## About

`concore` is a C++ library that aims to raise the abstraction level when designing concurrent programs. It allows the user to build complex concurrent programs without the need of manually controlling threads and without the need of (blocking) synchronization primitives. Instead, it allows the user to "describe" the existing concurrency, pushing the planning and execution at the library level.

We strongly believe that the user should focus on describing the concurrency, not fighting synchronization problems.

The library also aims at building highly efficient applications, by trying to maximize the throughput.

## Building

The following tools are needed:
* [`conan`](https://www.conan.io/)
* [`CMake`](https://cmake.org/)

Perform the following actions:
```
mkdir -p build
pushd build

conan install .. --build=missing -s build_type=Release

cmake -G<gen> -D CMAKE_BUILD_TYPE=Release -D concore.testing=ON ..
cmake --build .

popd build
```

Here, `<gen>` can be `Ninja`, `make`, `XCode`, `"Visual Studio 15 Win64"`, etc.