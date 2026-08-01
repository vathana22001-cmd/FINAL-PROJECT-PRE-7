cmake_minimum_required(VERSION 3.20)
project(FLEETFLOW CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(CTest)

include(FetchContent)

set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    ftxui
    GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
    GIT_TAG v7.0.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(ftxui)

add_library(fleetflow_core
    src/data_store.cpp
    src/delivery_service.cpp
    src/models.cpp
    src/security.cpp
    src/utils.cpp
    src/validation.cpp
)

target_include_directories(fleetflow_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if(WIN32)
    target_link_libraries(fleetflow_core PUBLIC bcrypt)
endif()

add_executable(FLEETFLOW
    src/main.cpp
    src/terminal_app.cpp
)

target_include_directories(FLEETFLOW PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Keep one canonical database for every launch method (launcher, command line,
# or starting the executable directly from the build directory).
target_compile_definitions(FLEETFLOW PRIVATE
    FLEETFLOW_DEFAULT_DATABASE_PATH="${CMAKE_SOURCE_DIR}/data/database.json"
)

target_link_libraries(FLEETFLOW PRIVATE
    fleetflow_core
    ftxui::screen
    ftxui::dom
    ftxui::component
)

if (MSVC)
    target_compile_options(FLEETFLOW PRIVATE /utf-8 /W4 /permissive-)
    target_compile_options(fleetflow_core PRIVATE /utf-8 /W4 /permissive-)
else()
    target_compile_options(FLEETFLOW PRIVATE -Wall -Wextra)
    target_compile_options(fleetflow_core PRIVATE -Wall -Wextra)
endif()

if (BUILD_TESTING)
    add_executable(FLEETFLOW_tests
        tests/test_main.cpp
    )
    target_link_libraries(FLEETFLOW_tests PRIVATE fleetflow_core)
    if (MSVC)
        target_compile_options(FLEETFLOW_tests PRIVATE /utf-8 /W4 /permissive-)
    else()
        target_compile_options(FLEETFLOW_tests PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME fleetflow_core COMMAND FLEETFLOW_tests)
endif()
