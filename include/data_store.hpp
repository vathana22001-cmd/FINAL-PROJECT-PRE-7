#pragma once

#include "models.hpp"
#include <string>
#include <vector>

class DataStore {
public:
    explicit DataStore(std::string path = "data/database.json");
    std::vector<User> users;
    std::vector<Driver> drivers;
    std::vector<Vehicle> vehicles;
    std::vector<Package> packages;
    std::string filePath;

    bool save() const;
    bool load();
    const std::string& lastError() const;
    void seedDefaultData();
    Driver* findDriver(int id);
    Vehicle* findVehicle(int id);
    Package* findPackage(int id);
    User* findUser(int id);
    User* findUserByName(const std::string& name);
    bool removeDriver(int id);
    bool removeVehicle(int id);

private:
    mutable std::string lastError_;
};
