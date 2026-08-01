#include "validation.hpp"

#include "utils.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace validation {
namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool same(const std::string& left, const std::string& right) {
    return lower(utils::trim(left)) == lower(utils::trim(right));
}

Result valid() { return {true, {}}; }

}  // namespace

Result username(const std::string& value) {
    const std::string clean = utils::trim(value);
    if (clean.size() < 3 || clean.size() > 32) {
        return {false, "Username must contain 3 to 32 characters."};
    }
    if (!std::all_of(clean.begin(), clean.end(), [](unsigned char c) {
            return std::isalnum(c) != 0 || c == '_' || c == '-';
        })) {
        return {false, "Username may contain only letters, numbers, '-' and '_'."};
    }
    return valid();
}

Result password(const std::string& value) {
    if (value.size() < 8) return {false, "Password must contain at least 8 characters."};
    if (value.size() > 128) return {false, "Password must not exceed 128 characters."};
    const bool has_letter = std::any_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isalpha(c) != 0;
    });
    const bool has_digit = std::any_of(value.begin(), value.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
    if (!has_letter || !has_digit) {
        return {false, "Password must include at least one letter and one number."};
    }
    return valid();
}

Result phone(const std::string& value, bool required) {
    const std::string clean = utils::trim(value);
    if (clean.empty()) return required ? Result{false, "Phone number is required."} : valid();
    int digits = 0;
    for (unsigned char c : clean) {
        if (std::isdigit(c) != 0) {
            ++digits;
        } else if (c != '+' && c != '-' && c != ' ' && c != '(' && c != ')') {
            return {false, "Phone number contains an unsupported character."};
        }
    }
    if (digits < 7 || digits > 15) return {false, "Phone number must contain 7 to 15 digits."};
    return valid();
}

Result license(const std::string& value) {
    const std::string clean = utils::trim(value);
    if (clean.size() < 3 || clean.size() > 32) {
        return {false, "License number must contain 3 to 32 characters."};
    }
    return valid();
}

Result plate(const std::string& value) {
    const std::string clean = utils::trim(value);
    if (clean.size() < 2 || clean.size() > 20) {
        return {false, "Vehicle plate must contain 2 to 20 characters."};
    }
    return valid();
}

Result positiveNumber(const std::string& value, const std::string& label, double& parsed) {
    try {
        std::size_t used = 0;
        parsed = std::stod(utils::trim(value), &used);
        if (used != utils::trim(value).size() || !std::isfinite(parsed) || parsed <= 0) {
            return {false, label + " must be a positive number."};
        }
    } catch (...) {
        return {false, label + " must be a positive number."};
    }
    return valid();
}

Result uniqueUsername(const std::vector<User>& users, const std::string& value, int except_id) {
    const auto duplicate = std::find_if(users.begin(), users.end(), [&](const User& user) {
        return user.id != except_id && same(user.username, value);
    });
    return duplicate == users.end() ? valid() : Result{false, "That username already exists."};
}

Result uniqueLicense(const std::vector<Driver>& drivers, const std::string& value, int except_id) {
    const auto duplicate = std::find_if(drivers.begin(), drivers.end(), [&](const Driver& driver) {
        return driver.id != except_id && same(driver.licenseNo, value);
    });
    return duplicate == drivers.end() ? valid() : Result{false, "That driver license already exists."};
}

Result uniquePlate(const std::vector<Vehicle>& vehicles, const std::string& value, int except_id) {
    const auto duplicate = std::find_if(vehicles.begin(), vehicles.end(), [&](const Vehicle& vehicle) {
        return vehicle.id != except_id && same(vehicle.plate, value);
    });
    return duplicate == vehicles.end() ? valid() : Result{false, "That vehicle plate already exists."};
}

}  // namespace validation
