.. concore documentation master file, created by
   sphinx-quickstart on Wed Apr  1 23:29:19 2020.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

.. role:: strike
   :class: strike

concore documentation
#####################

`concore` is a C++ library that aims to raise the abstraction level when designing concurrent programs. 
It allows the user to build complex concurrent programs without the need of (blocking) synchronization primitives.
Instead, it allows the user to "describe" the existing concurrency, pushing the planning and execution at the library level.

We strongly believe that the user should focus on describing the concurrency, not fighting synchronization problems.

The library also aims at building highly efficient applications, by trying to maximize the throughput.

*concore* is built around the concept of tasks. A task is an independent unit of work. Tasks can then be executed by so-called *executors*. With these two main concepts, users can construct complex concurrent applications that are safe and efficient.

concore
    concurrency core

    variation on *concord* -- agreement or harmony between :strike:`people` *threads* or groups (of threads); a chord that is pleasing or satisfactory in itself.

Table of content
^^^^^^^^^^^^^^^^

.. toctree::
   :maxdepth: 2

   quickstart
   concepts
   cxx23_executors
   api
