#include "storage/storage.h"

namespace cacheforge {

void Storage::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    data_[key] = value;
}

std::optional<std::string> Storage::get(const std::string& key) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool Storage::del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.erase(key) > 0;
}

size_t Storage::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_.size();
}

} // namespace cacheforge
