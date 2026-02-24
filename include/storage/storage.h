#ifndef CACHEFORGE_STORAGE_H
#define CACHEFORGE_STORAGE_H

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace cacheforge {

class Storage {
public:
    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    size_t size() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> data_;
};

} // namespace cacheforge

#endif // CACHEFORGE_STORAGE_H
