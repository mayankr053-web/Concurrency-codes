#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <chrono>
using namespace std;

// -----------------------------------------------
// Data structure representing a player
// -----------------------------------------------
struct Player {
    string name;
    int team;         // team id (1, 2, 3, ...)
    int play_time;    // time to stay in playground (seconds)
};

// -----------------------------------------------
// Playground class: controls access to the shared resource
// -----------------------------------------------
class Playground {
    mutex mtx;
    condition_variable cv;
    int current_team = -1;   // -1 means empty
    int inside_count = 0;    // number of players currently inside
    const int MAX_CAP = 10;  // max players per team at once

public:
    void enter(const Player &p) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [&]() {
            return (current_team == -1 || current_team == p.team) && inside_count < MAX_CAP;
        });

        if (current_team == -1)
            current_team = p.team;

        inside_count++;
        cout << fixed << setprecision(1)
             << "[" << chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count()
             << "] ENTER: " << p.name << " (Team " << p.team
             << ") inside=" << inside_count << endl;
    }

    void leave(const Player &p) {
        unique_lock<mutex> lock(mtx);
        inside_count--;
        cout << fixed << setprecision(1)
             << "[" << chrono::duration<double>(chrono::steady_clock::now().time_since_epoch()).count()
             << "] EXIT : " << p.name << " (Team " << p.team
             << ") remaining=" << inside_count << endl;

        if (inside_count == 0) {
            current_team = -1;
            cout << "Playground now empty.\n";
            cv.notify_all();
        } else {
            cv.notify_all(); // notify teammates waiting for a slot
        }
    }
};

// -----------------------------------------------
// GatedBatchScheduler class: handles fairness and batching
// -----------------------------------------------
class GatedBatchScheduler {
    Playground &playground;
    mutex q_mtx;
    condition_variable q_cv;
    bool running = true;
    bool gate_open = true;

    queue<int> team_order;   // FIFO of waiting teams
    unordered_map<int, queue<Player>> team_queues;   // active waiting players
    unordered_map<int, queue<Player>> pending_queues; // players who arrive when gate is closed

public:
    GatedBatchScheduler(Playground &pg) : playground(pg) {}

    // Non-blocking: immediately queues the player
    void addPlayer(const Player &p) {
        lock_guard<mutex> lock(q_mtx);

        if (gate_open) {
            // Gate open → add to active queue
            if (team_queues[p.team].empty())
                team_order.push(p.team);
            team_queues[p.team].push(p);
            cout << "[ARRIVAL] " << p.name << " (Team " << p.team << ") → active queue\n";
        } else {
            // Gate closed → add to pending queue (for next batch)
            pending_queues[p.team].push(p);
            cout << "[ARRIVAL] " << p.name << " (Team " << p.team << ") → pending queue (gate closed)\n";
        }

        q_cv.notify_all();
    }

    void stop() {
        running = false;
        q_cv.notify_all();
    }

    // Main scheduling loop
    void schedule() {
        while (running) {
            vector<Player> batch;
            int next_team = -1;

            {
                unique_lock<mutex> lock(q_mtx);
                q_cv.wait(lock, [&]() {
                    return !team_order.empty() || !running;
                });
                if (!running) break;

                next_team = team_order.front();
                team_order.pop();

                // Close gate (no new arrivals added to active queues)
                gate_open = false;
                cout << "\n=== GATE CLOSED for Team " << next_team << " ===\n";

                // Form batch (up to 10 players)
                queue<Player> &teamQ = team_queues[next_team];
                for (int i = 0; i < 10 && !teamQ.empty(); ++i) {
                    batch.push_back(teamQ.front());
                    teamQ.pop();
                }

                // Remove team if no more waiting players
                if (teamQ.empty())
                    team_queues.erase(next_team);
            }

            // Run the batch outside the lock
            cout << "--- Starting batch for Team " << next_team << " ---\n";
            vector<thread> workers;
            for (auto &p : batch) {
                workers.emplace_back([this, p]() {
                    playground.enter(p);
                    this_thread::sleep_for(chrono::seconds(p.play_time));
                    playground.leave(p);
                });
            }
            for (auto &t : workers) t.join();
            cout << "--- Batch for Team " << next_team << " finished ---\n";

            // After batch ends, move pending arrivals to active queues and open gate
            {
                lock_guard<mutex> lock(q_mtx);

                // Promote all pending teams to active
                for (auto &[team, pq] : pending_queues) {
                    if (team_queues[team].empty())
                        team_order.push(team);
                    while (!pq.empty()) {
                        team_queues[team].push(pq.front());
                        pq.pop();
                    }
                }
                pending_queues.clear();

                gate_open = true;
                cout << "=== GATE OPEN for new teams ===\n";
            }

            q_cv.notify_all();
        }
    }
};

// -----------------------------------------------
// Simulation driver
// -----------------------------------------------
int main() {
    Playground playground;
    GatedBatchScheduler scheduler(playground);

    vector<Player> arrivals = {
        {"A1", 1, 3}, {"A2", 1, 2}, {"B1", 2, 4}, {"C1", 3, 3},
        {"A3", 1, 3}, {"B2", 2, 2}, {"C2", 3, 4}, {"B3", 2, 3},
        {"A4", 1, 2}, {"C3", 3, 5}
    };

    thread sched_thread(&GatedBatchScheduler::schedule, &scheduler);

    // Simulate players arriving over time
    for (auto &p : arrivals) {
        this_thread::sleep_for(chrono::milliseconds(300 + rand() % 400));
        scheduler.addPlayer(p);
    }

    // Wait for completion
    this_thread::sleep_for(chrono::seconds(60));
    scheduler.stop();
    sched_thread.join();

    cout << "\nAll players have used the playground.\n";
    return 0;
}
