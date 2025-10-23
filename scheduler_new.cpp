#include <iostream>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <functional>
#include <atomic>

class ScheduledExecutorService {
public:
    ScheduledExecutorService() : stop(false) {
        worker = std::thread([this]() { run(); });
    }

    ~ScheduledExecutorService() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            stop = true;
        }
        cv.notify_all();
        if (worker.joinable())
            worker.join();
    }

    // schedule(Runnable, delay)
    void schedule(std::function<void()> command, long delayMs) {
        scheduleTask(std::move(command),
                     std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs),
                     0, TaskType::ONE_SHOT);
    }

    // scheduleAtFixedRate(Runnable, initialDelay, period)
    void scheduleAtFixedRate(std::function<void()> command, long initialDelayMs, long periodMs) {
        scheduleTask(std::move(command),
                     std::chrono::steady_clock::now() + std::chrono::milliseconds(initialDelayMs),
                     periodMs, TaskType::FIXED_RATE);
    }

    // scheduleWithFixedDelay(Runnable, initialDelay, delay)
    void scheduleWithFixedDelay(std::function<void()> command, long initialDelayMs, long delayMs) {
        scheduleTask(std::move(command),
                     std::chrono::steady_clock::now() + std::chrono::milliseconds(initialDelayMs),
                     delayMs, TaskType::FIXED_DELAY);
    }

private:
    enum class TaskType { ONE_SHOT, FIXED_RATE, FIXED_DELAY };

    struct Task {
        std::function<void()> func;
        std::chrono::steady_clock::time_point nextRun;
        long intervalMs;
        TaskType type;

        bool operator>(const Task &other) const {
            return nextRun > other.nextRun;
        }
    };

    std::priority_queue<Task, std::vector<Task>, std::greater<>> tasks;
    std::mutex mtx;
    std::condition_variable cv;
    std::thread worker;
    std::atomic<bool> stop;

    // Schedule helper
    void scheduleTask(std::function<void()> func,
                      std::chrono::steady_clock::time_point nextRun,
                      long intervalMs,
                      TaskType type) {
        std::lock_guard<std::mutex> lock(mtx);
        tasks.push(Task{std::move(func), nextRun, intervalMs, type});
        cv.notify_one();
    }

    // Main scheduler loop
    void run() {
        while (true) {
            std::unique_lock<std::mutex> lock(mtx);
            cv.wait(lock, [this]() { return stop || !tasks.empty(); });

            if (stop && tasks.empty())
                break;

            auto now = std::chrono::steady_clock::now();
            auto &next = tasks.top();

            if (now < next.nextRun) {
                cv.wait_until(lock, next.nextRun);
                continue;
            }

            // Ready to execute task
            auto task = next;
            tasks.pop();
            lock.unlock();

            // Execute task outside lock
            task.func();

            // Reschedule if periodic
            if (task.type == TaskType::FIXED_RATE) {
                task.nextRun += std::chrono::milliseconds(task.intervalMs);
                std::lock_guard<std::mutex> reLock(mtx);
                tasks.push(task);
                cv.notify_one();
            } else if (task.type == TaskType::FIXED_DELAY) {
                task.nextRun = std::chrono::steady_clock::now() + std::chrono::milliseconds(task.intervalMs);
                std::lock_guard<std::mutex> reLock(mtx);
                tasks.push(task);
                cv.notify_one();
            }
        }
    }
};
