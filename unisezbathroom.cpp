#include <bits/stdc++.h>
#include <thread>
#include <mutex>
#include <condition_variable>
using namespace std;

struct Person {
    string name;
    char party;    // 'D' or 'R'
    int duration;  // milliseconds (f(N))
};

// Comparator for min-heap (shortest duration first)
struct Compare {
    bool operator()(const Person& a, const Person& b) const {
        return a.duration > b.duration;
    }
};

class Bathroom {
    mutex m;
    condition_variable cv;

    const int CAP = 3;
    int insideCount = 0;           // how many currently inside
    char turn = '-';               // whose turn: '-', 'D', or 'R'

    // Waiting heaps (min-heap for each party)
    priority_queue<Person, vector<Person>, Compare> demQueue;
    priority_queue<Person, vector<Person>, Compare> repQueue;

public:
    void arrive(Person p) {
        unique_lock<mutex> lock(m);
        // Add to waiting heap
        if (p.party == 'D') demQueue.push(p);
        else repQueue.push(p);

        cv.wait(lock, [&]() {
            // allowed to enter if:
            // - bathroom empty, it's their turn or turn unset
            // - same party inside and capacity < 3
            return ((turn == '-' || turn == p.party) && insideCount < CAP && topOf(p.party) == p.name);
        });

        if (turn == '-') turn = p.party;
        insideCount++;

        // Pop from heap since this person is now inside
        if (p.party == 'D') {
            demQueue.pop();
        } else {
            repQueue.pop();
        }

        cout << p.name << " (" << p.party << ") ENTERED. Inside=" << insideCount 
             << " | Turn=" << turn << "\n";
    }

    void leave(Person p) {
        unique_lock<mutex> lock(m);
        insideCount--;
        cout << p.name << " (" << p.party << ") LEFT. Inside=" << insideCount << "\n";

        if (insideCount == 0) {
            // Flip turn if opposite party waiting
            if (turn == 'D' && !repQueue.empty())
                turn = 'R';
            else if (turn == 'R' && !demQueue.empty())
                turn = 'D';
            else if (demQueue.empty() && repQueue.empty())
                turn = '-'; // bathroom empty, no one waiting
        }

        cv.notify_all();
    }

private:
    string topOf(char party) {
        if (party == 'D') return demQueue.empty() ? "" : demQueue.top().name;
        else return repQueue.empty() ? "" : repQueue.top().name;
    }
};

// Worker thread: simulate a person arriving, using, and leaving
void personThread(Bathroom &bath, Person p) {
    bath.arrive(p);
    this_thread::sleep_for(chrono::milliseconds(p.duration)); // simulate usage
    bath.leave(p);
}

int main() {
    Bathroom bath;

    vector<Person> arrivals = {
        {"D1",'D',400}, {"D2",'D',300}, {"R1",'R',500},
        {"R2",'R',200}, {"D3",'D',100}, {"R3",'R',300},
        {"D4",'D',200}, {"R4",'R',150}
    };

    vector<thread> threads;
    // simulate staggered arrivals
    for (auto &p : arrivals) {
        threads.emplace_back(personThread, ref(bath), p);
        this_thread::sleep_for(chrono::milliseconds(50));
    }

    for (auto &t : threads)
        if (t.joinable()) t.join();

    cout << "\nSimulation finished successfully.\n";
    return 0;
}
