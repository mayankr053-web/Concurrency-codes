#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <queue>
#include <regex>
#include <atomic>
#include <chrono>

// -----------------------------------------------
// Mock HTML Parser (simulate network pages)
// -----------------------------------------------
class HtmlParser {
public:
    virtual std::vector<std::string> getUrls(const std::string &url) = 0;
};

class MockHtmlParser : public HtmlParser {
    std::unordered_map<std::string, std::vector<std::string>> graph;
public:
    MockHtmlParser() {
        graph["http://example.com"] = {
            "http://example.com/about",
            "http://example.com/blog",
            "http://external.com/home"
        };
        graph["http://example.com/about"] = {
            "http://example.com/team",
            "http://example.com"
        };
        graph["http://example.com/blog"] = {
            "http://example.com/post1",
            "http://example.com/post2"
        };
        graph["http://example.com/post1"] = {};
        graph["http://example.com/post2"] = {};
        graph["http://example.com/team"] = {};
    }

    std::vector<std::string> getUrls(const std::string &url) override {
        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // simulate latency
        std::cout << "Thread " << std::this_thread::get_id() << " parsing: " << url << "\n";
        return graph[url];
    }
};

// -----------------------------------------------
// Helper: Extract hostname from URL
// -----------------------------------------------
std::string getHostName(const std::string &url) {
    std::regex re(R"(https?://([^/]+))");
    std::smatch match;
    if (std::regex_search(url, match, re)) {
        return match[1];
    }
    return "";
}

// -----------------------------------------------
// Thread-safe blocking queue
// -----------------------------------------------
template <typename T>
class ConcurrentQueue {
    std::queue<T> q;
    std::mutex mtx;
    std::condition_variable cv;
    bool finished = false;

public:
    void push(T value) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            q.push(std::move(value));
        }
        cv.notify_one();
    }

    bool pop(T &value) {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&]() { return finished || !q.empty(); });
        if (q.empty()) return false;
        value = std::move(q.front());
        q.pop();
        return true;
    }

    void setFinished() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            finished = true;
        }
        cv.notify_all();
    }
};

// -----------------------------------------------
// Thread Pool
// -----------------------------------------------
class ThreadPool {
    std::vector<std::thread> workers;

public:
    template <typename Callable>
    ThreadPool(size_t numThreads, Callable workerFunc) {
        for (size_t i = 0; i < numThreads; ++i)
            workers.emplace_back(workerFunc);
    }

    ~ThreadPool() {
        for (auto &t : workers)
            if (t.joinable())
                t.join();
    }
};

// -----------------------------------------------
// WebCrawler
// -----------------------------------------------
class WebCrawler {
    std::unordered_set<std::string> visited;
    std::mutex visitedMutex;
    std::atomic<int> activeTasks{0};
    std::condition_variable cv_done;
    std::mutex doneMutex;

public:
    std::vector<std::string> crawl(const std::string &startUrl, HtmlParser &parser) {
        const std::string host = getHostName(startUrl);

        ConcurrentQueue<std::string> queue;
        {
            std::lock_guard<std::mutex> lock(visitedMutex);
            visited.insert(startUrl);
        }
        queue.push(startUrl);
        activeTasks = 1;

        // Thread pool with explicit worker function
        const int NUM_THREADS = std::thread::hardware_concurrency();
        ThreadPool pool(NUM_THREADS ? NUM_THREADS : 4, [&]() {
            workerThread(parser, host, queue);
        });

        // Wait for completion
        {
            std::unique_lock<std::mutex> lock(doneMutex);
            cv_done.wait(lock, [&]() { return activeTasks.load() == 0; });
        }

        queue.setFinished();

        return std::vector<std::string>(visited.begin(), visited.end());
    }

private:
    // Worker thread main loop
    void workerThread(HtmlParser &parser, const std::string &host, ConcurrentQueue<std::string> &queue) {
        std::string url;
        while (queue.pop(url)) {
            processUrl(url, host, parser, queue);
            if (--activeTasks == 0) {
                std::lock_guard<std::mutex> lock(doneMutex);
                cv_done.notify_one();
            }
        }
    }

    // Process a single URL: fetch links and enqueue new ones
    void processUrl(const std::string &url, const std::string &host,
                    HtmlParser &parser, ConcurrentQueue<std::string> &queue) {
        try {
            auto urls = parser.getUrls(url);
            for (auto &next : urls) {
                if (getHostName(next) != host) continue;

                std::lock_guard<std::mutex> lock(visitedMutex);
                if (visited.insert(next).second) {
                    queue.push(next);
                    activeTasks++;
                }
            }
        } catch (...) {
            // Ignore network / parsing errors
        }
    }
};

// -----------------------------------------------
// Demo
// -----------------------------------------------
int main() {
    MockHtmlParser parser;
    WebCrawler crawler;

    std::string startUrl = "http://example.com";
    auto result = crawler.crawl(startUrl, parser);

    std::cout << "\nCrawled URLs:\n";
    for (const auto &url : result) {
        std::cout << " - " << url << "\n";
    }

    return 0;
}
