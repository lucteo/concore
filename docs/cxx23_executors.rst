C++23 executors
===============

.. role:: cpp_code(code)
   :language: C++

.. |br| raw:: html

  <br/>

.. |ref_task| replace:: :cpp:class:`task <concore::v1::task>`


`concore` implements C++23 executors, as defined by `P0443: A Unified Executors Proposal for C++ <http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p0443r14.html>`_. It is not a complete implementation, but the main features are present. Concore's implementation includes:

- concepts
- customization point objects
- thread pool
- type wrappers

However, it does not include:

- properties and requirements -- they seem too complicated to be actually needed
- extra conditions for customization point object behavior; i.e., a scheduler does not automatically become a sender -- the design for this is too messy, with too many circular dependencies


Concepts and customization-point objects
----------------------------------------

.. uml:: 
   
    @startuml
    scale 0.8

    interface connect as "concore::connect"
    interface start as "concore::start"
    interface submit as "concore::submit"
    interface schedule as "concore::schedule"
    interface set_value as "concore::set_value"
    interface set_done as "concore::set_done"
    interface set_error as "concore::set_error"
    interface execute as "concore::execute"
    interface bulk_execute as "concore::bulk_execute"

    agent receiver
    agent receiver_of
    agent operation_state
    agent sender
    agent typed_sender
    agent sender_to
    agent scheduler
    agent executor
    agent executor_of

    skinparam rectangle {
      backgroundColor transparent
      borderColor transparent
    }

    executor -d-> execute
    executor_of -d-> execute

    connect -d-> receiver

    submit -d-> sender_to

    receiver -d-> set_done
    receiver -d-> set_error

    receiver_of -d-> receiver
    receiver_of -d--> set_value

    operation_state -d-> start

    typed_sender -d-> sender

    sender_to -d-> sender
    sender_to -d-> receiver
    sender_to -d-> connect

    scheduler -d-> schedule

    schedule -d-> sender

    connect -d-> operation_state
    connect -d-> sender

    scheduler .d.> executor
    start .d.> executor
    start .d.> executor_of

    execute -[hidden]- bulk_execute


    legend
    <b>Legend</b>
    | <b>Element<b> | <b>Description<b> |
    | circle | customization point object |
    | rectangle | concept |
    | solid arrow | dependency |
    | dotted arrow | logical/assumed dependency |
    endlegend

    @enduml

The following table lists the customization-point objects (CPOs) defined:

==================================  ===========
CPO                                 Description
==================================  ===========
``void set_value(R&&, Vs&&...)``    Given a receiver ``R``, signals that the sender operation has completed |br|
                                    (with zero or more values)
``void set_done(R&&)``              Given a receiver ``R``, signals that the sender operation was canceled
``void set_error(R&&, E&&)``        Given a receiver ``R``, signals that the sender operation has an error
``void execute(E&&, F&&)``          Executes a functor in an executor
``auto connect(S&&, R&&)``          Connects the given sender with the given receiver, resulting an |br|
                                    ``operation_state`` object
``void start(O&&)``                 Starts an ``operation_state`` object
``void submit(S&&, R&&)``           Submit a sender and a receiver for execution
``auto schedule(S&&)``              Given a scheduler, returns a sender that can kick-off a chain of |br|
                                    processing
``void bulk_execute(E&&, F&&, N)``  Bulk-executes a functor N times in the context of an executor.
==================================  ===========


The following table lists the concepts defined:

=========================================== ===========
Concept                                     Description
=========================================== ===========
``executor<E>``                             Indicates that the given type can execute work of type |br|
                                            ``void()``. It has a corresponding ``execute()`` CPO defined.
``executor_of<E, F>``                       Indicates that the given type can execute work of the given |br|
                                            type. It has a corresponding ``execute()`` CPO defined.
``receiver<R, E=exception_ptr>``            Indicates that the given type is a bare-bone receiver. That is, |br|
                                            it supports ``set_done`` and ``set_error`` (with the given |br|
                                            error type)
``receiver_of<R, E=exception_ptr, Vs...>``  Indicates that the given type is a receiver. That is, it supports |br|
                                            ``set_done`` and ``set_error`` (with the given error type) and |br|
                                            ``set_value`` with the given values types
``sender<S>``                               Indicates that the given type is a sender.
``typed_sender<S>``                         Indicates that the given type is a typed sender.
``sender_to<S, R>``                         Indicates that the given type ``S`` is a sender compatible with |br|
                                            the given receiver type. That is ``connect(S, R)`` is valid.
``operation_state<O>``                      Indicates that the given type is an operation state. |br|
                                            That is ``start(O)`` is valid.
