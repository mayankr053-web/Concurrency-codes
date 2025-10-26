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
    int time;   // bathroom time in seconds
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
        } else {
            cv.notify_all();
        }
    }
};

class GatedBatchScheduler {
    Bathroom &bathroom;
    mutex q_mtx;
    condition_variable q_cv;
    bool running = true;
    char last_served = '\0';
    bool gate_open = true;

    // Active queues for current batch
    queue<Person> demQ, repQ;
    // Pending queues for next batch
    queue<Person> dem_pending, rep_pending;

public:
    GatedBatchScheduler(Bathroom &b) : bathroom(b) {}

    // Non-blocking addPerson — respects the gate
    void addPerson(const Person &p) {
        {
            lock_guard<mutex> lock(q_mtx);
            if (gate_open) {
                if (p.party == 'D') demQ.push(p);
                else repQ.push(p);
                cout << "[ARRIVAL] " << p.name << " (" << p.party << ") added to ACTIVE queue\n";
            } else {
                if (p.party == 'D') dem_pending.push(p);
                else rep_pending.push(p);
                cout << "[ARRIVAL] " << p.name << " (" << p.party << ") added to PENDING queue\n";
            }
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
                // Wait until we have people to serve
                q_cv.wait(lock, [&]() { return !demQ.empty() || !repQ.empty() || !running; });
                if (!running) break;

                // Choose next party (alternate for fairness)
                if (demQ.empty()) next_party = 'R';
                else if (repQ.empty()) next_party = 'D';
                else if (last_served == 'D') next_party = 'R';
                else if (last_served == 'R') next_party = 'D';
                else next_party = 'D'; // default start

                // Gate closes — new arrivals deferred
                gate_open = false;
                cout << "\n=== GATE CLOSED for party " << next_party << " ===\n";

                queue<Person> &q = (next_party == 'D') ? demQ : repQ;

                for (int i = 0; i < 3 && !q.empty(); ++i) {
                    batch.push_back(q.front());
                    q.pop();
                }
                last_served = next_party;
            }

            // --- Process batch outside lock ---
            cout << "--- Starting batch for party " << last_served << " ---\n";
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

            // --- Batch finished, move pending to active ---
            {
                lock_guard<mutex> lock(q_mtx);
                if (!dem_pending.empty() || !rep_pending.empty()) {
                    while (!dem_pending.empty()) {
                        demQ.push(dem_pending.front());
                        dem_pending.pop();
                    }
                    while (!rep_pending.empty()) {
                        repQ.push(rep_pending.front());
                        rep_pending.pop();
                    }
                }

                gate_open = true;
                cout << "=== GATE OPEN for new arrivals ===\n";
            }
            q_cv.notify_all();
        }
    }
};

// ---------- DEMO ----------
int main() {
    Bathroom bathroom;
    GatedBatchScheduler scheduler(bathroom);

    vector<Person> arrivals = {
        {"D1",'D',3}, {"D2",'D',4}, {"R1",'R',5},
        {"R2",'R',3}, {"D3",'D',2}, {"D4",'D',6}, {"R3",'R',4}, {"R4",'R',3}
    };

    thread sched_thread(&GatedBatchScheduler::schedule, &scheduler);

    // Simulate random arrival intervals (people queueing over time)
    for (auto &p : arrivals) {
        this_thread::sleep_for(chrono::milliseconds(250));
        scheduler.addPerson(p);
    }

    // Allow all to complete
    this_thread::sleep_for(chrono::seconds(40));
    scheduler.stop();

    sched_thread.join();
    cout << "\nAll have used the bathroom.\n";
}
