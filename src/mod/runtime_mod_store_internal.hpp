#pragma once

#include <string>
#include <string_view>

namespace bruce::runtime_mod_store_detail {

void check_private_node(int fd, bool directory);
void check_private_directory(const std::string& path, bool create);

// Walk the supplied path without following any component symlink and keep the
// final private directory pinned while catalog readers access its children.
class ReadOnlyRoot {
public:
    explicit ReadOnlyRoot(const std::string& path);
    ~ReadOnlyRoot();
    ReadOnlyRoot(const ReadOnlyRoot&) = delete;
    ReadOnlyRoot& operator=(const ReadOnlyRoot&) = delete;
    [[nodiscard]] std::string anchored_path() const;
private:
    int fd_ = -1;
};

class RootLock {
public:
    explicit RootLock(const std::string& root, bool read_only = false, bool create = true);
    ~RootLock();
    RootLock(const RootLock&) = delete;
    RootLock& operator=(const RootLock&) = delete;
private:
    int fd_ = -1;
};

// Internal deterministic crash-boundary hook. It is unreachable from product
// CLIs and exists solely so recovery tests can interrupt publication steps.
void set_test_failpoint(std::string name);
void clear_test_failpoint();
void test_failpoint(std::string_view name);

} // namespace bruce::runtime_mod_store_detail
