#include "delivery_service.hpp"

#include "utils.hpp"

#include <algorithm>

namespace delivery {
namespace {

Result failure(const std::string& message) { return {false, message}; }
Result success(const std::string& message) { return {true, message}; }

bool terminal(const std::string& status) {
    return status == "Delivered" || status == "Failed" || status == "Cancelled";
}

void releaseAssignment(DataStore& store, Package& package) {
    if (auto* driver = store.findDriver(package.driverId)) {
        driver->status = "Available";
        driver->vehicleId = 0;
    }
    if (auto* vehicle = store.findVehicle(package.vehicleId)) vehicle->status = "Available";
}

}  // namespace

std::vector<std::string> nextStatuses(const std::string& current_status) {
    if (current_status == "Pending") return {"Cancelled"};
    if (current_status == "Assigned") return {"InTransit", "Cancelled"};
    if (current_status == "InTransit") return {"Delivered", "Failed", "Cancelled"};
    return {};
}

Result dispatch(DataStore& store, int package_id, int driver_id, int vehicle_id, const User& actor) {
    if (actor.role != "Administrator" && actor.role != "Dispatcher") {
        return failure("Only an administrator or dispatcher can dispatch packages.");
    }
    Package* package = store.findPackage(package_id);
    Driver* driver = store.findDriver(driver_id);
    Vehicle* vehicle = store.findVehicle(vehicle_id);
    if (!package) return failure("Package was not found.");
    if (!driver) return failure("Driver was not found.");
    if (!vehicle) return failure("Vehicle was not found.");
    if (package->status != "Pending") return failure("Only pending packages can be dispatched.");
    if (package->weightKg <= 0) return failure("Package weight must be positive before dispatch.");
    if (driver->status != "Available") return failure("The selected driver is not available.");
    if (vehicle->status != "Available") return failure("The selected vehicle is not available.");
    if (vehicle->capacityKg < package->weightKg) return failure("The vehicle cannot carry this package.");

    package->driverId = driver->id;
    package->vehicleId = vehicle->id;
    package->status = "Assigned";
    package->history.push_back({"Assigned", utils::now(),
                                "Assigned to " + driver->name + " / " + vehicle->plate});
    driver->status = "OnDelivery";
    driver->vehicleId = vehicle->id;
    vehicle->status = "InUse";
    return success("Package assigned to " + driver->name + ".");
}

Result updateStatus(DataStore& store, int package_id, const std::string& next_status,
                    const std::string& note, const User& actor) {
    Package* package = store.findPackage(package_id);
    if (!package) return failure("Package was not found.");
    if (actor.role != "Administrator" && actor.role != "Dispatcher" && actor.role != "Driver") {
        return failure("This account cannot update delivery status.");
    }
    if (actor.role == "Driver" && package->driverId != actor.linkedId) {
        return failure("Drivers can update only their assigned packages.");
    }
    const auto allowed = nextStatuses(package->status);
    if (std::find(allowed.begin(), allowed.end(), next_status) == allowed.end()) {
        return failure("Invalid status transition from " + package->status + " to " + next_status + ".");
    }
    if (actor.role == "Driver" && next_status == "Cancelled") {
        return failure("Only an administrator or dispatcher can cancel a package.");
    }

    package->status = next_status;
    package->history.push_back({next_status, utils::now(), note});
    if (terminal(next_status)) releaseAssignment(store, *package);
    return success("Package is now " + next_status + ".");
}

Result deleteDriver(DataStore& store, int driver_id) {
    Driver* driver = store.findDriver(driver_id);
    if (!driver) return failure("Driver was not found.");
    const bool active = std::any_of(store.packages.begin(), store.packages.end(), [&](const Package& package) {
        return package.driverId == driver_id && !terminal(package.status);
    });
    if (active) return failure("Cannot delete a driver with an active package.");
    for (auto& user : store.users) if (user.linkedId == driver_id) user.linkedId = 0;
    for (auto& package : store.packages) if (package.driverId == driver_id) package.driverId = 0;
    store.removeDriver(driver_id);
    return success("Driver deleted.");
}

Result deleteVehicle(DataStore& store, int vehicle_id) {
    Vehicle* vehicle = store.findVehicle(vehicle_id);
    if (!vehicle) return failure("Vehicle was not found.");
    const bool active = std::any_of(store.packages.begin(), store.packages.end(), [&](const Package& package) {
        return package.vehicleId == vehicle_id && !terminal(package.status);
    });
    if (active) return failure("Cannot delete a vehicle with an active package.");
    for (auto& driver : store.drivers) if (driver.vehicleId == vehicle_id) driver.vehicleId = 0;
    for (auto& package : store.packages) if (package.vehicleId == vehicle_id) package.vehicleId = 0;
    store.removeVehicle(vehicle_id);
    return success("Vehicle deleted.");
}

Result deletePackage(DataStore& store, int package_id) {
    Package* package = store.findPackage(package_id);
    if (!package) return failure("Package was not found.");
    if (!terminal(package->status) && package->status != "Pending") {
        return failure("Finish or cancel an active package before deleting it.");
    }
    store.packages.erase(std::remove_if(store.packages.begin(), store.packages.end(),
                                       [&](const Package& item) { return item.id == package_id; }),
                         store.packages.end());
    return success("Package deleted.");
}

Result deleteUser(DataStore& store, int user_id, int current_user_id) {
    if (user_id == current_user_id) return failure("You cannot delete the account currently in use.");
    User* user = store.findUser(user_id);
    if (!user) return failure("User was not found.");
    if (user->role == "Driver" && user->linkedId != 0) {
        const bool active = std::any_of(store.packages.begin(), store.packages.end(), [&](const Package& package) {
            return package.driverId == user->linkedId && !terminal(package.status);
        });
        if (active) return failure("Cannot delete a driver account with an active package.");
    }
    for (auto& package : store.packages) if (package.customerId == user_id) package.customerId = 0;
    store.users.erase(std::remove_if(store.users.begin(), store.users.end(),
                                    [&](const User& item) { return item.id == user_id; }),
                      store.users.end());
    return success("User deleted.");
}

}  // namespace delivery
