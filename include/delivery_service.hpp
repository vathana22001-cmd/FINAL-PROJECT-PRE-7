#pragma once

#include "data_store.hpp"

#include <string>
#include <vector>

namespace delivery {

struct Result {
    bool ok;
    std::string message;
};

std::vector<std::string> nextStatuses(const std::string& current_status);
Result dispatch(DataStore& store, int package_id, int driver_id, int vehicle_id, const User& actor);
Result updateStatus(DataStore& store, int package_id, const std::string& next_status,
                    const std::string& note, const User& actor);
Result deleteDriver(DataStore& store, int driver_id);
Result deleteVehicle(DataStore& store, int vehicle_id);
Result deletePackage(DataStore& store, int package_id);
Result deleteUser(DataStore& store, int user_id, int current_user_id);

}  // namespace delivery
