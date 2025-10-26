#include <bits/stdc++.h>
#include <mutex>
#include <thread>
#include <chrono>
using namespace std;

//
// ---------- Simulated Physical Memory ----------
class PhysicalMemory {
public:
    string fetchPage(int pageId) {
        // Simulate slow access
        this_thread::sleep_for(chrono::milliseconds(50));
        return "Data_for_Page_" + to_string(pageId);
    }
};

//
// ---------- Logical Memory Unit (Thread-Safe with Linked List) ----------
class LogicalMemoryUnit {
    struct PageEntry {
        string data;
        chrono::steady_clock::time_point timestamp;
    };

    struct PageData {
        list<PageEntry> versions;   // Most recent first
        mutable mutex mtx;          // Protects this page's linked list
    };

    unordered_map<int, shared_ptr<PageData>> pageTable;
    mutable mutex globalMutex;      // Protects pageTable itself
    PhysicalMemory& physicalMemory;

public:
    LogicalMemoryUnit(PhysicalMemory& pm) : physicalMemory(pm) {}

    //
    // Get or load a page (thread-safe)
    //
    string getPage(int pageId) {
        shared_ptr<PageData> pageData;

        {
            // Lock the map first to find or insert the page entry
            lock_guard<mutex> lock(globalMutex);
            auto it = pageTable.find(pageId);
            if (it != pageTable.end()) {
                pageData = it->second;
            } else {
                // Create entry now (without version)
                pageData = make_shared<PageData>();
                pageTable[pageId] = pageData;
            }
        }

        // Now operate on that page (under page-specific lock)
        lock_guard<mutex> pageLock(pageData->mtx);

        if (pageData->versions.empty()) {
            // Load from physical memory (outside of global lock)
            string data = physicalMemory.fetchPage(pageId);
            pageData->versions.push_front({data, chrono::steady_clock::now()});
        }

        return pageData->versions.front().data;
    }

    //
    // Add new version for a page (update operation)
    //
    void updatePage(int pageId, const string& newData) {
        shared_ptr<PageData> pageData;

        {
            lock_guard<mutex> lock(globalMutex);
            auto it = pageTable.find(pageId);
            if (it == pageTable.end()) {
                pageData = make_shared<PageData>();
                pageTable[pageId] = pageData;
            } else {
                pageData = it->second;
            }
        }

        lock_guard<mutex> pageLock(pageData->mtx);
        pageData->versions.push_front({newData, chrono::steady_clock::now()});
    }

    //
    // Get most recent version of a page
    //
    string getMostRecent(int pageId) const {
        shared_ptr<PageData> pageData;
        {
            lock_guard<mutex> lock(globalMutex);
            auto it = pageTable.find(pageId);
            if (it == pageTable.end())
                throw runtime_error("Page not found");
            pageData = it->second;
        }

        lock_guard<mutex> pageLock(pageData->mtx);
        if (pageData->versions.empty())
            throw runtime_error("Page has no versions");
        return pageData->versions.front().data;
    }

    //
    // Get K most recent versions of a page
    //
    vector<string> getKRecent(int pageId, int k) const {
        shared_ptr<PageData> pageData;
        {
            lock_guard<mutex> lock(globalMutex);
            auto it = pageTable.find(pageId);
            if (it == pageTable.end())
                throw runtime_error("Page not found");
            pageData = it->second;
        }

        lock_guard<mutex> pageLock(pageData->mtx);
        vector<string> result;
        int count = 0;
        for (auto& entry : pageData->versions) {
            if (count++ >= k) break;
            result.push_back(entry.data);
        }
        return result;
    }

    //
    // Debugging utility: Print full memory state
    //
    void printState() const {
        lock_guard<mutex> lock(globalMutex);
        cout << "----- Logical Memory State -----\n";
        for (auto& [pageId, pdata] : pageTable) {
            lock_guard<mutex> pageLock(pdata->mtx);
            cout << "Page " << pageId << ": ";
            for (auto& entry : pdata->versions)
                cout << "[" << entry.data << "] ";
            cout << "\n";
        }
        cout << "--------------------------------\n";
    }
};

//
// ---------- Example Usage ----------
int main() {
    PhysicalMemory pm;
    LogicalMemoryUnit lmu(pm);

    auto cpuTask = [&](int cpuId) {
        for (int pageId = 1; pageId <= 3; ++pageId) {
            string page = lmu.getPage(pageId);
            cout << "CPU-" << cpuId << " got: " << page << "\n";
            if (pageId % 2 == 0) {
                string newData = "Updated_Page_" + to_string(pageId) +
                                 "_by_CPU_" + to_string(cpuId);
                lmu.updatePage(pageId, newData);
            }
        }
    };

    vector<thread> cpus;
    for (int i = 1; i <= 3; ++i)
        cpus.emplace_back(cpuTask, i);

    for (auto& t : cpus)
        t.join();

    cout << "\n";
    lmu.printState();

    // Get 3 recent versions of Page 2
    auto recent = lmu.getKRecent(2, 3);
    cout << "\nRecent versions of Page 2:\n";
    for (auto& r : recent)
        cout << "  " << r << "\n";
}