``scheduler<S>``                            Indicate that the given type is a scheduler. That is |br|
                                            ``schedule(S)`` is valid and returns a valid sender type.
=========================================== ===========

Concepts, explained
-------------------

executor
^^^^^^^^

A C++23 ``executor`` concept matches the way concore looks at an executor: it is able to schedule work. To be noted that all concore executors (:cpp:type:`global_executor <concore::v1::global_executor>`, :cpp:type:`spawn_executor <concore::v1::spawn_executor>`, :cpp:type:`inline_executor <concore::v1::inline_executor>`, etc.) fulfill the ``executor`` concept.

The way that P0443 defines the concept, an ``executor`` is able to execute any type of functor compatible with ``void()``. While a |ref_task| is a type compatible with ``void()``, concore ensures that all the executors have a specialization that takes directly |ref_task|. This is done mostly for type erasure, helping compilation times.

If ``ex`` is of type ``E`` that models concept ``executor``, then the one can perform work on that executor with a code similar to:

.. code-block:: C++

    concore::execute(ex, [](){ do_work(); });


operation_state
^^^^^^^^^^^^^^^

An ``operation_state`` object is essentially a pair between an ``executor`` object and a |ref_task| object.


Given an operation ``op`` of type ``Oper``, one can start executing it with a code like:

.. code-block:: C++

    concore::start(op);

An operation is typically obtained from a ``sender`` object and a ``receiver`` object by calling the ``connect`` CPO:

.. code-block:: C++

    operation_state auto op = concore::connect(snd, recv);

scheduler, sender
^^^^^^^^^^^^^^^^^

A ``scheduler`` is an agent that is capable of starting asynchronous computations. Most often a scheduler is created out of an ``executor`` object, but there is no direct linkage between the two.

A ``scheduler`` object can start asynchronous computations by creating a ``sender`` object. Given a ``sched`` object that matches the z``scheduler`` concept, then one can obtain a sender in the following way:

.. code-block:: C++

    sender auto snd = concore::schedule(sched);

A ``sender`` object is an object that performs some asynchronous operation in a given execution context. To use a ``sender``, one must always pair it with are ``receiver``, so that somebody knows about the operation being completed. As shown, above, this pairing can be done with the ``connect`` function. Thus, putting them all together, one one can start a computation from a ``scheduler`` if there is a ``receiver`` object to collect the results, as shown below:

.. code-block:: C++

    receiver recv = ...
    sender auto snd = concore::schedule(sched);
    operation_state auto op = concore::connect(snd, recv);
    concore::start(op);


To skip the intermediate step of creating an ``operation_state``, one might call ``submit``, that essentially combines ``connect`` and ``start``:

.. code-block:: C++

    receiver recv = ...
    sender auto snd = concore::schedule(sched);
    concore::submit(snd, recv);

Also, a ``sender`` can be directly created from an ``executor`` by using the ``as_sender`` type wrapper:

.. code-block:: C++

    receiver recv = ...
    sender auto snd = concore::as_sender(ex);
    concore::submit(snd, recv);


receiver
^^^^^^^^

A ``receiver`` is the continuation of an asynchronous task. It is aways used to consume the results of a ``sender``.

One can create a receiver from an invocable object by using the ``as_receiver`` wrapper:

.. code-block:: C++

    auto my_fun = []() { on_task_done(); }
    receiver recv = concore::as_receiver(my_fun)

The division between a ``receiver`` and a ``sender`` is a bit blurry. One can add computations that need to be executed asynchronously in any of them. Moreover, one can construct objects that are both ``receiver`` and ``sender`` at the same type. This is useful to create chains of computations.

Type wrappers
-------------

The following diagrams shows the type wrappers and how they transform different types of objects:

.. uml:: 
   
    @startuml
    scale 0.8

    usecase as_receiver
    usecase as_invocable
    usecase as_operation
    usecase as_sender

    agent receiver
    agent operation_state
    agent sender
    agent executor
    agent "std::invocable" as invocable

    skinparam rectangle {
      backgroundColor transparent
      borderColor transparent
    }

    invocable --> as_receiver
    as_receiver --> receiver

    receiver --> as_invocable
    as_invocable --> invocable

    as_receiver -[hidden]- as_invocable

    executor --> as_operation
    receiver --> as_operation
    as_operation --> operation_state

    executor --> as_sender
    as_sender --> sender

    legend
    <b>Legend</b>
    | <b>Element<b> | <b>Description<b> |
    | rectangle | concept |
    | oval | type wrapper |
    | arrow | input or output of a type wrapper |
    endlegend

    @enduml
