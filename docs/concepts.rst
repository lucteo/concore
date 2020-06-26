Concepts
========

.. role:: cpp_code(code)
   :language: C++

Overview of main concepts:

.. uml:: 
   
    @startuml
    actor "Concurrent application" as conc
    agent Tasks
    agent Serializers
    agent "Task graphs" as Graphs
    agent Executors
    conc --> Tasks: focuses on
    conc --> Serializers: use
    conc --> Graphs: use
    Tasks --> Executors: are executed with
    Tasks --> Serializers: can be constrained with
    Tasks --> Graphs: can be constrained with
    Serializers --> Executors: are based on
    Graphs --> Executors: are based on
    @enduml

Building concurrent applications with concore
---------------------------------------------

Traditionally, applications are using manually specified threads and manual synchronization to
support concurrency. With many occasions this method has been proven to have a set of limitations:

* performance is suboptimal due to synchronization
* understandability is compromised
* thread-safety is often a major issue
* composability is not achieved

**concore** aims at alleviating these issues by implementing a *Task-Oriented Design* model for expressing concurrent applications. Instead of focusing on manual creation of threads and solving synchronization issues, the user should focus on decomposing the application into smaller units of work that can be executed in parallel. If the decomposition is done correctly, the synchronization problems will disappear. Also, assuming there is enough work, the performance of the application can be close-to-optimal (considering throughput). Understandability is also improved as the concurrency is directly visible at the design level.

The main focus of this model is on the design. The users should focus on the design of the concurrent application, and leave the threading concerns to the concore library. This way, building good concurrent applications becomes a far easier job.

Proper design should have two main aspects in mind:

1. the total work needs to be divided into manageable units of work
2. proper constraints need to be placed between these units of work

concore have tools to help with both of these aspects.

For breaking down the total work, there are the following rules of thumb:

* at any time there should be enough unit of works that can be executed; if one has *N* cores on the target system the application should have 2*N units of works ready for execution
* too many units of execution can make the application spend too much time in bookkeeping; i.e., don't create thousands or millions of units of work upfront.
* if the units of work are too small, the overhead of the library can have a higher impact on performance
* if the units of work are too large, the scheduling may be suboptimal
* in practice, a good rule of thumb is to keep as much as possible the tasks between 10ms to 1 second -- but this depends a lot on the type of application being built

For placing the constraints, the user should plan what types of work units can be executed in parallel to what other work units. concore then provides several features to help managing the constraints.

If these are followed, fast, safe and clean concurrent applications can be built with relatively low effort.


Tasks
-----

Instead of using the generic term *work*, concore prefers to use the term *task* defined the following way:

task
    An independent unit of work

The definition of *task* adds emphasis on two aspects of the work: to be a *unit* of work, and to be *independent*.

We use the term *unit of work* instead of *work* to denote an appropriate division of the entire work. As the above rules of thumb stated, the work should not be too small and should not be too big. It should be at the right size, such as dividing it any further will not bring any benefits. Also, the size of a task can be influenced by the relations that it needs to have with other tasks in the application.

The *independent* aspect of the tasks refers to the context of the execution of the work, and the relations with other tasks. Given two tasks *A* and *B*, there can be no constraints between the two tasks, or there can be some kind of execution constraints (e.g., "*A* needs to be executed before *B*", "*A* needs to be executed after *B*", "*A* cannot be executed in parallel with *B*", etc.). If there are no explicit constraints for a task, or if the existing constraints are satisfied at execution time, then the execution of the task should be safe, and not produce undefined behavior. That is, an *independent* unit of work should not depend on anything else but the constraints that are recognized at design time.

Please note that the *independence* of tasks is heavily dependent on design choices, and maybe less on the internals of the work contained in the tasks.

Let us take an example. Let's assume that we have an application with a central data storage. This central data storage has 3 zones with information that can be read or written: *Z1*, *Z2* and *Z3*. One can have tasks that read data, tasks that write data, and tasks that both read and write data. These operations can be specific to a zone or multiple zones in the central data storage. We want to create a task for each of these operations. Then, at design time, we want to impose the following constraints:

