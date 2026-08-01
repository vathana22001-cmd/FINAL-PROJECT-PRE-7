#include "models.hpp"

void to_json(json& j, const User& user) {
    j = json{{"id", user.id}, {"username", user.username}, {"password", user.password},
             {"role", user.role}, {"linkedId", user.linkedId}};
}
void from_json(const json& j, User& user) {
    j.at("id").get_to(user.id);
    j.at("username").get_to(user.username);
    j.at("password").get_to(user.password);
    j.at("role").get_to(user.role);
    user.linkedId = j.value("linkedId", 0);
}
void to_json(json& j, const Driver& driver) {
    j = json{{"id", driver.id}, {"name", driver.name}, {"phone", driver.phone},
             {"licenseNo", driver.licenseNo}, {"status", driver.status},
             {"vehicleId", driver.vehicleId}};
}
void from_json(const json& j, Driver& driver) {
    j.at("id").get_to(driver.id);
    j.at("name").get_to(driver.name);
    j.at("phone").get_to(driver.phone);
    j.at("licenseNo").get_to(driver.licenseNo);
    j.at("status").get_to(driver.status);
    driver.vehicleId = j.value("vehicleId", 0);
}
void to_json(json& j, const Vehicle& vehicle) {
    j = json{{"id", vehicle.id}, {"plate", vehicle.plate}, {"type", vehicle.type},
             {"capacityKg", vehicle.capacityKg}, {"status", vehicle.status}};
}
void from_json(const json& j, Vehicle& vehicle) {
    j.at("id").get_to(vehicle.id);
    j.at("plate").get_to(vehicle.plate);
    j.at("type").get_to(vehicle.type);
    j.at("capacityKg").get_to(vehicle.capacityKg);
    j.at("status").get_to(vehicle.status);
}
void to_json(json& j, const StatusEvent& event) {
    j = json{{"status", event.status}, {"timestamp", event.timestamp}, {"note", event.note}};
}
void from_json(const json& j, StatusEvent& event) {
    j.at("status").get_to(event.status);
    j.at("timestamp").get_to(event.timestamp);
    event.note = j.value("note", "");
}
void to_json(json& j, const Package& package) {
    j = json{{"id", package.id}, {"senderName", package.senderName},
             {"receiverName", package.receiverName}, {"receiverPhone", package.receiverPhone},
             {"address", package.address}, {"weightKg", package.weightKg},
             {"status", package.status}, {"driverId", package.driverId},
             {"vehicleId", package.vehicleId}, {"customerId", package.customerId},
             {"createdAt", package.createdAt}, {"history", package.history}};
}
void from_json(const json& j, Package& package) {
    j.at("id").get_to(package.id);
    j.at("senderName").get_to(package.senderName);
    j.at("receiverName").get_to(package.receiverName);
    package.receiverPhone = j.value("receiverPhone", "");
    j.at("address").get_to(package.address);
    j.at("weightKg").get_to(package.weightKg);
    j.at("status").get_to(package.status);
    package.driverId = j.value("driverId", 0);
    package.vehicleId = j.value("vehicleId", 0);
    package.customerId = j.value("customerId", 0);
    package.createdAt = j.value("createdAt", "");
    package.history = j.value("history", std::vector<StatusEvent>{});
}