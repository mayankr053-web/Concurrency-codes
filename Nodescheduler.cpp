#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <chrono>
#include <stdexcept>

// ---------- Task structure ----------
struct Task {
    int taskId;
    int cpuCost;

    Task(int id, int cost) : taskId(id), cpuCost(cost) {}
};

// ---------- WorkerNode ----------
class WorkerNode {
public:
    int nodeId;
    std::queue<Task> taskQueue;
    int totalLoad = 0;

    WorkerNode(int id) : nodeId(id) {}

    void addTask(const Task& t) {
        taskQueue.push(t);
        totalLoad += t.cpuCost;
    }

    void runTasks() {
        while (!taskQueue.empty()) {
            Task t = taskQueue.front();
            taskQueue.pop();
            totalLoad -= t.cpuCost;

            std::cout << "[Node " << nodeId << "] Running Task "
                      << t.taskId << " (CPU Cost = " << t.cpuCost << ")\n";

            // Simulate execution
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    void printQueue() const {
        std::queue<Task> copy = taskQueue;
        std::cout << "Node " << nodeId << " -> [ ";
        while (!copy.empty()) {
            std::cout << copy.front().taskId << "(" << copy.front().cpuCost << ") ";
            copy.pop();
        }
        std::cout << "]  Load=" << totalLoad << "\n";
    }
};

// ---------- TaskManager ----------
class TaskManager {
private:
    std::vector<WorkerNode> nodes;

public:
    TaskManager(int n) {
        for (int i = 0; i < n; ++i)
            nodes.emplace_back(i);
    }

    // 1️⃣ Add a task to a node
    void addTask(int nodeId, int taskId, int cpuCost) {
        if (nodeId < 0 || nodeId >= (int)nodes.size())
            throw std::out_of_range("Invalid node ID");

        nodes[nodeId].addTask(Task(taskId, cpuCost));
    }

    // 2️⃣ Print all tasks assigned to a node
    void printTaskQueue(int nodeId) const {
        if (nodeId < 0 || nodeId >= (int)nodes.size())
            throw std::out_of_range("Invalid node ID");

        nodes[nodeId].printQueue();
    }

    // Helper to print all nodes
    void printAllQueues() const {
        std::cout << "\n------ Cluster State ------\n";
        for (auto &n : nodes)
            n.printQueue();
        std::cout << "----------------------------\n";
    }

    // 3️⃣ Run all tasks sequentially (or in parallel if desired)
    void runTask(bool parallel = false) {
        if (!parallel) {
            // Sequential execution node-by-node
            for (auto &n : nodes)
                n.runTasks();
        } else {
            // Parallel execution: one thread per node
            std::vector<std::thread> threads;
            for (auto &n : nodes)
                threads.emplace_back([&n]() { n.runTasks(); });

            for (auto &t : threads)
                if (t.joinable()) t.join();
        }
    }

    // 4️⃣ Rebalance (Reassemble) Tasks
    void reassembleTasks() {
        if (nodes.empty()) return;

        // Step 1: Compute total and average load
        int totalLoad = 0;
        for (auto &n : nodes)
            totalLoad += n.totalLoad;

        int avgLoad = totalLoad / nodes.size();
        if (avgLoad == 0) return;

        // Step 2: Move tasks from high-load to low-load nodes
        for (size_t i = 0; i < nodes.size(); ++i) {
            while (nodes[i].totalLoad > avgLoad) {
                // find a node with lower load
                for (size_t j = 0; j < nodes.size(); ++j) {
                    if (i == j) continue;

                    if (nodes[j].totalLoad < avgLoad && !nodes[i].taskQueue.empty()) {
                        // move one task
                        Task t = nodes[i].taskQueue.front();
                        nodes[i].taskQueue.pop();

                        nodes[i].totalLoad -= t.cpuCost;
                        nodes[j].taskQueue.push(t);
                        nodes[j].totalLoad += t.cpuCost;

                        // stop if balanced enough
                        if (nodes[i].totalLoad <= avgLoad)
                            break;
                    }
                }
            }
        }
    }
};

// ---------- Main (Demo) ----------
int main() {
    TaskManager manager(3);  // 3 worker nodes

    // Add tasks to nodes (deliberately uneven load)
    manager.addTask(0, 101, 10);
    manager.addTask(0, 102, 5);
    manager.addTask(0, 103, 8);
    manager.addTask(1, 201, 4);
    manager.addTask(2, 301, 2);
    manager.addTask(2, 302, 1);

    std::cout << "Initial State:\n";
    manager.printAllQueues();

    // Rebalance the tasks
    std::cout << "\nRebalancing Tasks...\n";
    manager.reassembleTasks();
    manager.printAllQueues();

    // Run all tasks sequentially
    std::cout << "\nRunning Tasks Sequentially...\n";
    manager.runTask(false);
    manager.printAllQueues();

    return 0;
}
