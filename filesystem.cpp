#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <sstream>
#include <memory>
#include <ctime>
#include <stdexcept>

// ================================================================
// ===============  Exception Classes (Single Responsibility)  ====
// ================================================================
class FsException : public std::runtime_error {
public:
    explicit FsException(const std::string &msg) : std::runtime_error(msg) {}
};

class NotFoundException : public FsException {
public:
    explicit NotFoundException(const std::string &msg) : FsException("Not Found: " + msg) {}
};

class AlreadyExistsException : public FsException {
public:
    explicit AlreadyExistsException(const std::string &msg) : FsException("Already Exists: " + msg) {}
};

class InvalidPathException : public FsException {
public:
    explicit InvalidPathException(const std::string &msg) : FsException("Invalid Path: " + msg) {}
};

// ================================================================
// =================  Metadata (Open for extension)  ===============
// ================================================================
struct Metadata {
    std::time_t createdAt;
    std::time_t updatedAt;
    size_t size;

    Metadata() {
        createdAt = updatedAt = std::time(nullptr);
        size = 0;
    }

    void touch() { updatedAt = std::time(nullptr); }
};

// ================================================================
// ================  Abstract Base Class (Composite)  ==============
// ================================================================
class Directory; // Forward declaration

class FsNode {
protected:
    std::string name;
    Directory *parent;
    Metadata meta;

public:
    explicit FsNode(std::string name, Directory *parent = nullptr)
        : name(std::move(name)), parent(parent) {}

    virtual ~FsNode() = default;
    std::string getName() const { return name; }
    Directory *getParent() const { return parent; }
    void setParent(Directory *p) { parent = p; }
    Metadata &getMetadata() { return meta; }

    virtual bool isDirectory() const = 0;
    virtual void print(int depth = 0) const = 0;

    // Build absolute path
    std::string getAbsolutePath() const;
};

// ================================================================
// ========================  File Class  ===========================
// ================================================================
class File : public FsNode {
    std::string content;

public:
    explicit File(std::string name, Directory *parent = nullptr)
        : FsNode(std::move(name), parent) {}

    bool isDirectory() const override { return false; }

    void write(const std::string &data) {
        content = data;
        meta.size = data.size();
        meta.touch();
    }

    std::string read() const { return content; }

    void print(int depth = 0) const override {
        std::cout << std::string(depth, ' ') << "- " << name << " (file)\n";
    }
};

// ================================================================
// =====================  Directory Class  =========================
// ================================================================
class Directory : public FsNode {
    std::unordered_map<std::string, std::shared_ptr<FsNode>> children;

public:
    explicit Directory(std::string name, Directory *parent = nullptr)
        : FsNode(std::move(name), parent) {}

    bool isDirectory() const override { return true; }

    void addChild(std::shared_ptr<FsNode> node) {
        if (children.count(node->getName()))
            throw AlreadyExistsException(node->getName());
        children[node->getName()] = node;
        node->setParent(this);
        meta.touch();
    }

    std::shared_ptr<FsNode> getChild(const std::string &name) const {
        auto it = children.find(name);
        if (it == children.end()) return nullptr;
        return it->second;
    }

    void removeChild(const std::string &name) {
        if (!children.count(name))
            throw NotFoundException(name);
        children.erase(name);
        meta.touch();
    }

    const std::unordered_map<std::string, std::shared_ptr<FsNode>> &getChildren() const {
        return children;
    }

    void print(int depth = 0) const override {
        std::cout << std::string(depth, ' ') << "+ " << name << " (dir)\n";
        for (auto &p : children)
            p.second->print(depth + 2);
    }
};

// ================================================================
// =====================  FsNode::getAbsolutePath  =================
// ================================================================
std::string FsNode::getAbsolutePath() const {
    if (!parent) return "/";
    std::vector<std::string> parts;
    const FsNode *cur = this;
    while (cur) {
        parts.push_back(cur->getName());
        cur = cur->getParent();
    }
    std::string path;
    for (auto it = parts.rbegin(); it != parts.rend(); ++it)
        if (!it->empty()) path += "/" + *it;
    return path.empty() ? "/" : path;
}

