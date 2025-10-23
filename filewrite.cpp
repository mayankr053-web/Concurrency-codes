#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <atomic>
#include <cstring>

// --- Provided interfaces (as per question) ---
int open(std::string name);
int pread(int fd, char* buf, size_t len, long long offset);
int pwrite(int fd, const char* buf, size_t len, long long offset);
int close(int fd);

// --- Configuration ---
constexpr size_t BUFFER_SIZE = 4096;
constexpr size_t MAX_QUEUE_SIZE = 32;
constexpr int NUM_READERS = 4;
constexpr int NUM_WRITERS = 4;

// --- Data structure representing a file chunk ---
struct Chunk {
    std::vector<char> data;
    size_t size;
    long long offset;
};

// --- Thread-safe bounded queue ---
class ChunkQueue {
    std::queue<Chunk> q;
    std::mutex mtx;
    std::condition_variable cv_full, cv_empty;
    bool done = false;

public:
    void push(Chunk&& chunk) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_full.wait(lock, [this]() { return q.size() < MAX_QUEUE_SIZE; });
        q.push(std::move(chunk));
        cv_empty.notify_one();
    }

    bool pop(Chunk& chunk) {
        std::unique_lock<std::mutex> lock(mtx);
        cv_empty.wait(lock, [this]() { return done || !q.empty(); });
        if (q.empty()) return false;
        chunk = std::move(q.front());
        q.pop();
        cv_full.notify_one();
        return true;
    }

    void setDone() {
        std::lock_guard<std::mutex> lock(mtx);
        done = true;
        cv_empty.notify_all();
    }
};

// --- Reader thread function ---
void readerThreadFunc(int srcFD, ChunkQueue& queue,
                      std::atomic<long long>& nextOffset,
                      std::atomic<int>& activeReaders,
                      long long fileSize) {
    while (true) {
        long long offset = nextOffset.fetch_add(BUFFER_SIZE);
        if (offset >= fileSize) break;

        Chunk chunk;
        chunk.data.resize(BUFFER_SIZE);
        int bytesRead = pread(srcFD, chunk.data.data(), BUFFER_SIZE, offset);

        if (bytesRead < 0) {
            std::cerr << "Error reading source at offset " << offset << std::endl;
            break;
        }
        if (bytesRead == 0) break;

        chunk.size = bytesRead;
        chunk.offset = offset;

        queue.push(std::move(chunk));
    }

    if (--activeReaders == 0)
        queue.setDone(); // signal writers that all data is read
}

// --- Writer thread function ---
void writerThreadFunc(int dstFD, ChunkQueue& queue) {
    Chunk chunk;
    while (queue.pop(chunk)) {
        size_t written = 0;
        while (written < chunk.size) {
            int ret = pwrite(dstFD, chunk.data.data() + written, chunk.size - written,
                             chunk.offset + written);
            if (ret < 0) {
                std::cerr << "Error writing destination at offset "
                          << (chunk.offset + written) << std::endl;
                return;
            }
            written += ret;
        }
    }
}

// --- Main copy() function ---
int copy(const std::string& dst, const std::string& src, long long fileSize) {
    int srcFD = -1, dstFD = -1;

    srcFD = open(src);
    if (srcFD < 0) {
        std::cerr << "Failed to open source file: " << src << std::endl;
        return -1;
    }

    dstFD = open(dst);
    if (dstFD < 0) {
        std::cerr << "Failed to open destination file: " << dst << std::endl;
        close(srcFD);
        return -1;
    }

    ChunkQueue queue;
    std::atomic<long long> nextOffset{0};
    std::atomic<int> activeReaders{NUM_READERS};

    // Launch reader threads
    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back(readerThreadFunc, srcFD, std::ref(queue),
                             std::ref(nextOffset), std::ref(activeReaders), fileSize);
    }

    // Launch writer threads
    std::vector<std::thread> writers;
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writers.emplace_back(writerThreadFunc, dstFD, std::ref(queue));
    }

    // Join all threads
    for (auto& t : readers) t.join();
    queue.setDone(); // ensure done flag is set
    for (auto& t : writers) t.join();

    close(srcFD);
    close(dstFD);

    return 0;
}

// --- Mock Implementations for Demonstration ---
#ifdef TEST_COPY
static const char* FAKE_CONTENT =
    "This is a large file being copied concurrently by multiple threads using pread/pwrite.";

int open(std::string name) { static int fd = 1; return fd++; }

int pread(int fd, char* buf, size_t len, long long offset) {
    size_t total = strlen(FAKE_CONTENT);
    if (offset >= total) return 0;
    size_t n = std::min(len, total - (size_t)offset);
    memcpy(buf, FAKE_CONTENT + offset, n);
    return n;
}

int pwrite(int fd, const char* buf, size_t len, long long offset) {
    std::cout << "[WRITE offset=" << offset << ", len=" << len << "] "
              << std::string(buf, len) << std::endl;
    return len;
}

int close(int fd) { return 0; }

int main() {
    long long fileSize = strlen(FAKE_CONTENT);
    copy("dst.txt", "src.txt", fileSize);
    return 0;
}
#endif
