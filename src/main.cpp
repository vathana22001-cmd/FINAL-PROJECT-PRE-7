#include "data_store.hpp"
#include "terminal_app.hpp"

#include <iostream>
#include <string>

#ifndef FLEETFLOW_DEFAULT_DATABASE_PATH
#define FLEETFLOW_DEFAULT_DATABASE_PATH "data/database.json"
#endif

int main(int argc, char* argv[]) {
    const std::string database_path =
        argc > 1 ? argv[1] : FLEETFLOW_DEFAULT_DATABASE_PATH;
    DataStore store(database_path);
    if (!store.load() && store.users.empty()) {
        std::cerr << "Could not load database: " << store.lastError() << '\n';
        return 1;
    }
    terminal::run(store);
    if (!store.save()) {
        std::cerr << "Could not save database: " << store.lastError() << '\n';
        return 1;
    }
    return 0;
}
