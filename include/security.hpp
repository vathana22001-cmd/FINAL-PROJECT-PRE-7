#pragma once

#include <string>

namespace security {

std::string hashPassword(const std::string& password);
bool verifyPassword(const std::string& password, const std::string& stored);
bool needsPasswordUpgrade(const std::string& stored);

}  // namespace security
