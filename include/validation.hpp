#pragma once

#include "models.hpp"

#include <string>
#include <vector>

namespace validation {

struct Result {
    bool ok;
    std::string message;
};

Result username(const std::string& value);
Result password(const std::string& value);
Result phone(const std::string& value, bool required = false);
Result license(const std::string& value);
Result plate(const std::string& value);
Result positiveNumber(const std::string& value, const std::string& label, double& parsed);
Result uniqueUsername(const std::vector<User>& users, const std::string& value, int except_id = 0);
Result uniqueLicense(const std::vector<Driver>& drivers, const std::string& value, int except_id = 0);
Result uniquePlate(const std::vector<Vehicle>& vehicles, const std::string& value, int except_id = 0);

}  // namespace validation
