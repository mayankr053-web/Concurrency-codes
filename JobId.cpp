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
    bool finished = false;

public:
    void push(T value) {
        {
            lock_guard lock(m);
            if (finished) return;
            q.push(std::move(value));
        }
        cv.notify_one();
    }

    bool pop(T &value) {
        unique_lock lock(m);
        cv.wait(lock, [&]() { return !q.empty() || finished; });

        if (finished && q.empty()) return false;
        value = std::move(q.front());
        q.pop();
        return true;
    }

    void set_finished() {
        {
            lock_guard lock(m);
            finished = true;
        }
        cv.notify_all();
    }
};

//
// ---------- Job & Pipeline ----------
//
struct Job {
    int jobId;
    function<void()> doWork;
};

class PipelineManager {
    vector<Job> jobs;
    unordered_map<int, vector<int>> adj;
    unordered_map<int, int> indegree;
    unordered_map<int, Job*> jobMap;
    ConcurrentQueue<int> ready;
    atomic<int> done = 0;
    int total = 0;

    mutex m;
    condition_variable cv;
    exception_ptr exceptionPtr = nullptr;
    atomic<bool> stopAll = false;

public:
    PipelineManager(const vector<Job>& jobs, const vector<pair<int,int>>& deps)
        : jobs(jobs)
    {
        for (auto &j : this->jobs)
            jobMap[j.jobId] = &j;

        for (auto &[u, v] : deps)
            indegree[v]++, adj[u].push_back(v);

        for (auto &j : this->jobs)
            if (!indegree[j.jobId])
                ready.push(j.jobId);

        total = jobs.size();
    }

    void execute() {
        int n = max(2u, thread::hardware_concurrency());
        vector<thread> workers;
        for (int i = 0; i < n; ++i)
            workers.emplace_back(&PipelineManager::worker, this);

        {
            unique_lock lock(m);
            cv.wait(lock, [&]() { return done == total || stopAll; });
        }

        ready.set_finished();
        for (auto &t : workers) t.join();

        if (exceptionPtr) {
            try { rethrow_exception(exceptionPtr); }
            catch (const exception &e) { cerr << "❌ Exception: " << e.what() << endl; }
        } else {
            cout << "✅ Pipeline completed successfully!\n";
        }
    }

private:
    void worker() {
        while (!stopAll) {
            int jobId;
            if (!ready.pop(jobId)) return;

            try {
                jobMap[jobId]->doWork();
                int finished = ++done;
                if (finished == total) {
                    cv.notify_all();
                    return;
                }

                lock_guard lock(m);
                for (int dep : adj[jobId])
                    if (--indegree[dep] == 0)
                        ready.push(dep);
            }
            catch (...) {
                handle_exception();
                return;
            }
        }
    }

    void handle_exception() {
        if (!stopAll.exchange(true)) { // first error
            exceptionPtr = current_exception();
            ready.set_finished();
            cv.notify_all();
        }
    }
};

//
// ---------- Example Usage ----------
//
int main() {
    vector<Job> jobs = {
        {1, [] { cout << "Job 1 executed\n"; }},
        {2, [] { cout << "Job 2 executed\n"; }},
        {3, [] {
            cout << "Job 3 failed!\n";
            throw runtime_error("Error in Job 3");
        }},
        {4, [] { cout << "Job 4 executed\n"; }},
    };

    vector<pair<int,int>> deps = {
        {1,2}, {1,3}, {2,4}, {3,4}
    };

    PipelineManager pm(jobs, deps);
    pm.execute();
}
