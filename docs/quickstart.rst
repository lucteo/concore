Quick-start
===========

Building the library
--------------------

The following tools are needed:

* `conan <https://www.conan.io/>`_
* `CMake <https://cmake.org/>`_


Perform the following actions:

.. code-block:: bash

    mkdir -p build
    pushd build

    conan install .. --build=missing -s build_type=Release

    cmake -G<gen> -D CMAKE_BUILD_TYPE=Release -D concore.testing=ON ..
    cmake --build .

    popd build


Here, ``<gen>`` can be ``Ninja``, ``make``, ``XCode``, ``"Visual Studio 15 Win64"``, etc.


Tutorial
--------

TODO

Examples
--------

TODO
