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

Then run `make test` which will test the threadpool and future libraries using macierz.c and silnia.c also.
Macierz and Silnia are examples how to use future, runnable and threadpool.

### Matrix(Macierz in Polish) ###

Macierz is a program that reads two positive integers R and C which will respectively describe the number of rows and columns of a matrix. Then, program reads R*C lines where on each line there are two numbers V and T, separated by space. Value V on the line i (line counting starts from 0) is the value of the cell on row floor(i/C) and column (i mod C). Counting of columns and rows starts from 0. T is the number of milliseconds that is needed to evaluate the value V. An example of valid input is:

```
2
3
1 2
1 5
12 4
23 9
3 11
7 2
```

This input data will generate matrix with two rows and three columns:

```
|  1  1 12 |
| 23  3  7 |
```
Program creates a threadpool with 4 threads and calculates the row sums of the matrix, where on each cell the program waits until the value of the cell is evaluated(for example, for value 3 the program waits 11 milliseconds until value is evaluated). After the calculations the program writes the results on the stdout, one by one for each row. For the example above included in a file `data1.dat` executing:

`cat data1.dat | ./macierz`

should write on the stdout the following lines:

```
14
33
```

### Factorial(Silnia in Polish) ###

The program silnia.c should read a single number n from the standard input, and then calculate the number n! using a threadpool of 3 threads. After calculating this number, the result should be printed to standard output. The program should calculate the factorial using the function `map` and passing it into `future_value` partial products. For example, the call:

`echo 5 | ./silnia`

should write on the stdout

`120`

## Usage ##

* Include the headers in your source file: 

`#include "threadpool.h"`

`#include "future.h"`

* Initialize a threadpool with fixed number of threads: `thread_pool_init(thread_pool_t *pool, size_t pool_size);`

Note that the threadpool may be stack allocated or allocated in the heap, so the semantics of thread_pool_init() is to initialise a threadpool given by its reference. So, the malloced threadpool should be freed by the user to avoid mem leaks.

* Submit a runnable to the threadpool: `defer(thread_pool_t *pool, runnable_t runnable);`

e.g. `defer(&pool, (runnable_t){.function = your_function,
                            .arg = your_arg,
                            .argsz = your_arg_size});`
                            
* Submit a future to the threadpool: `async(thread_pool_t *pool, future *future, callable_t callable);`
* Creating new future based on another future and a new_function: `map(pool, mapped_value, future_value, new_function);`
* Wait until the future result is ready and returned: `void *result = await(future_value);`

The worker threads will start their work after there is a new work on the threadpool. If you want to destroy the threadpool, it will wait until all the jobs are done and will destroy the threadpool. To destroy the pool just use `thread_pool_destroy(thread_pool_t *pool)`. The library also handles signal SIGINT as follows:

* After receiving signal SIGINT, blocks the user to submit new tasks to the running threadpools,
* Completes all the calculations submitted to current running pools,
* In the end, destroys the working threadpools. (Note that it may not end the program after handling SIGINT...)


###For fast practical overview a table showing all functions with their descriptions will be added...###
