#include "data_store.hpp"
#include "delivery_service.hpp"
#include "security.hpp"
#include "validation.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void testSecurity() {
    const std::string encoded = security::hashPassword("StrongPass123");
    require(encoded.rfind("$pbkdf2-sha256$", 0) == 0, "New password format is not PBKDF2.");
    require(security::verifyPassword("StrongPass123", encoded), "Correct password was rejected.");
    require(!security::verifyPassword("WrongPass123", encoded), "Wrong password was accepted.");
    require(!security::needsPasswordUpgrade(encoded), "New hash was marked for upgrade.");
    require(security::verifyPassword("admin123", "185030e416736038"),
            "Legacy admin password is not compatible.");
    require(security::needsPasswordUpgrade("185030e416736038"),
            "Legacy password was not marked for migration.");
    require(security::verifyPassword(
                "password",
                "$pbkdf2-sha256$10000$73616c7473616c74$"
                "f00b02e07ff5e3b94594410c0a72f524cce210859a486bd72c1ee333830825a3"),
            "PBKDF2 does not match the independent .NET test vector.");
}

void testValidation() {
    require(validation::username("driver_1").ok, "Valid username was rejected.");
    require(!validation::username("x").ok, "Short username was accepted.");
    require(validation::password("Password9").ok, "Valid password was rejected.");
    require(!validation::password("password").ok, "Password without a number was accepted.");
    require(validation::phone("+855 12 345 678", true).ok, "Valid phone was rejected.");
    require(!validation::phone("call-me", true).ok, "Invalid phone was accepted.");
    double parsed = 0;
    require(validation::positiveNumber("42.5", "Weight", parsed).ok && parsed == 42.5,
            "Positive number was rejected.");
    require(!validation::positiveNumber("-1", "Weight", parsed).ok,
            "Negative number was accepted.");

    const std::vector<User> users = {{1, "Admin", "hash", "Administrator", 0}};
    const std::vector<Driver> drivers = {{1, "One", "1234567", "DL-1", "Available", 0}};
    const std::vector<Vehicle> vehicles = {{1, "ABC-1", "Van", 10, "Available"}};
    require(!validation::uniqueUsername(users, "admin").ok, "Case-insensitive username duplicate passed.");
    require(!validation::uniqueLicense(drivers, "dl-1").ok, "License duplicate passed.");
    require(!validation::uniquePlate(vehicles, "abc-1").ok, "Plate duplicate passed.");
}

void testPersistence(const std::filesystem::path& directory) {
    const auto path = directory / "database.json";
    DataStore source(path.string());
    source.users.push_back({1, "tester", "hash", "Customer", 0});
    source.drivers.push_back({1, "Driver", "1234567", "DL-9", "Available", 0});
    require(source.save(), "Atomic save failed: " + source.lastError());
    require(!std::filesystem::exists(path.string() + ".tmp"), "Temporary save file was left behind.");

    DataStore loaded(path.string());
    require(loaded.load(), "Saved database did not reload: " + loaded.lastError());
    require(loaded.users.size() == 1 && loaded.users[0].username == "tester",
            "Reloaded users do not match saved users.");
    require(loaded.drivers.size() == 1 && loaded.drivers[0].licenseNo == "DL-9",
            "Reloaded drivers do not match saved drivers.");

    std::ofstream broken(path, std::ios::trunc);
    broken << "{invalid json";
    broken.close();
    require(!loaded.load(), "Malformed JSON was accepted.");
    require(loaded.users.size() == 1, "Malformed load destroyed in-memory data.");
    require(!loaded.lastError().empty(), "Malformed load did not provide an error.");
}

DataStore deliveryStore(const std::filesystem::path& path) {
    DataStore store(path.string());
    store.users.push_back({1, "dispatcher", "hash", "Dispatcher", 0});
    store.users.push_back({2, "driver", "hash", "Driver", 1});
    store.users.push_back({3, "customer", "hash", "Customer", 0});
    store.drivers.push_back({1, "Driver", "1234567", "DL-1", "Available", 0});
    store.vehicles.push_back({1, "VAN-1", "Van", 100, "Available"});
    Package package;
    package.id = 1;
    package.receiverName = "Receiver";
    package.weightKg = 25;
    package.status = "Pending";
    store.packages.push_back(package);
    return store;
}

void testDeliveryRules(const std::filesystem::path& directory) {
    DataStore store = deliveryStore(directory / "delivery.json");
    require(!delivery::dispatch(store, 1, 1, 1, store.users[2]).ok,
            "Customer was allowed to dispatch a package.");
    require(delivery::dispatch(store, 1, 1, 1, store.users[0]).ok, "Valid dispatch failed.");
    require(store.packages[0].status == "Assigned", "Dispatch did not assign package.");
    require(store.drivers[0].status == "OnDelivery", "Dispatch did not reserve driver.");
    require(store.vehicles[0].status == "InUse", "Dispatch did not reserve vehicle.");
    require(!delivery::deleteDriver(store, 1).ok, "Active driver deletion was allowed.");
    require(!delivery::updateStatus(store, 1, "Delivered", "skip", store.users[0]).ok,
            "Assigned package skipped InTransit.");
    require(!delivery::updateStatus(store, 1, "InTransit", "", store.users[2]).ok,
            "Customer changed delivery status.");
    require(delivery::updateStatus(store, 1, "InTransit", "departed", store.users[1]).ok,
            "Assigned driver could not start delivery.");
    require(delivery::updateStatus(store, 1, "Delivered", "received", store.users[1]).ok,
            "Assigned driver could not complete delivery.");
    require(store.drivers[0].status == "Available" && store.drivers[0].vehicleId == 0,
            "Completing delivery did not release driver.");
    require(store.vehicles[0].status == "Available", "Completing delivery did not release vehicle.");
    require(delivery::nextStatuses("Delivered").empty(), "Terminal status has outgoing transitions.");
    require(delivery::deleteDriver(store, 1).ok, "Completed driver's deletion was blocked.");
}

}  // namespace

int main() {
    const std::filesystem::path test_directory =
        std::filesystem::temp_directory_path() / "fleetflow_core_tests";
    std::error_code error;
    std::filesystem::remove_all(test_directory, error);
    std::filesystem::create_directories(test_directory);
    try {
        testSecurity();
        testValidation();
        testPersistence(test_directory);
        testDeliveryRules(test_directory);
        std::filesystem::remove_all(test_directory, error);
        std::cout << "All FLEETFLOW core tests passed.\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << "TEST FAILURE: " << exception.what() << '\n';
        return 1;
    }
}
