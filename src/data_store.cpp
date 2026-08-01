#include "data_store.hpp"
#include "security.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

DataStore::DataStore(std::string path) : filePath(std::move(path)) {}

bool DataStore::save() const {
    namespace fs = std::filesystem;
    lastError_.clear();
    const json document{{"users", users}, {"drivers", drivers}, {"vehicles", vehicles},
                        {"packages", packages}};
    const fs::path target(filePath);
    const fs::path temporary = target.string() + ".tmp";
    std::error_code error;
    if (!target.parent_path().empty()) {
        fs::create_directories(target.parent_path(), error);
        if (error) {
            lastError_ = "Could not create the database directory: " + error.message();
            return false;
        }
    }
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) {
        lastError_ = "Could not open a temporary database file for writing.";
        return false;
    }
    output << document.dump(2) << '\n';
    output.flush();
    if (!output.good()) {
        lastError_ = "Writing the temporary database file failed.";
        output.close();
        fs::remove(temporary, error);
        return false;
    }
    output.close();

#if defined(_WIN32)
    if (!MoveFileExW(temporary.c_str(), target.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        lastError_ = "Could not replace the database file (Windows error " +
                     std::to_string(GetLastError()) + ").";
        fs::remove(temporary, error);
        return false;
    }
#else
    if (std::rename(temporary.c_str(), target.c_str()) != 0) {
        lastError_ = "Could not atomically replace the database file.";
        fs::remove(temporary, error);
        return false;
    }
#endif
    return true;
}

bool DataStore::load() {
    lastError_.clear();
    std::ifstream input(filePath);
    if (!input) {
        seedDefaultData();
        lastError_ = "Database file was not found; default data was loaded.";
        return false;
    }
    try {
        json document;
        input >> document;
        auto loaded_users = document.value("users", std::vector<User>{});
        auto loaded_drivers = document.value("drivers", std::vector<Driver>{});
        auto loaded_vehicles = document.value("vehicles", std::vector<Vehicle>{});
        auto loaded_packages = document.value("packages", std::vector<Package>{});
        users = std::move(loaded_users);
        drivers = std::move(loaded_drivers);
        vehicles = std::move(loaded_vehicles);
        packages = std::move(loaded_packages);
        if (users.empty()) seedDefaultData();
        return true;
    } catch (const json::exception& error) {
        lastError_ = std::string("Database JSON is invalid: ") + error.what();
        return false;
    }
}

const std::string& DataStore::lastError() const { return lastError_; }

void DataStore::seedDefaultData() {
    if (users.empty()) {
        users.push_back({1, "admin", security::hashPassword("admin123"), "Administrator", 0});
    }
}
Driver* DataStore::findDriver(int id) {
    auto found = std::find_if(drivers.begin(), drivers.end(), [id](const Driver& item) { return item.id == id; });
    return found == drivers.end() ? nullptr : &*found;
}
Vehicle* DataStore::findVehicle(int id) {
    auto found = std::find_if(vehicles.begin(), vehicles.end(), [id](const Vehicle& item) { return item.id == id; });
    return found == vehicles.end() ? nullptr : &*found;
}
Package* DataStore::findPackage(int id) {
    auto found = std::find_if(packages.begin(), packages.end(), [id](const Package& item) { return item.id == id; });
    return found == packages.end() ? nullptr : &*found;
}
User* DataStore::findUser(int id) {
    auto found = std::find_if(users.begin(), users.end(), [id](const User& item) { return item.id == id; });
    return found == users.end() ? nullptr : &*found;
}
User* DataStore::findUserByName(const std::string& name) {
    auto found = std::find_if(users.begin(), users.end(), [&](const User& item) { return item.username == name; });
    return found == users.end() ? nullptr : &*found;
}
bool DataStore::removeDriver(int id) {
    const auto previous_size = drivers.size();
    drivers.erase(std::remove_if(drivers.begin(), drivers.end(), [id](const Driver& item) { return item.id == id; }), drivers.end());
    return drivers.size() != previous_size;
}
bool DataStore::removeVehicle(int id) {
    const auto previous_size = vehicles.size();
    vehicles.erase(std::remove_if(vehicles.begin(), vehicles.end(), [id](const Vehicle& item) { return item.id == id; }), vehicles.end());
    return vehicles.size() != previous_size;
}
