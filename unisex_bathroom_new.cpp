#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
using namespace std;

struct Person {
    string name;
    char party; // 'D' or 'R'
    int time;   // bathroom time
};

class Bathroom {
    mutex mtx;
    condition_variable cv;
    char current_party = '\0';
    int occupants = 0;
    const int MAX_CAP = 3;

public:
    void enter(const Person &p) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [&]() {
            return (current_party == '\0' || current_party == p.party) && occupants < MAX_CAP;
        });

        if (current_party == '\0')
            current_party = p.party;

        occupants++;
        cout << fixed << setprecision(1)
             << "[" << chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count()
             << "] ENTER: " << p.name << " (" << p.party << ") occupants=" << occupants << endl;
    }

    void leave(const Person &p) {
        unique_lock<mutex> lock(mtx);
        occupants--;
        cout << fixed << setprecision(1)
             << "[" << chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count()
             << "] EXIT:  " << p.name << " (" << p.party << ") remaining=" << occupants << endl;

        if (occupants == 0) {
            current_party = '\0';
            cout << "Bathroom now empty.\n";
            cv.notify_all();
        }
    }
};

class GatedBatchScheduler {
    Bathroom &bathroom;
    queue<Person> demQ, repQ;
    mutex q_mtx;
    condition_variable q_cv;
    bool running = true;
    char last_served = '\0';
    bool gate_open = false;

public:
    GatedBatchScheduler(Bathroom &b) : bathroom(b) {}

    void addPerson(const Person &p) {
        {
            lock_guard<mutex> lock(q_mtx);
            if (p.party == 'D') demQ.push(p);
            else repQ.push(p);
        }
        q_cv.notify_all();
    }

    void stop() {
        running = false;
        q_cv.notify_all();
    }

    void schedule() {
        while (running) {
            vector<Person> batch;
            char next_party;

            {
                unique_lock<mutex> lock(q_mtx);
                // Wait until we have at least one person waiting
                q_cv.wait(lock, [&]() { return !demQ.empty() || !repQ.empty() || !running; });
                if (!running) break;

                // Choose next party (alternate for fairness)
                if (demQ.empty()) next_party = 'R';
                else if (repQ.empty()) next_party = 'D';
                else if (last_served == 'D') next_party = 'R';
                else if (last_served == 'R') next_party = 'D';
                else next_party = 'D'; // default start

                // Gate opens: form a batch with *currently waiting* people only
                gate_open = true;
                queue<Person> &q = (next_party == 'D') ? demQ : repQ;

                for (int i = 0; i < 3 && !q.empty(); ++i) {
                    batch.push_back(q.front());
                    q.pop();
                }
                gate_open = false;
                last_served = next_party;
            }

            // Run the batch (simulate bathroom usage)
            cout << "\n--- Starting batch for party " << last_served << " ---\n";
            vector<thread> workers;
            for (auto &p : batch) {
                workers.emplace_back([this, p]() {
                    bathroom.enter(p);
                    this_thread::sleep_for(chrono::seconds(p.time));
                    bathroom.leave(p);
                });
            }

            for (auto &t : workers) t.join();
            cout << "--- Batch for party " << last_served << " finished ---\n";
        }
    }
};

// ---------- DEMO ----------
int main() {
    Bathroom bathroom;
    GatedBatchScheduler scheduler(bathroom);

    vector<Person> arrivals = {
        {"D1",'D',3}, {"D2",'D',4}, {"R1",'R',5},
        {"R2",'R',3}, {"D3",'D',2}, {"D4",'D',6}, {"R3",'R',4}
    };

    thread sched_thread(&GatedBatchScheduler::schedule, &scheduler);

    // Simulate random arrival intervals (people queueing over time)
    for (auto &p : arrivals) {
        this_thread::sleep_for(chrono::milliseconds(200));
        scheduler.addPerson(p);
        cout << "[ARRIVAL] " << p.name << " (" << p.party << ")\n";
    }

    // Allow time for all to complete
    this_thread::sleep_for(chrono::seconds(40));
    scheduler.stop();

    sched_thread.join();
    cout << "\nAll have used the bathroom.\n";
}
