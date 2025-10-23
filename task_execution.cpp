#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

class Worker {
public:
    explicit Worker(int numThreads) : stop(false), submittedCount(0), completedCount(0) {
        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([this]() { this->workerLoop(); });
        }
    }

    ~Worker() {
        {
            std::unique_lock<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        for (auto &t : threads) t.join();
    }

    // Submit a task for execution (non-blocking)
    void submitWork(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(mtx);
            tasks.push(std::move(task));
            submittedCount++;
        }
        cv.notify_one();
    }

    // Block until all submitted tasks are complete
    void blockUntilComplete() {
        std::unique_lock<std::mutex> lock(doneMtx);
        doneCv.wait(lock, [this]() {
            return completedCount.load() == submittedCount.load();
        });
    }

private:
    std::vector<std::thread> threads;
    std::queue<std::function<void()>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    bool stop;

    std::atomic<int> submittedCount;
    std::atomic<int> completedCount;

    std::mutex doneMtx;
    std::condition_variable doneCv;

    // Each worker thread executes tasks from the queue
    void workerLoop() {
        while (true) {
            std::function<void()> task;

            {
                std::unique_lock<std::mutex> lock(mtx);
                cv.wait(lock, [this]() { return stop || !tasks.empty(); });

                if (stop && tasks.empty()) return;

                task = std::move(tasks.front());
                tasks.pop();
            }

            // Run the task
            try {
                task();
            } catch (...) {
                // ignore task exceptions for simplicity
            }

            // Mark completion
            completedCount++;

            // Notify any waiting blockUntilComplete() calls
            {
                std::lock_guard<std::mutex> lk(doneMtx);
                doneCv.notify_all();
            }
        }
    }
};
