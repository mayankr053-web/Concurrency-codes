#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
using namespace std;

//
// ---------- Thread-Safe Queue with built-in finish handling ----------
//
template <typename T>
class ConcurrentQueue {
    queue<T> q;
    mutable mutex m;
    condition_variable cv;
    atomic<bool> finished = false;

public:
    // Push a new value into the queue
    void push(T value) {
        {
            lock_guard<mutex> lock(m);
            q.push(std::move(value));
        }
        cv.notify_one();
    }

    // Pop waits until an item is available or finished is set
    bool pop(T &value) {
        unique_lock<mutex> lock(m);
        cv.wait(lock, [&]() { return !q.empty() || finished.load(); });

        if (finished && q.empty())
            return false;  // stop gracefully

        value = std::move(q.front());
        q.pop();
        return true;
    }

    // Called by main thread when no more items will be added
    void set_finished() {
        finished = true;
        cv.notify_all();  // wake up all waiting threads
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
    function<void()> doWork;
};

class PipelineManager {
    vector<Job> jobs;
    unordered_map<int, vector<int>> adj;   // job -> dependents
    unordered_map<int, int> indegree;      // job -> dependency count
    unordered_map<int, Job*> jobMap;
    ConcurrentQueue<int> ready;            // ready jobs
    atomic<int> done = 0;
    int total = 0;
    mutex indegreeMutex;
    condition_variable cv_done;
    mutex doneMutex;

public:
    PipelineManager(const vector<Job>& jobs, const vector<pair<int,int>>& deps)
        : jobs(jobs)
    {
        for (auto &j : this->jobs)
            jobMap[j.jobId] = &j;

        // Build dependency graph
        for (auto &e : deps) {
            adj[e.first].push_back(e.second);
            indegree[e.second]++;
        }

        // Push jobs with no dependencies
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

        // Wait until all jobs are done
        {
            unique_lock<mutex> lock(doneMutex);
            cv_done.wait(lock, [&]() { return done.load() >= total; });
        }

        // Mark the queue as finished so workers can exit
        ready.set_finished();

        for (auto &t : workers)
            t.join();

        cout << "Pipeline completed successfully!\n";
    }

private:
    void worker() {
        while (true) {
            int jobId;
            if (!ready.pop(jobId)) // queue finished & empty
                return;

            // Execute job
            Job* job = jobMap[jobId];
            job->doWork();

            int finishedCount = ++done;
            if (finishedCount == total) {
                unique_lock<mutex> lock(doneMutex);
                cv_done.notify_all();
            }

            // Unlock dependents safely
            {
                lock_guard<mutex> lock(indegreeMutex);
                for (int dep : adj[jobId]) {
                    indegree[dep]--;
                    if (indegree[dep] == 0)
                        ready.push(dep);
                }
            }
        }
    }
};

//
// ---------- Example Usage ----------
//
int main() {
    vector<Job> jobs = {
        {1, [](){ cout << "Job 1 executed\n"; }},
        {2, [](){ cout << "Job 2 executed\n"; }},
        {3, [](){ cout << "Job 3 executed\n"; }},
        {4, [](){ cout << "Job 4 executed\n"; }},
    };

    // Dependencies: 1 -> 2, 1 -> 3, 2 -> 4, 3 -> 4
    vector<pair<int,int>> deps = {
        {1, 2}, {1, 3}, {2, 4}, {3, 4}
    };

    PipelineManager pm(jobs, deps);
    pm.execute();
}
