#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <atomic>

class Scheduler {
public:
    Scheduler() : stop(false) {
        worker = std::thread([this]() { run(); });
    }

    ~Scheduler() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();  // Wake worker to exit
        if (worker.joinable())
            worker.join();
    }

    // Schedule a one-time task after delayMs milliseconds
    void scheduleOnce(std::function<void()> func, int delayMs) {
        scheduleTask(std::move(func), delayMs, false);
    }

    // Schedule a recurring task every intervalMs milliseconds
    void scheduleRecurring(std::function<void()> func, int intervalMs) {
        scheduleTask(std::move(func), intervalMs, true);
    }

private:
    struct Task {
        std::function<void()> func;
        std::chrono::steady_clock::time_point nextRun;
        int intervalMs;
        bool recurring;

        bool operator>(const Task &other) const {
            return nextRun > other.nextRun; // Min-heap behavior
        }
    };

    std::priority_queue<Task, std::vector<Task>, std::greater<>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> stop;

    void scheduleTask(std::function<void()> func, int delayMs, bool recurring) {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push(Task{
            std::move(func),
            std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs),
            delayMs,
            recurring
        });
        cv.notify_one();
    }

    void run() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);

            // Wait until there is at least one task or a stop request
            cv.wait(lock, [this]() { return stop || !tasks.empty(); });

            // Exit if stop requested and no remaining tasks
            if (stop && tasks.empty())
                break;

            auto now = std::chrono::steady_clock::now();
            auto &next = tasks.top();

            // If the next task is not yet ready, wait until its scheduled time
            if (now < next.nextRun) {
                cv.wait_until(lock, next.nextRun);
                continue;
            }

            // Ready to run the next task
            auto task = next;
            tasks.pop();
            lock.unlock(); // Unlock while executing task

            task.func(); // Execute task

            // Reschedule recurring tasks
            if (task.recurring) {
                task.nextRun = std::chrono::steady_clock::now() + std::chrono::milliseconds(task.intervalMs);
                std::lock_guard<std::mutex> reLock(mtx);
                tasks.push(task);
                cv.notify_one();
            }
        }
    }
};
