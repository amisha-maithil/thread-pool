![Build Status](https://github.com/amisha-maithil/thread-pool/actions/workflows/build.yml/badge.svg)

# thread-pool

A header-only, general-purpose C++17 thread pool built from scratch, with
zero external dependencies -- just the standard library.

## What it does

Runs a fixed number of worker threads that process a queue of submitted
tasks. Any callable (function, lambda, member function) can be submitted,
and you get a `std::future` back to retrieve its result later:

```cpp
#include "thread_pool.hpp"

ThreadPool pool(4); // 4 worker threads

std::future<int> result = pool.enqueue([] {
    return 2 + 2;
});

std::cout << result.get(); // prints 4, blocks until the task finishes
```

## Why build this instead of just using `std::async`?

`std::async` creates a new OS thread (or reuses one unpredictably) per
call, which has real overhead if you're submitting thousands of small
tasks. A thread pool creates a *fixed* set of threads once, and reuses
them -- the standard pattern used in real concurrent systems (including
automotive middleware, web servers, and game engines) for exactly this
reason.

## How it works (the concurrency primitives)

- **`std::mutex`**: only one thread may touch the shared task queue at a
  time, preventing two workers from grabbing the same task.
- **`std::condition_variable`**: lets idle worker threads sleep with zero
  CPU usage until new work arrives, instead of constantly polling.
- **`std::packaged_task` + `std::future`**: lets a task's return value
  travel safely from the worker thread back to whoever submitted it.
- **RAII shutdown**: the destructor signals all workers to stop and
  `join()`s each thread, so cleanup is automatic and can't be forgotten.

## Build & run

```bash
mkdir build && cd build
cmake ..
make
./thread-pool-demo
```

The demo submits 20 tasks and verifies every result is correct, then runs
a CPU-heavy workload serially vs. in parallel and reports the real speedup
on your machine.

## Verified with ThreadSanitizer

Concurrency bugs (race conditions) are notoriously easy to write and hard
to spot by reading code -- they can pass thousands of test runs and then
fail once, in production, under load. This project has been run under
Clang/GCC's ThreadSanitizer with zero race conditions reported:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
./thread-pool-demo
```

## Known limitations / ideas for extending it

- Tasks run in FIFO order only -- no priority queue. A natural extension:
  add a priority parameter to `enqueue()` and back the queue with a
  `std::priority_queue` instead of `std::queue`.
- No work-stealing between threads -- all workers share one queue, which
  is simple and correct but can bottleneck under very high task counts.
  A work-stealing deque per thread is the next-level version of this.
- Pool size is fixed at construction -- no dynamic resizing.

## Why I built this

To understand thread synchronization primitives at a level deeper than
"I know what a mutex is" -- and to build something genuinely reusable
that models a pattern (bounded worker pools processing a shared queue)
that shows up constantly in real embedded and systems software.
