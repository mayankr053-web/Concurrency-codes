#include <bits/stdc++.h>
using namespace std;

class PlayGround {
    mutex m;
    condition_variable cv;
    queue<int> q;             // FIFO order of teams
    unordered_set<int> s;     // Teams already waiting
    int cur = -1;             // Current team (-1 = empty)
    int count = 0;            // Current players inside

    bool canEnter(int team) {
        if (cur == -1 && (q.empty() || q.front() == team))
            return true;
        if (cur == team && count < 10)
            return true;
        return false;
    }

public:
    void enter(int team, int person) {
        unique_lock<mutex> lock(m);

        // Add team to queue if not already waiting
        if (!s.count(team)) {
            q.push(team);
            s.insert(team);
        }

        // Wait until this team can enter
        cv.wait(lock, [this, team]() { return canEnter(team); });

        // If playground was empty, this team now takes over
        if (cur == -1) {
            cur = team;
            q.pop();
            s.erase(team);
        }

        count++;
        cout << "Player " << person << " from Team " << team
             << " entered. (count=" << count << ")\n";
    }

    void leave(int team, int person) {
        unique_lock<mutex> lock(m);
        count--;
        cout << "Player " << person << " from Team " << team
             << " left. (count=" << count << ")\n";

        // If last player leaves, release playground
        if (count == 0) {
            cur = -1;
            cv.notify_all();
        } else {
            // Notify other waiting players of same team
            cv.notify_all();
        }
    }
};