// ================================================================
// ======================  PathResolver  ===========================
// ================================================================
class PathResolver {
public:
    // Split path and handle '.' and '..'
    static std::vector<std::string> split(const std::string &path) {
        std::stringstream ss(path);
        std::string item;
        std::vector<std::string> parts;
        while (std::getline(ss, item, '/')) {
            if (item.empty() || item == ".") continue;
            if (item == "..") {
                if (!parts.empty()) parts.pop_back();
            } else {
                parts.push_back(item);
            }
        }
        return parts;
    }

    // Resolve path (absolute or relative)
    static std::shared_ptr<FsNode> resolve(Directory *root, Directory *cwd, const std::string &path) {
        if (path.empty()) throw InvalidPathException("Empty path");

        Directory *cur = path[0] == '/' ? root : cwd;
        std::vector<std::string> parts = split(path);
        std::shared_ptr<FsNode> node;

        for (const auto &p : parts) {
            node = cur->getChild(p);
            if (!node) throw NotFoundException(p);
            if (node->isDirectory()) cur = static_cast<Directory *>(node.get());
        }
        return node ? node : std::shared_ptr<FsNode>(cur);
    }
};

// ================================================================
// =====================  FileSystem Facade  =======================
// ================================================================
class FileSystem {
    std::shared_ptr<Directory> root;
    Directory *cwd;

public:
    FileSystem() {
        root = std::make_shared<Directory>("");
        cwd = root.get();
    }

    void mkdir(const std::string &path) {
        auto parent = getParentDir(path);
        std::string name = getLastName(path);
        parent->addChild(std::make_shared<Directory>(name, parent));
    }

    void createFile(const std::string &path) {
        auto parent = getParentDir(path);
        std::string name = getLastName(path);
        parent->addChild(std::make_shared<File>(name, parent));
    }

    void writeFile(const std::string &path, const std::string &content) {
        auto node = PathResolver::resolve(root.get(), cwd, path);
        if (!node->isDirectory()) {
            std::dynamic_pointer_cast<File>(node)->write(content);
        } else {
            throw InvalidPathException("Cannot write to directory");
        }
    }

    std::string readFile(const std::string &path) {
        auto node = PathResolver::resolve(root.get(), cwd, path);
        if (!node->isDirectory())
            return std::dynamic_pointer_cast<File>(node)->read();
        throw InvalidPathException("Cannot read directory");
    }

    void ls(const std::string &path = ".") {
        auto node = PathResolver::resolve(root.get(), cwd, path);
        if (!node->isDirectory()) {
            std::cout << node->getName() << "\n";
            return;
        }
        auto dir = std::dynamic_pointer_cast<Directory>(node);
        for (auto &p : dir->getChildren()) {
            std::cout << p.first << (p.second->isDirectory() ? "/" : "") << " ";
        }
        std::cout << "\n";
    }

    void cd(const std::string &path) {
        auto node = PathResolver::resolve(root.get(), cwd, path);
        if (!node->isDirectory()) throw InvalidPathException("Not a directory");
        cwd = static_cast<Directory *>(node.get());
    }

    void pwd() const {
        std::cout << cwd->getAbsolutePath() << "\n";
    }

    void printTree() const { root->print(); }

private:
    // Utility: get parent directory of given path
    Directory *getParentDir(const std::string &path) {
        size_t pos = path.find_last_of('/');
        std::string parentPath = (pos == std::string::npos) ? "." : path.substr(0, pos);
        auto node = PathResolver::resolve(root.get(), cwd, parentPath);
        if (!node->isDirectory()) throw InvalidPathException("Parent is not a directory");
        return static_cast<Directory *>(node.get());
    }

    std::string getLastName(const std::string &path) {
        size_t pos = path.find_last_of('/');
        return (pos == std::string::npos) ? path : path.substr(pos + 1);
    }
};

// ================================================================
// =====================  Demonstration  ===========================
// ================================================================
int main() {
    FileSystem fs;

    fs.mkdir("/home");
    fs.mkdir("/home/user");
    fs.createFile("/home/user/readme.txt");
    fs.writeFile("/home/user/readme.txt", "Hello FileSystem!");
    std::cout << fs.readFile("/home/user/readme.txt") << "\n";

    fs.cd("/home/user");
    fs.pwd(); // /home/user

    fs.createFile("notes.txt");
    fs.writeFile("notes.txt", "Notes content");
    fs.ls(".");
    fs.cd("..");
    fs.ls("/home");

    std::cout << "\nFile tree:\n";
    fs.printTree();

    return 0;
}
