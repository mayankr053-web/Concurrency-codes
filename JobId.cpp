#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
using namespace std;

//
// ---------- Simple Thread-Safe Queue ----------
//
template <typename T>
class ConcurrentQueue {
    queue<T> q;
    mutable mutex m;
    condition_variable cv;
public:
    void push(T value) {
        {
            lock_guard<mutex> lock(m);
            q.push(std::move(value));
        }
        cv.notify_one();
    }

    // Wait and pop
    bool pop(T &value) {
        unique_lock<mutex> lock(m);
        cv.wait(lock, [&]() { return !q.empty(); });
        value = std::move(q.front());
        q.pop();
        return true;
    }

    bool try_pop(T &value) {
        lock_guard<mutex> lock(m);
        if (q.empty()) return false;
        value = std::move(q.front());
        q.pop();
        return true;
    }

    bool empty() const {
        lock_guard<mutex> lock(m);
        return q.empty();
    }
};

//
// ---------- Job + Pipeline Manager ----------
//
struct Job {
    int jobId;
    function<bool()> doWork; // returns true = success, false = fail
};

class PipelineManager {
    vector<Job> jobs;
    unordered_map<int, vector<int>> adj;   // job -> dependents
    unordered_map<int, int> indegree;      // job -> dependency count
    unordered_map<int, Job*> jobMap;
    ConcurrentQueue<int> ready;            // ready jobs
    atomic<bool> failed = false;
    atomic<int> done = 0;
    int total = 0;
    mutex indegreeMutex;                   // protect indegree map

public:
    PipelineManager(const vector<Job>& jobs, const vector<pair<int,int>>& deps)
        : jobs(jobs) 
    {
        for (auto &j : this->jobs)
            jobMap[j.jobId] = &j;

        // Build graph
        for (auto &e : deps) {
            adj[e.first].push_back(e.second);
            indegree[e.second]++;
        }

        // Jobs with no dependencies are initially ready
        for (auto &j : this->jobs)
            if (indegree[j.jobId] == 0)
                ready.push(j.jobId);

        total = jobs.size();
    }

    void execute() {
        int nThreads = thread::hardware_concurrency();
        if (nThreads == 0) nThreads = 4;

        vector<thread> workers;
        for (int i = 0; i < nThreads; ++i)
            workers.emplace_back(&PipelineManager::worker, this);

        for (auto &t : workers)
            t.join();

        if (failed)
            cout << "Pipeline failed!\n";
        else
            cout << "Pipeline completed successfully!\n";
    }

private:
    void worker() {
        while (true) {
            if (failed) return;
            if (done >= total) return;

            int jobId;
            if (!ready.try_pop(jobId)) {
                // if queue empty but not done yet, short sleep to avoid busy spin
                this_thread::sleep_for(chrono::milliseconds(5));
                continue;
            }

            // Execute job
            Job* job = jobMap[jobId];
            bool ok = job->doWork();
            if (!ok) {
                failed = true;
                return;
            }

            done++;

            // Unlock dependents safely
            {
                lock_guard<mutex> lock(indegreeMutex);
                for (int dep : adj[jobId]) {
                    indegree[dep]--;
                    if (indegree[dep] == 0 && !failed)
                        ready.push(dep);
                }
            }
        }
    }
};
