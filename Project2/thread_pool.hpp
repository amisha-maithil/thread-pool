// thread_pool.hpp
//
// WHAT THIS IS (in plain English):
//   A fixed-size group of "worker" threads that sit waiting for work.
//   You hand it tasks (any callable: a function, a lambda, etc.) via
//   `enqueue()`, and whichever worker is free picks it up next.
//
//   Analogy: 4 chefs (threads), one stack of order tickets (task queue).
//   Chefs grab the next ticket when free; they sleep (not spin/waste CPU)
//   when there's nothing to do.
//
// KEY CONCURRENCY IDEAS USED HERE:
//   - std::mutex: a "lock" so only one thread touches the shared task
//     queue at a time (otherwise two chefs could grab the same ticket).
//   - std::condition_variable: lets sleeping worker threads be woken up
//     efficiently the instant new work arrives, instead of constantly
//     checking "is there work yet?" in a loop (which would waste CPU).
//   - std::future / std::packaged_task: lets the caller get the RESULT
//     of a task back later, even though the task runs on another thread.
//
// This is header-only (no separate .cpp) so it's easy to drop into any
// project: just #include "thread_pool.hpp".

#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

class ThreadPool {
public:
    // Creates the pool and immediately spins up `numThreads` worker
    // threads, each running `workerLoop()` and waiting for tasks.
    explicit ThreadPool(size_t numThreads) : stopping_(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    // Submits a task to the pool and immediately returns a std::future
    // that will hold the task's return value once it actually runs.
    //
    // Example usage:
    //   std::future<int> result = pool.enqueue([] { return 2 + 2; });
    //   int value = result.get(); // blocks until the task finishes
    //
    // Works with ANY callable and ANY argument types, thanks to the
    // template + variadic args below.
    template <typename Func, typename... Args>
    auto enqueue(Func &&f, Args &&...args)
        -> std::future<std::invoke_result_t<Func, Args...>> {
        using ReturnType = std::invoke_result_t<Func, Args...>;

        // packaged_task wraps our function so calling it also fills in
        // a future with the result -- this is how we hand results back
        // across threads safely.
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::bind(std::forward<Func>(f), std::forward<Args>(args)...));

        std::future<ReturnType> result = task->get_future();

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (stopping_) {
                throw std::runtime_error("enqueue called on a stopped ThreadPool");
            }
            // Store as a type-erased std::function<void()> so tasks with
            // different return types can all live in the same queue.
            tasks_.emplace([task] { (*task)(); });
        }

        // Wake up exactly one sleeping worker thread to handle this.
        condition_.notify_one();
        return result;
    }

    // Signals all workers to finish current work and stop, then waits
    // for every thread to actually exit before the pool is destroyed.
    // This is the RAII pattern: cleanup happens automatically, you can
    // never "forget" to shut the pool down properly.
    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            stopping_ = true;
        }
        // Wake up EVERY worker (not just one) so they all notice we're
        // stopping and can exit their loops.
        condition_.notify_all();

        for (std::thread &worker : workers_) {
            worker.join(); // wait for this thread to actually finish
        }
    }

    // Thread pools shouldn't be copied -- copying threads doesn't make
    // sense (which thread would the copy "own"?). Deleting these
    // prevents accidental bugs from an implicit copy.
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;

private:
    // This is the function each worker thread runs, forever, until stopped.
    void workerLoop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(queueMutex_);

                // Sleep here until EITHER there's a task to do OR we've
                // been told to stop. This is efficient: the thread uses
                // zero CPU while waiting, unlike a busy-loop that keeps
                // checking "is there work yet?" over and over.
                condition_.wait(lock, [this] {
                    return stopping_ || !tasks_.empty();
                });

                // If we're stopping AND there's no work left, this
                // worker's job is done -- exit the loop (and thread).
                if (stopping_ && tasks_.empty()) {
                    return;
                }

                task = std::move(tasks_.front());
                tasks_.pop();
            } // lock released here, BEFORE running the task, so other
              // workers can grab the next task while this one runs.

            task(); // actually run the task (outside the lock!)
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    std::mutex queueMutex_;             // protects tasks_ and stopping_
    std::condition_variable condition_; // used to wake sleeping workers
    bool stopping_;
};
