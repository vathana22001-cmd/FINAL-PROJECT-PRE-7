# FLEETFLOW

A C++17 terminal dashboard built with FTXUI. It manages users, drivers, vehicles,
packages, dispatch assignments, tracking events, and reports. Application data is
stored in human-readable JSON.

## Features

- Dark FleetFlow-inspired cyan, purple, green, amber, and red FTXUI theme
- Arrow-key navigation, Enter to select, Left/Right to switch panels, Esc to go back
- Immediate feature navigation without an artificial processing delay
- Responsive dashboard with tables, details, statistics, search, and history
- Login, restricted registration, role-based authorization, and secure password storage
- Validated usernames, phones, licenses, plates, weights, and vehicle capacities
- Atomic JSON persistence after successful mutations and at application exit
- Enforced package lifecycle and protected active driver/vehicle assignments

## Menus

Administrator: Driver Management, Vehicle Management, Package Management, Dispatch
Packages, Delivery Tracking, Reports, User Management, Save Data, Load Data, Exit.

Dispatcher: View Drivers, View Vehicles, Add Package, Assign Package, Update Package
Status, Search Package, Driver Manifest, Exit.

Driver: View My Packages, Update Delivery Status, Delivery History, Exit.

Customer: My Profile, My Packages, Track Delivery, Exit.

Packages can be linked to a customer account. Customer views are filtered by the
`customerId` stored on each package, so customers cannot see other users' deliveries.
Existing JSON files without this field remain compatible and default to unassigned.
Public registration is limited to Driver and Customer accounts. Administrator and
Dispatcher accounts can be created only from the authenticated administrator dashboard.

## Build

Requirements: CMake 3.20+, a C++17 compiler, Git, and internet access for the first
configure. CMake pins FTXUI v7.0.1.

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\FLEETFLOW.exe .\data\database.json
```

`Run-FLEETFLOW.cmd` passes the project database path automatically. The CMake build
also embeds this path as the default, so starting the executable directly uses the
same project database. You can still pass another JSON path as the first argument.

Default demo login: username `admin`, password `admin123`.

## Tests

Core authentication, validation, persistence, dispatch, authorization, deletion, and
status-transition behavior is covered by an automated test executable.

```powershell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Controls

- `Up` / `Down`: navigate the current menu or records
- `Left` / `Right` or `Tab`: switch menu/record focus
- `Enter`: open the selected feature or record
- `Esc`: go back or exit the current session
- `A`, `E`, `D`: add, edit, delete/dispatch where authorized
- `U`: update delivery status
- `V`: view package history
- `S` or `/`: search
- `R`: refresh

## JSON data

The application reads and writes `data/database.json`. The file contains four arrays:
`users`, `drivers`, `vehicles`, and `packages`. Package status changes are appended to
the package's `history` array with a timestamp and optional note.

Database updates are written to a temporary file and atomically replace the previous
file only after the complete JSON document has been flushed successfully.

New passwords use salted PBKDF2-HMAC-SHA256 with 200,000 iterations. Existing legacy
passwords remain compatible and are upgraded automatically after a successful login.
The enforced delivery lifecycle is `Pending -> Assigned -> InTransit -> Delivered`;
cancel and failure branches are available only from appropriate states.

## Project layout

```text
FLEETFLOW/
|-- CMakeLists.txt
|-- README.md
|-- data/
|   `-- database.json
|-- include/
|   |-- data_store.hpp
|   |-- delivery_service.hpp
|   |-- models.hpp
|   |-- security.hpp
|   |-- terminal_app.hpp
|   |-- utils.hpp
|   |-- validation.hpp
|   `-- nlohmann/json.hpp
`-- src/
    |-- main.cpp
    |-- data_store.cpp
    |-- delivery_service.cpp
    |-- models.cpp
    |-- security.cpp
    |-- terminal_app.cpp
    |-- utils.cpp
    `-- validation.cpp
```

Public declarations live in `include/*.hpp`; implementations live in `src/*.cpp`.
Business rules are built as the reusable `fleetflow_core` library, while FTXUI remains
in the terminal presentation layer.
# FINAL-PROJECT-PRE-7
# Fleetflow_V2
