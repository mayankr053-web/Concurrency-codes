#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>

class LogicalMemoryUnit {
private:
    struct Node {
        std::string data;
        Node* next;
        Node(const std::string& d) : data(d), next(nullptr) {}
    };

    std::unordered_map<int, Node*> pageMap;

    std::string fetchFromPhysical(int pageId) {
        return "Page " + std::to_string(pageId) + " loaded from physical memory";
    }

public:
    // Request for a page
    std::string getPage(int pageId) {
        if (pageMap.find(pageId) != pageMap.end()) {
            // Page already in logical memory
            std::string data = "Accessing " + pageMap[pageId]->data;
            // Add a new node at head (recent access)
            Node* newNode = new Node("Re-accessed: " + std::to_string(pageId));
            newNode->next = pageMap[pageId];
            pageMap[pageId] = newNode;
            return data;
        }

        // Page not in logical memory → fetch from physical memory
        std::string fetchedData = fetchFromPhysical(pageId);
        pageMap[pageId] = new Node(fetchedData);
        return fetchedData;
    }

    // Get k most recent entries of a page
    std::vector<std::string> getKRecent(int pageId, int k) {
        std::vector<std::string> result;
        if (pageMap.find(pageId) == pageMap.end()) return result;

        Node* curr = pageMap[pageId];
        while (curr && k--) {
            result.push_back(curr->data);
            curr = curr->next;
        }
        return result;
    }

    // Optional: destructor to clean up nodes
    ~LogicalMemoryUnit() {
        for (auto& [id, head] : pageMap) {
            Node* curr = head;
            while (curr) {
                Node* temp = curr->next;
                delete curr;
                curr = temp;
            }
        }
    }
};

// ---------------- DEMO ----------------
int main() {
    LogicalMemoryUnit lmu;

    std::cout << lmu.getPage(1) << "\n";
    std::cout << lmu.getPage(2) << "\n";
    std::cout << lmu.getPage(1) << "\n"; // re-access page 1
    std::cout << lmu.getPage(3) << "\n";

    auto recents = lmu.getKRecent(1, 3);
    std::cout << "Recent entries for page 1:\n";
    for (auto& r : recents) std::cout << "  " << r << "\n";
}
