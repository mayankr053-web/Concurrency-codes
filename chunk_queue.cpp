#include <iostream>
#include <vector>
#include <deque>
#include <queue>
#include <stdexcept>

//
// ---------- Chunk Manager ----------
//
class ChunkPool {
    std::vector<int> buffer;
    std::queue<size_t> free_chunks;
    size_t chunk_size;
    size_t total_chunks;

public:
    ChunkPool(size_t total_ints, size_t chunk_size)
        : buffer(total_ints),
          chunk_size(chunk_size),
          total_chunks(total_ints / chunk_size)
    {
        for (size_t i = 0; i < total_chunks; ++i)
            free_chunks.push(i);
    }

    int* getChunkPtr(size_t chunk_id) {
        return buffer.data() + chunk_id * chunk_size;
    }

    size_t allocateChunk() {
        if (free_chunks.empty())
            throw std::runtime_error("No free chunks left!");
        size_t id = free_chunks.front();
        free_chunks.pop();
        return id;
    }

    void releaseChunk(size_t id) {
        if (id >= total_chunks)
            throw std::runtime_error("Invalid chunk ID!");
        free_chunks.push(id);
    }

    size_t freeCount() const { return free_chunks.size(); }
    size_t totalCount() const { return total_chunks; }
};

//
// ---------- Queue Using Chunks ----------
//
class ChunkQueue {
    struct ChunkInfo {
        size_t id;   // chunk index
        size_t head; // read index (within chunk)
        size_t tail; // write index (within chunk)
    };

    ChunkPool& pool;
    size_t chunk_size;
    std::deque<ChunkInfo> chunks;

public:
    ChunkQueue(ChunkPool& p, size_t chunk_size)
        : pool(p), chunk_size(chunk_size) {}

    bool empty() const {
        return chunks.empty() || (chunks.size() == 1 && chunks.front().head == chunks.front().tail);
    }

    bool isFull() const {
        if (chunks.empty()) return pool.freeCount() == 0;
        const ChunkInfo& back = chunks.back();
        return (back.tail == chunk_size) && (pool.freeCount() == 0);
    }

    void enqueue(int value) {
        if (isFull()) {
            throw std::runtime_error("Queue is full — no space left in shared buffer!");
        }

        if (chunks.empty() || chunks.back().tail == chunk_size) {
            size_t id = pool.allocateChunk();
            chunks.push_back({id, 0, 0});
        }

        ChunkInfo& back = chunks.back();
        int* ptr = pool.getChunkPtr(back.id);
        ptr[back.tail++] = value;
    }

    int dequeue() {
        if (chunks.empty())
            throw std::runtime_error("Queue is empty!");

        ChunkInfo& front = chunks.front();
        int* ptr = pool.getChunkPtr(front.id);
        int val = ptr[front.head++];

        // If chunk fully consumed, release it
        if (front.head == front.tail) {
            pool.releaseChunk(front.id);
            chunks.pop_front();
        }

        return val;
    }

    size_t usedChunks() const { return chunks.size(); }
};

//
// ---------- Demonstration ----------
//
int main() {
    constexpr size_t TOTAL_BUFFER_INTS = 128; // total ints in shared pool
    constexpr size_t CHUNK_SIZE = 16;         // ints per chunk

    ChunkPool pool(TOTAL_BUFFER_INTS, CHUNK_SIZE);
    ChunkQueue q1(pool, CHUNK_SIZE);
    ChunkQueue q2(pool, CHUNK_SIZE);

    // Fill up Queue 1
    for (int i = 0; i < 32; ++i)
        q1.enqueue(i);

    std::cout << "Queue 1 full? " << (q1.isFull() ? "Yes" : "No") << "\n";

    // Fill up Queue 2 until memory runs out
    try {
        while (true) q2.enqueue(100);
    } catch (const std::exception& e) {
        std::cout << "Exception: " << e.what() << "\n";
    }

    std::cout << "Queue 2 full? " << (q2.isFull() ? "Yes" : "No") << "\n";
    std::cout << "Pool free chunks: " << pool.freeCount() << "\n";
}