* No two tasks that write in the same zone of the data storage can be executed in parallel.
* A task that writes in a zone cannot be executed in parallel with a task that reads from the same zone.
* A task that reads from a zone can be executed in parallel with another task that reads from the same zone (if other rules don't prevent it)
* Tasks in any other combination can be safely executed in parallel

These rules mean that we can execute the following four tasks in parallel: *READ(Z1)*, *READ(Z1, Z3)*, *WRITE(Z2)*, *READ(Z3)*. On the other hand, task *READ(Z1, Z3)* cannot be executed in parallel with *WRITE(Z3)*.

Graphically, we can represent these constraints with lines in a graph that looks like the following:

.. uml:: 
   
    @startuml
    agent "READ(Z1)" as R1
    agent "READ(Z2)" as R2
    agent "READ(Z3)" as R3
    agent "READ(Z1, Z3)" as R13 
    agent "READ(Z1, Z2, Z3)" as R123 
    agent "WRITE(Z1)" as W1
    agent "WRITE(Z2)" as W2
    agent "WRITE(Z3)" as W3
    agent "WRITE(Z2, Z3)" as W23

    R1 -down- W1
    R2 -down- W2
    R3 -down- W3
    R13 -down- W1
    R13 -down- W3
    R123 -down- W1
    R123 -down- W2
    R123 -down- W3

    R2 -down-- W23
    R3 -down-- W23
    R13 -down-- W23
    R123 -down-- W23
    W2 -down- W23
    W3 -down- W23
    @enduml

One can check by looking at the figure what are all the constraints between these tasks.

In general, just like we did with the example above, one can define the constraints in two ways: synthetically (by rules) or by enumerating all the legal/illegal combinations.

In code, concore models the tasks by using the :cpp:class:`concore::v1::task` class. They can be constructed using arbitrary work, given in the form of a :cpp_code:`std::function<void()>`.


Executors
---------

Creating tasks is just declaring the work that needs to be done. There needs to be a way of executing the tasks. In concore, this is done through the *executors*.

executor
    An abstraction that takes a task and schedules its execution, typically at a later time, and maybe with certain constraints.

Concore has defined the following executors:

* :cpp:var:`global_executor <concore::v1::global_executor>`
* :cpp:var:`global_executor_critical_prio <concore::v1::global_executor_critical_prio>`
* :cpp:var:`global_executor_high_prio <concore::v1::global_executor_high_prio>`
* :cpp:var:`global_executor_normal_prio <concore::v1::global_executor_normal_prio>`
* :cpp:var:`global_executor_low_prio <concore::v1::global_executor_low_prio>`
* :cpp:var:`global_executor_background_prio <concore::v1::global_executor_background_prio>`
* :cpp:var:`spawn_executor <concore::v1::spawn_executor>`
* :cpp:var:`spawn_continuation_executor <concore::v1::spawn_continuation_executor>`
* :cpp:var:`immediate_executor <concore::v1::immediate_executor>`
* :cpp:var:`dispatch_executor <concore::v1::dispatch_executor>`
* :cpp:var:`tbb_executor <concore::v1::tbb_executor>`

An executor can always be stored into a :cpp:type:`executor_t <concore::executor_t>` (which is an alias for :cpp_code:`std::function<void(task)>`). In other words, an executor can be thought of as objects that consume tasks.

For most of the cases, using a :cpp:var:`global_executor <concore::v1::global_executor>` is the right choice. This will add the task to a global queue from which concore's worker threads will extract and execute tasks.

Another popular alternative is to use the *spawn* functionality (either as a free function :cpp:func:`spawn() <concore::v1::spawn>`, or through :cpp:var:`spawn_executor <concore::v1::spawn_executor>`). This should be called from within the execution of a task and will add the given task to the local queue of the current worker thread; the thread will try to pick up the last task with priority. If using :cpp:var:`global_executor <concore::v1::global_executor>` favors fairness, :cpp:func:`spawn() <concore::v1::spawn>` favors locality.

Using tasks and executors will allow users to build concurrent programs without worrying about threads and synchronization. But, they would still have to manage constraints and dependencies between the tasks manually. concore offers some features to ease this job.

Task graphs
-----------

Without properly applying constraints between tasks the application will have thread-safety issues. One needs to properly set up the constraints before enqueueing tasks to be executed. One simple way of adding constraints is to add dependencies; that is, to say that certain tasks need to be executed before other tasks. If we chose the encode the application with dependencies the application becomes a directed acyclic graph. For all types of applications, this organization of tasks is possible and it's safe.

Here is an example of how a graph of tasks can look like:

.. uml:: 
   
    @startuml
    left to right direction
    agent T1
    agent T2
    agent T3
    agent T4
    agent T5
    agent T6
    agent T7
    agent T8
    agent T9
    agent T10
    agent T11
    agent T12
    agent T13
    agent T14
    agent T15
    agent T16
    agent T17
    agent T18

    T1 --> T2
    T1 --> T3
    T1 --> T5
    T1 --> T4
    T2 --> T7
    T2 --> T8
    T2 --> T9
    T3 --> T10
    T3 --> T11
    T5 --> T12
    T4 --> T6
    T4 --> T14
    T6 --> T13
    T7 --> T15
    T8 --> T15
    T9 --> T18
    T10 --> T16
    T11 --> T16
    T12 --> T18
    T13 --> T17
    T14 --> T17
    T15 --> T18
    T16 --> T18
    T17 --> T18

    @enduml

Two tasks that don't have a path between them can be executed in parallel.

This graph, as well as any other graph, can be built manually while executing it. One strategy for building the graph is the following:

- tasks that don't have any predecessors or for which all predecessors are completely executed can be enqueued for execution
- tasks that have predecessors that are not run should not be scheduled for execution
- with each completion of a task, other tasks may become candidates for execution: enqueue them
- as the graph is acyclic, in the end, all the tasks in the graph will be executed

Another way of building task graph is to use concore's abstractions. The nodes in the graph can be modeled with :cpp:type:`chained_task <concore::v1::chained_task>` objects. Dependencies can be calling :cpp:func:`add_dependency() <concore::v1::add_dependency>` of :cpp:func:`add_dependencies() <concore::v1::add_dependencies>` functions.


Serializers
-----------

Another way of constructing sound concurrent applications is to apply certain execution patterns for areas in the application that can lead to race conditions. This is analogous to adding mutexes, read-write mutexes, and semaphores in traditional multi-threaded applications.

In the world of tasks, the analogous of a mutex would be a :cpp:type:`serializer <concore::v1::serializer>`. This behaves like an executor. One can enqueue tasks into it, and they would be *serialized*, i.e., executed one at a time.

For example, it can turn 5 arbitrary tasks that are enqueued roughly at the same time into something for which the execution looks like:

.. uml:: 
   
    @startuml
    left to right direction
    agent T1
    agent T2
    agent T3
    agent T4
    agent T5
    T1 --> T2
    T2 --> T3
    T3 --> T4
    T4 --> T5
    @enduml

A serializer will have a waiting list, in which it keeps the tasks that are enqueued while there are tasks that are in execution. As soon as a serializer task finishes a new task is picked up.

Similar to a :cpp:type:`serializer <concore::v1::serializer>`, is an :cpp:type:`n_serializer <concore::v1::n_serializer>`. This corresponds to a semaphore. Instead of allowing only one task to be executing at a given time, this allows *N* tasks to be executed at a given time, but not more.

Finally, corresponding to a read-write mutex, concore offers :cpp:type:`rw_serializer <concore::v1::rw_serializer>`. This is not an executor, but a pair of two executors: one for *READ* tasks and one for *WRITE* tasks. The main idea is that the tasks are scheduled such as the following constraints are satisfied:

- no two *WRITE* tasks can be executed at the same time
- a *WRITE* task and a *READ* tasks cannot be executed at the same time
- multiple *READ* tasks can be executed at the same time, in the absence of *WRITE* tasks 

As working with mutexes, read-write mutexes and semaphores in the traditional multi-threaded applications are covering most of the synchronization cases, the :cpp:type:`serializer <concore::v1::serializer>`, :cpp:type:`rw_serializer <concore::v1::rw_serializer>` and :cpp:type:`n_serializer <concore::v1::n_serializer>` concepts should also cover a large variety of constraints between the tasks.


Others
------

Manually creating constraints
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

One doesn't need concore features like task graphs or serializers to add constraints between the tasks. They can easily be added on top of the existing tasks by some logic at the end of each task.

First, a constraint is something that acts to prevent some tasks to be executed while other tasks are executed. So, most of our logic is added to prevent tasks from executing.

To simplify things, we assume that a task starts executing immediately after it is enqueued; in practice, this does not always happen. Although one can say that implementing constraints based on this assumption is suboptimal, in practice it's not that bad. The assumption is not true when the system is busy; then, the difference between enqueue time and execution time is not something that will degrade the throughput.

Finally, at any point in time we can divide the tasks to be executed in an application in three categories:

#. tasks that can be executed right away; i.e., tasks without constraints
#. tasks that can be executed right away, but they do have constraints with other tasks in this category; i.e., two tasks that want to WRITE at the same memory location, any of them can be executed, but not both of them
#. tasks for which the constraints prevent them to be executed at the current moment; i.e., tasks that depend on other tasks that are not yet finished executing

At any given point, the application can enqueue tasks from the first category, and some tasks from the second category. Enqueueing tasks from the second category must be done atomically with the check of the constraint; also, other tasks that are related to the constraint must be prevented to be enqueued, as part of the same atomic operation.

While the tasks are running without any tasks completing, or starting, the constraints do not change -- we set the constraints between tasks, not between parts of tasks. That is, there is no interest for us to do anything while the system is in steady-state executing tasks. Whenever a new task is created, or whenever we complete a task we need to consider if we can start executing a task, and which one can we execute. At those points, we should evaluate the state of the system to see which tasks belong to the first two categories. Having the tasks placed in these 3 categories we know which tasks can start executing right away -- and we can enqueue these in the system.

If the constraints of the tasks are properly set up, i.e., we don't have circular dependencies, then we are guaranteed to make progress eventually. If we have enough tasks from the first two categories, then we can make progress at each step.

Based on the above description it can be assumed that one needs to evaluate all tasks in the system at every step. That would obviously not be efficient. But, in practice, this can easily be avoided. Tasks don't necessarily need to sit in one big pool and be evaluated each time. They are typically stored in smaller data structures, corresponding to different parts of the application. And, furthermore, most of the time is not needed to check all the tasks in a pool to know which one can be started. In practice evaluating which tasks can be executed can be done really fast. See the serializers above.


Task groups
^^^^^^^^^^^

Task groups can be used to control the execution of tasks, in a very primitive way. When creating a task, the user can specify a :cpp:class:`concore::v1::task_group` object, making the task belong to the task group object passed in.

Task groups are useful for canceling tasks. One can tell the task group to cancel, and all the tasks from the task group are canceled. Tasks that haven't started executed yet will not be executed. Tasks that are in progress can query the cancel flag of the group and decide to finish early.

This is very useful in *shutdown* scenarios when one wants to cancel all the tasks that access an object that needs to be destroyed. One can place all the tasks that operate on that object in a task group and cancel the task group before destroying the object.

Another important feature of task groups is the ability to wait on all the tasks in the group to complete. This is also required to the *shutdown* scenario above. concore does not block the thread while waiting; instead, it tries to execute tasks while waiting. The hope is to help in getting the tasks from the arena done faster.

Note that, while waiting on a group, tasks outside of the group can be executed. That can also mean that waiting takes more time than it needs to. The overall goal of maximizing throughput is still maintained.


Details on the task system
^^^^^^^^^^^^^^^^^^^^^^^^^^

This section briefly describes the most important implementation details of the task system. Understanding these implementation details can help in creating more efficient applications.

If the processor on which the application is run has *N* cores, then concore creates *N* worker threads. Each of these worker threads has a local list of tasks to be executed can execute one task at a given time. That is, the library can only execute a maximum of *N* tasks at the same time. Increasing the number of tasks in parallel will typically not increase the performance, but on the contrary, it can decrease it.

Besides the local list of tasks for each worker, there is a global queue of tasks. Whenever a task is enqueued with :cpp:var:`global_executor <concore::v1::global_executor>` it will reach in this queue. Tasks in this queue will be executed by any of the workers. They are extracted by the workers in the order in which they are enqueued -- this maintains fairness for task execution.

If, from inside a worker thread one calls :cpp:func:`spawn() <concore::v1::spawn>` with a task, that task will be added to the local list of tasks corresponding to the current worker thread. This list of tasks behaves like a stack: last-in-first-out. This way, the local task lists aim to improve locality, as it's assumed that the latest tasks added are *closer* to the current tasks.

A worker thread favors tasks from the local task list to tasks from the global queue. Whenever the local list runs out of tasks, the worker thread tries to get tasks from the central queue. If there are no tasks to get from the central queue, the worker will try to steal tasks from other worker thread's local list. If that fails too, the thread goes to sleep.

When stealing tasks from another worker, the worker is chosen in a round-robin fashion. Also, the first task in the local list is extracted, that is, the furthest task from the currently executing task in that worker. This is also done like that to improve locality.

So far we mentioned that there is only one global queue. There are in fact multiple global queues, one for each priority. Tasks with higher priorities are extracted before the tasks with lower priority, regardless of the order in which they came in.

All the operations related to task extraction are designed to be fast. The library does not traverse all the tasks when choosing the next task to be executed.

Extra concore features
----------------------

Algorithms
^^^^^^^^^^

Concore provices a couple concurrent algorithms that of general use:

* :cpp:func:`conc_for() <concore::v1::conc_for>`
* :cpp:func:`conc_reduce() <concore::v1::conc_reduce>`
* :cpp:func:`conc_scan() <concore::v1::conc_scan>`
* :cpp:func:`conc_sort() <concore::v1::conc_sort>`

A concurrent application typically means much more than transforming some STL algorithms to concurrent algorithms. Moreover, the performance of the concurrent algorithms and the performance gain in multi-threaded environments depends a lot on the type of data they operate on. Therefore, concore doesn't focus on providing an exhaustive list of concurrent algorithms like Parallel STL.

Probably the most useful of the algorithms is :cpp:func:`conc_for() <concore::v1::conc_for>`, which is highly useful for expressing embarrassing parallel problems.

