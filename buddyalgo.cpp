#include <iostream>
#include <memory>
#include <cmath>
#include <limits>

class BuddyAllocator {
public:
    static constexpr unsigned long long TOTAL_MEMORY = 16ull * 1024 * 1024 * 1024; // 16 GB
    static constexpr unsigned long long MIN_BLOCK = 1024ull; // 1 KB

    BuddyAllocator() {
        root = std::make_unique<Node>(TOTAL_MEMORY, 0);
    }

    // API required by prompt:
    // returns offset as int (returns -1 on failure or overflow)
    int alloc(size_t size) {
        // Round request up to at least MIN_BLOCK and power of two
        unsigned long long req = std::max<unsigned long long>(size, MIN_BLOCK);
        unsigned long long allocSize = nextPowerOfTwo(req);
        Node* node = allocateDFS(root.get(), allocSize);
        if (!node) return -1;

        // node->isFree already set to false by allocateDFS
        unsigned long long off = node->offset;
        if (off > static_cast<unsigned long long>(std::numeric_limits<int>::max()))
            return -1; // can't return offset in int safely
        return static_cast<int>(off);
    }

    // free the block located at the given index (offset)
    void free(int index) {
        if (index < 0) {
            std::cerr << "free: invalid index\n";
            return;
        }
        unsigned long long off = static_cast<unsigned long long>(index);
        bool freed = freeDFS(root.get(), off);
        if (!freed) {
            std::cerr << "free: no allocated block found at offset " << off << "\n";
        }
    }

    // Debug print function to visualize tree (optional)
    void dump() const {
        dumpDFS(root.get(), 0);
    }

private:
    struct Node {
        unsigned long long size;
        unsigned long long offset;
        bool isFree; // valid only for leaf nodes (no children)
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        Node(unsigned long long s, unsigned long long o)
            : size(s), offset(o), isFree(true), left(nullptr), right(nullptr) {}
    };

    std::unique_ptr<Node> root;

    static unsigned long long nextPowerOfTwo(unsigned long long n) {
        if (n == 0) return 1;
        // if already power of two
        if ((n & (n - 1)) == 0) return n;
        unsigned long long p = 1;
        while (p < n) p <<= 1;
        return p;
    }

    // allocate DFS: returns pointer to leaf node allocated, or nullptr
    Node* allocateDFS(Node* node, unsigned long long reqSize) {
        if (!node) return nullptr;

        // If node is leaf
        if (!node->left && !node->right) {
            if (!node->isFree) return nullptr;         // already used
            if (node->size < reqSize) return nullptr; // too small

            if (node->size == reqSize) {
                node->isFree = false; // allocate this leaf
                return node;
            }

            // need to split
            unsigned long long half = node->size / 2;
            node->left  = std::make_unique<Node>(half, node->offset);
            node->right = std::make_unique<Node>(half, node->offset + half);
            // after splitting, leaf state is represented by children existence
            // both children initially free
        }

        // node is split (has children) -> try left then right
        Node* res = allocateDFS(node->left.get(), reqSize);
        if (res) return res;
        res = allocateDFS(node->right.get(), reqSize);

        // After attempting children, if both children are free leaves, we could mark parent free,
        // but parent being split is signified by children existing; keep them until merges happen on free.
        return res;
    }

    // freeDFS: returns true if a block was freed (and merges done as necessary)
    bool freeDFS(Node* node, unsigned long long off) {
        if (!node) return false;

        // If leaf: check offset & allocated state
        if (!node->left && !node->right) {
            if (!node->isFree && node->offset == off) {
                node->isFree = true;
                return true;
            }
            return false;
        }

        // Non-leaf: search children
        bool freed = freeDFS(node->left.get(), off) || freeDFS(node->right.get(), off);

        if (freed) {
            // After freeing in children, if both children exist and are leaves and free -> merge
            if (node->left && node->right) {
                if (!node->left->left && !node->left->right && node->left->isFree &&
                    !node->right->left && !node->right->right && node->right->isFree) {
                    // delete children to merge
                    node->left.reset();
                    node->right.reset();
                    node->isFree = true; // parent becomes a free leaf
                }
            }
        }
        return freed;
    }

    void dumpDFS(const Node* node, int depth) const {
        if (!node) return;
        for (int i = 0; i < depth; ++i) std::cout << "  ";
        std::cout << "[" << node->offset << ", size=" << node->size
                  << ", leaf=" << (!node->left && !node->right)
                  << ", free=" << node->isFree << "]\n";
        if (node->left) dumpDFS(node->left.get(), depth + 1);
        if (node->right) dumpDFS(node->right.get(), depth + 1);
    }
};

// ----------------- Simple demo -----------------
int main() {
    BuddyAllocator alloc;

    // Allocate 3GB -> rounds to 4GB
    int a = alloc.alloc(3ull * 1024 * 1024 * 1024);
    std::cout << "alloc 3GB -> offset: " << a << "\n";

    // Allocate 2GB -> rounds to 2GB
    int b = alloc.alloc(2ull * 1024 * 1024 * 1024);
    std::cout << "alloc 2GB -> offset: " << b << "\n";

    // Dump tree
    std::cout << "\nTree after allocations:\n";
    alloc.dump();

    // Free the first block
    alloc.free(a);
    std::cout << "\nTree after freeing first block:\n";
    alloc.dump();

    // Allocate 1GB -> rounds to 1GB
    int c = alloc.alloc(1ull * 1024 * 1024 * 1024);
    std::cout << "alloc 1GB -> offset: " << c << "\n";

    std::cout << "\nFinal tree:\n";
    alloc.dump();

    // Free remaining
    alloc.free(b);
    alloc.free(c);
    std::cout << "\nAfter freeing all:\n";
    alloc.dump();

    return 0;
}
