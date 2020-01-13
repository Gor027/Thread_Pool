# Thread_Pool_In_C #
Implementation of Thread_Pool, Runnable and CompletableFuture in C.

### Note ###

The project is an assessed homework from main course subject "Concurrent Programming" in MIM UW,
so the structs are created in headers to be tested automatically by the subject coordinators.

* ANCI C and POSIX compliant
* Simple API for thread_pool, runnable and future usage
* Well tested
* Signal Handling on SIGINT to prevent instant interruption
* Well Tested

Some help from: https://github.com/Pithikos/C-Thread-Pool#run-an-example

## Try to run an example ##

`mkdir build && cd build`

`cmake ..`

`make`

Then run one of the executibles(await.c defer.c main.c) like this:

`./await`

## Usage ##

* Include the headers in your source file: 

`#include "threadpool.h"`

`#include "future.h"`

* Initialize a threadpool with fixed number of threads: thread_pool_init(thread_pool_t *pool, size_t pool_size);

Note that the threadpool may be stack allocated or allocated in the heap, so the semantics of thread_pool_init() is to initialise a threadpool given by its reference. So, the malloced threadpool should be freed by the user to avoid mem leaks.

* Submit a runnable to the threadpool: defer(thread_pool_t *pool, runnable_t runnable);

e.g. `defer(&pool, (runnable_t){.function = your_function,
                            .arg = your_arg,
                            .argsz = your_arg_size});`
                            
* Submit a future to the threadpool: async(thread_pool_t *pool, future *future, callable_t callable);
