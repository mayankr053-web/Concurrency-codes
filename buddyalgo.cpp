#include <iostream>
#include <memory>
#include <unordered_map>
#include <cmath>

class BuddyAllocator {
    struct Node {
        size_t size;       // size of this block (bytes)
        size_t offset;     // start address
        bool free;         // is this block free?
        std::unique_ptr<Node> left, right;

        Node(size_t s, size_t off) : size(s), offset(off), free(true) {}

        bool isLeaf() const { return !left && !right; }
    };

    std::unique_ptr<Node> root;
    size_t totalSize;
    std::unordered_map<size_t, Node*> allocationMap; // offset → allocated node

public:
    explicit BuddyAllocator(size_t total = 16ULL * 1024 * 1024 * 1024)  // 16 GB
        : totalSize(total) {
        root = std::make_unique<Node>(total, 0);
    }

    // Helper: round size to next power of 2
    size_t nextPowerOf2(size_t n) {
        if (n <= 1) return 1;
        size_t p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    // Allocate memory and return offset (address)
    size_t alloc(size_t size) {
        size = nextPowerOf2(size);
        Node* node = allocate(root.get(), size);
        if (!node) {
            std::cerr << "Allocation failed for size " << size << " bytes\n";
            return static_cast<size_t>(-1);
        }
        node->free = false;  // ✅ mark as allocated
        allocationMap[node->offset] = node;
        std::cout << "Allocated block at offset " << node->offset
                  << " size = " << formatSize(node->size) << "\n";
        return node->offset;
    }

    // Free a block
    void free(size_t offset) {
        auto it = allocationMap.find(offset);
        if (it == allocationMap.end()) {
            std::cerr << "Invalid free at offset " << offset << "\n";
            return;
        }

        Node* node = it->second;
        node->free = true;
        allocationMap.erase(it);

        // Merge via DFS
        merge(root.get());

        std::cout << "Freed block at offset " << offset
                  << " (size = " << formatSize(node->size) << ")\n";
    }

    // Print current memory tree
    void print() const {
        std::cout << "\nMemory Tree:\n";
        printTree(root.get(), 0);
    }

private:
    // Recursive allocation logic
    Node* allocate(Node* node, size_t reqSize) {
        if (node->size < reqSize) return nullptr;

        // Perfect fit: allocate here
        if (node->isLeaf() && node->free && node->size == reqSize) {
            node->free = false;
            return node;
        }

        // If leaf but larger than needed, split into halves
        if (node->isLeaf() && node->free && node->size > reqSize) {
            size_t half = node->size / 2;
            node->left = std::make_unique<Node>(half, node->offset);
            node->right = std::make_unique<Node>(half, node->offset + half);
            node->free = false; // now split
        }

        // Recurse left, then right
        if (node->left) {
            Node* leftAlloc = allocate(node->left.get(), reqSize);
            if (leftAlloc) return leftAlloc;
        }
        if (node->right) {
            Node* rightAlloc = allocate(node->right.get(), reqSize);
            if (rightAlloc) return rightAlloc;
        }

        return nullptr;
    }

    // Recursive DFS-based merge
    bool merge(Node* node) {
        if (!node) return false;

        if (node->isLeaf()) {
            return node->free;
        }

        bool leftFree = merge(node->left.get());
        bool rightFree = merge(node->right.get());

        // If both children are free → merge them
        if (leftFree && rightFree) {
            node->left.reset();
            node->right.reset();
            node->free = true;
        } else {
            node->free = false;
        }
        return node->free;
    }

    // Pretty-print tree (for visualization)
    void printTree(const Node* node, int depth) const {
        if (!node) return;
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "|-- [" << formatSize(node->size)
                  << "] offset=" << formatSize(node->offset)
                  << " free=" << (node->free ? "true" : "false") << "\n";
        printTree(node->left.get(), depth + 1);
        printTree(node->right.get(), depth + 1);
    }

    std::string formatSize(size_t bytes) const {
        double gb = bytes / (1024.0 * 1024.0 * 1024.0);
        if (gb >= 1.0) {
            char buf[20];
            sprintf(buf, "%.1fGB", gb);
            return buf;
        }
        double mb = bytes / (1024.0 * 1024.0);
        if (mb >= 1.0) {
            char buf[20];
            sprintf(buf, "%.1fMB", mb);
            return buf;
        }
        double kb = bytes / 1024.0;
        if (kb >= 1.0) {
            char buf[20];
            sprintf(buf, "%.1fKB", kb);
            return buf;
        }
        return std::to_string(bytes) + "B";
    }
};
