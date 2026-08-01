#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct User {
    int id = 0;
    std::string username;
    std::string password;
    std::string role;
    int linkedId = 0;
};
void to_json(json& j, const User& user);
void from_json(const json& j, User& user);

struct Driver {
    int id = 0;
    std::string name;
    std::string phone;
    std::string licenseNo;
    std::string status = "Available";
    int vehicleId = 0;
};
void to_json(json& j, const Driver& driver);
void from_json(const json& j, Driver& driver);

struct Vehicle {
    int id = 0;
    std::string plate;
    std::string type;
    double capacityKg = 0;
    std::string status = "Available";
};
void to_json(json& j, const Vehicle& vehicle);
void from_json(const json& j, Vehicle& vehicle);

struct StatusEvent {
    std::string status;
    std::string timestamp;
    std::string note;
};
void to_json(json& j, const StatusEvent& event);
void from_json(const json& j, StatusEvent& event);

struct Package {
    int id = 0;
    std::string senderName;
    std::string receiverName;
    std::string receiverPhone;
    std::string address;
    double weightKg = 0;
    std::string status = "Pending";
    int driverId = 0;
    int vehicleId = 0;
    int customerId = 0;
    std::string createdAt;
    std::vector<StatusEvent> history;
};
void to_json(json& j, const Package& package);
void from_json(const json& j, Package& package);