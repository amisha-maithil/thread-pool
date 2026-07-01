// main.cpp
//
// Demonstrates and sanity-checks the ThreadPool:
//   1. Submits many tasks that each return a value, collects results via
//      std::future, and verifies they're all correct.
//   2. Times how long a CPU-heavy workload takes across multiple threads,
//     to show real parallel speedup.

#include "thread_pool.hpp"
#include <chrono>
#include <iostream>
#include <numeric>
#include <vector>

// A deliberately CPU-heavy function, so we can measure real speedup from
// running many of these in parallel instead of one at a time.
static long long slowSquareSum(int n) {
    long long sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += static_cast<long long>(i) * i;
    }
    return sum;
}

int main() {
    const unsigned int hardwareThreads = std::thread::hardware_concurrency();
    const size_t numThreads = hardwareThreads > 0 ? hardwareThreads : 4;
    std::cout << "Creating thread pool with " << numThreads << " threads "
              << "(matches this machine's CPU core count)\n\n";

    // ---- Test 1: correctness -- do we get the right results back? ----
    {
        ThreadPool pool(numThreads);
        std::vector<std::future<int>> futures;

        // Submit 20 tasks, each just squaring a number.
        for (int i = 0; i < 20; ++i) {
            futures.push_back(pool.enqueue([i] { return i * i; }));
        }

        std::cout << "Test 1: correctness check\n";
        bool allCorrect = true;
        for (int i = 0; i < 20; ++i) {
            int result = futures[i].get(); // blocks until that task is done
            int expected = i * i;
            if (result != expected) {
                allCorrect = false;
                std::cout << "  MISMATCH at i=" << i << ": got " << result
                           << ", expected " << expected << "\n";
            }
        }
        std::cout << (allCorrect ? "  All 20 results correct.\n\n"
                                  : "  FAILURES DETECTED.\n\n");
    }

    // ---- Test 2: real speedup from parallelism ----
    {
        const int taskCount = static_cast<int>(numThreads) * 4;
        const int workSize = 20000000;

        std::cout << "Test 2: timing " << taskCount
                   << " CPU-heavy tasks\n";

        // Single-threaded baseline: run everything one at a time.
        auto startSerial = std::chrono::steady_clock::now();
        long long serialTotal = 0;
        for (int i = 0; i < taskCount; ++i) {
            serialTotal += slowSquareSum(workSize);
        }
        auto endSerial = std::chrono::steady_clock::now();
        double serialSeconds =
            std::chrono::duration<double>(endSerial - startSerial).count();

        // Same work, spread across the thread pool.
        auto startParallel = std::chrono::steady_clock::now();
        ThreadPool pool(numThreads);
        std::vector<std::future<long long>> futures;
        for (int i = 0; i < taskCount; ++i) {
            futures.push_back(pool.enqueue(slowSquareSum, workSize));
        }
        long long parallelTotal = 0;
        for (auto &f : futures) {
            parallelTotal += f.get();
        }
        auto endParallel = std::chrono::steady_clock::now();
        double parallelSeconds =
            std::chrono::duration<double>(endParallel - startParallel).count();

        std::cout << "  Serial time:   " << serialSeconds << "s\n";
        std::cout << "  Parallel time: " << parallelSeconds << "s\n";
        std::cout << "  Speedup:       " << (serialSeconds / parallelSeconds)
                   << "x\n";
        std::cout << "  Results match: "
                   << (serialTotal == parallelTotal ? "yes" : "NO -- BUG")
                   << "\n";
    }

    return 0;
}