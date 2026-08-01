#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace utils {
std::string now();
std::string trim(const std::string& value);
bool isNumber(const std::string& value);

template <typename T>
int nextId(const std::vector<T>& items) {
    int maximum = 0;
    for (const auto& item : items) maximum = std::max(maximum, item.id);
    return maximum + 1;
}
}  // namespace utils
