#include "terminal_app.hpp"

#include "data_store.hpp"
#include "delivery_service.hpp"
#include "security.hpp"
#include "utils.hpp"
#include "validation.hpp"

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <ftxui/screen/terminal.hpp>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace terminal {
namespace {

using namespace ftxui;

namespace theme {
const Color background = Color::RGB(8, 15, 23);
const Color surface = Color::RGB(8, 15, 23);
const Color border = Color::RGB(30, 40, 60);
const Color primary = Color::RGB(0, 200, 255);
const Color selection = Color::RGB(18, 70, 92);
const Color success = Color::RGB(57, 255, 20);
const Color warning = Color::RGB(255, 193, 7);
const Color danger = Color::RGB(255, 59, 48);
const Color accent = Color::RGB(160, 32, 240);
const Color text = Color::RGB(222, 230, 238);
const Color muted = Color::RGB(128, 145, 160);
}  // namespace theme

struct FormField {
    std::string label;
    std::string value;
    std::string placeholder;
    bool password = false;
};

enum class Action {
    None,
    Add,
    Edit,
    Delete,
    Dispatch,
    Status,
    View,
    Search,
    Save,
    Load,
    Logout,
};

struct NavItem {
    std::string key;
    std::string label;
};

struct TableData {
    std::vector<std::string> headers;
    std::vector<int> widths;
    std::vector<std::vector<std::string>> rows;
    std::vector<int> ids;
};

struct DashboardState {
    int nav = 0;
    int row = 0;
    int focus = 0;
    int selected_id = 0;
    Action action = Action::None;
    std::string search;
    std::string notice = "Data loaded successfully";
};

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

bool contains(const std::vector<std::string>& row, const std::string& query) {
    if (query.empty()) {
        return true;
    }
    const std::string needle = lower(query);
    for (const auto& cell : row) {
        if (lower(cell).find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

std::string number(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(value == static_cast<int>(value) ? 0 : 1) << value;
    return out.str();
}

std::string driver_name(DataStore& store, int id) {
    if (auto* driver = store.findDriver(id)) {
        return driver->name;
    }
    return "-";
}

std::string vehicle_name(DataStore& store, int id) {
    if (auto* vehicle = store.findVehicle(id)) {
        return vehicle->plate;
    }
    return "-";
}

Color status_color(const std::string& status) {
    if (status == "Available" || status == "Delivered" || status == "Online") {
        return theme::success;
    }
    if (status == "OnDelivery" || status == "InUse" || status == "InTransit" ||
        status == "Assigned") {
        return theme::primary;
    }
    if (status == "Pending" || status == "OffDuty" || status == "Maintenance") {
        return theme::warning;
    }
    return theme::danger;
}

Element titled(const std::string& title, Element body, Color title_color = theme::primary) {
    return window(text(" " + title + " ") | bold | color(title_color),
                  std::move(body) | color(theme::text)) |
           color(theme::border) | bgcolor(theme::surface);
}

Element metric(const std::string& label, int value, int total, Color value_color) {
    const float ratio = total == 0 ? 0.0F : static_cast<float>(value) / static_cast<float>(total);
    return vbox({
        hbox({
            text(label) | color(theme::muted) | flex,
            text(std::to_string(value)) | bold | color(value_color),
        }),
        gauge(ratio) | color(value_color),
    });
}

Element brand_logo();

ButtonOption outline_only_button() {
    ButtonOption option;
    option.transform = [](const EntryState& state) {
        Element label = text(state.label);
        return state.focused ? label | bold | borderDouble
                             : label | borderRounded;
    };
    return option;
}

ScreenInteractive& shared_screen() {
    static ScreenInteractive screen = ScreenInteractive::Fullscreen();
    return screen;
}

bool run_form(const std::string& title, std::vector<FormField>& fields,
              const std::string& submit_label = "Save",
              const std::string& cancel_label = "Cancel",
              bool* alternate_requested = nullptr) {
    ScreenInteractive& screen = shared_screen();
    bool submitted = false;
    std::vector<Component> inputs;
    for (std::size_t i = 0; i < fields.size(); ++i) {
        auto& field = fields[i];
        InputOption option;
        option.password = field.password;
        option.multiline = false;
        if (i + 1 == fields.size()) {
            option.on_enter = [&] {
                submitted = true;
                screen.ExitLoopClosure()();
            };
        }
        inputs.push_back(Input(&field.value, field.placeholder, option));
    }

    auto save = Button(submit_label, [&] {
        submitted = true;
        screen.ExitLoopClosure()();
    }, outline_only_button());
    auto cancel = Button(cancel_label, [&] {
        if (alternate_requested) *alternate_requested = true;
        screen.ExitLoopClosure()();
    }, outline_only_button());
    auto buttons = Container::Horizontal({save, cancel});
    auto form_components = inputs;
    form_components.push_back(buttons);
    auto container = Container::Vertical(std::move(form_components));
    container = CatchEvent(container, [&](Event event) {
        if (event != Event::Escape) return false;
        screen.ExitLoopClosure()();
        return true;
    });

    auto renderer = Renderer(container, [&] {
        Elements lines;
        lines.push_back(brand_logo() | center);
        lines.push_back(text("Complete the fields below") | center | color(theme::text));
        lines.push_back(separator() | color(theme::border));
        for (std::size_t i = 0; i < fields.size(); ++i) {
            lines.push_back(hbox({
                text(fields[i].label) | size(WIDTH, EQUAL, 18) | color(theme::text),
                inputs[i]->Render() | border | color(theme::primary) | flex,
            }));
        }
        lines.push_back(separator() | color(theme::border));
        lines.push_back(hbox({
            filler(),
            save->Render() | color(theme::success),
            text("  "),
            cancel->Render() |
                color(cancel_label == "Login" ? theme::primary : theme::danger),
            filler(),
        }));
        return vbox({
            filler(),
            hbox({
                filler(),
                titled("◆ " + title, vbox(std::move(lines)), theme::accent) | size(WIDTH, EQUAL, 64),
                filler(),
            }),
            filler(),
        }) | bgcolor(theme::background);
    });

    screen.Loop(renderer);
    return submitted;
}

Color option_color(const std::string& label) {
    if (label.find("Exit") != std::string::npos || label.find("Cancel") != std::string::npos ||
        label.find("Failed") != std::string::npos) return theme::danger;
    if (label.find("Register") != std::string::npos || label.find("Administrator") != std::string::npos ||
        label.find("Report") != std::string::npos) return theme::accent;
    if (label.find("Driver") != std::string::npos || label.find("Pending") != std::string::npos) return theme::warning;
    if (label.find("Customer") != std::string::npos || label.find("Delivered") != std::string::npos ||
        label.find("Available") != std::string::npos) return theme::success;
    return theme::primary;
}

Element brand_logo() {
    return text("FLEETFLOW") | bold | color(theme::primary);
}

Element access_logo() {
    return vbox({
        text("███████╗██╗     ███████╗███████╗████████╗███████╗██╗      ██████╗ ██╗    ██╗"),
        text("██╔════╝██║     ██╔════╝██╔════╝╚══██╔══╝██╔════╝██║     ██╔═══██╗██║    ██║"),
        text("█████╗  ██║     █████╗  █████╗     ██║   █████╗  ██║     ██║   ██║██║ █╗ ██║"),
        text("██╔══╝  ██║     ██╔══╝  ██╔══╝     ██║   ██╔══╝  ██║     ██║   ██║██║███╗██║"),
        text("██║     ███████╗███████╗███████╗   ██║   ██║     ███████╗╚██████╔╝╚███╔███╔╝"),
        text("╚═╝     ╚══════╝╚══════╝╚══════╝   ╚═╝   ╚═╝     ╚══════╝ ╚═════╝  ╚══╝╚══╝ "),
    }) | bold | color(theme::primary);
}

int choose(const std::string& title, const std::vector<std::string>& choices) {
    if (choices.empty()) {
        return -1;
    }
    ScreenInteractive& screen = shared_screen();
    int selected = 0;
    int result = -1;
    auto options = choices;
    const bool access_page = title == "FLEETFLOW";
    MenuOption menu_option;
    menu_option.entries_option.transform = [access_page](EntryState state) {
        const Color tone = option_color(state.label);
        Element entry;
        if (access_page) {
            entry = hbox({
                text(state.active ? " ▌ " : "   ") | color(tone) |
                    size(WIDTH, EQUAL, 5),
                text(state.label) | center | color(tone) | flex,
                text(state.active ? "  ◀ " : "    ") | color(tone) |
                    size(WIDTH, EQUAL, 5),
            });
        } else {
            entry = hbox({
                text(state.active ? " ▌ " : "   ") | color(tone),
                text(state.label) | color(tone) | flex,
                text(state.active ? "  ◀ " : "    ") | color(tone),
            });
        }
        if (state.active) entry = entry | bold | bgcolor(theme::selection);
        return entry;
    };
    menu_option.on_enter = [&] {
        result = selected;
        screen.ExitLoopClosure()();
    };
    auto menu = Menu(&options, &selected, menu_option);
    auto select = Button("Select", [&] {
        result = selected;
        screen.ExitLoopClosure()();
    }, outline_only_button());
    auto cancel = Button("Cancel", screen.ExitLoopClosure(), outline_only_button());
    auto container = Container::Vertical({menu, Container::Horizontal({select, cancel})});
    container = CatchEvent(container, [&](Event event) {
        if (event == Event::ArrowLeft || event == Event::ArrowRight) {
            const int direction = event == Event::ArrowLeft ? -1 : 1;
            selected = (selected + direction + static_cast<int>(options.size())) %
                       static_cast<int>(options.size());
            return true;
        }
        if (event != Event::Escape) return false;
        screen.ExitLoopClosure()();
        return true;
    });
    auto renderer = Renderer(container, [&] {
        Element dialog_body = vbox({
            menu->Render() | frame | size(HEIGHT, LESS_THAN, 16),
            separator() | color(theme::border),
            text("↑↓ or ←→ Navigate   Enter Select   Esc Back") | center | color(theme::muted),
            hbox({filler(), select->Render() | color(theme::success), text("  "),
                  cancel->Render() | color(theme::danger), filler()}),
        });
        Element dialog = access_page
                             ? vbox({
                                   text(" ACCESS") | center | bold | color(theme::primary),
                                   separator() | color(theme::border),
                                   std::move(dialog_body),
                               }) | border | color(theme::border) | bgcolor(theme::surface)
                             : titled("◆ " + title, std::move(dialog_body), theme::primary);
        return vbox({
            filler(),
            (access_page ? access_logo() : brand_logo()) | center,
            text(""),
            hbox({
                filler(),
                std::move(dialog) | size(WIDTH, EQUAL, 64),
                filler(),
            }),
            filler(),
        }) | bgcolor(theme::background);
    });
    screen.Loop(renderer);
    return result;
}

void notify(const std::string& title, const std::string& message, Color tone = theme::success) {
    ScreenInteractive& screen = shared_screen();
    auto close = Button("Continue", screen.ExitLoopClosure(), outline_only_button());
    auto renderer = Renderer(close, [&] {
        return vbox({
            filler(),
            hbox({
                filler(),
                titled(title, vbox({
                    text(message) | center | color(tone),
                    separator() | color(theme::border),
                    close->Render() | center,
                }), tone) | size(WIDTH, EQUAL, 60),
                filler(),
            }),
            filler(),
        }) | bgcolor(theme::background);
    });
    screen.Loop(renderer);
}

void save_with_notice(DataStore& store, const std::string& success_message) {
    const bool saved = store.save();
    notify(saved ? "SUCCESS" : "SAVE FAILED", saved ? success_message : store.lastError(),
           saved ? theme::success : theme::danger);
}

void loading_animation(const std::string& message, int duration_ms = 240) {
    auto screen = ScreenInteractive::Fullscreen();
    std::atomic<int> progress{0};
    auto renderer = Renderer([&] {
        const int value = progress.load();
        return vbox({
            filler(),
            hbox({filler(), titled("PROCESSING", vbox({
                text(message) | center | bold | color(theme::primary),
                separator() | color(theme::border),
                gauge(static_cast<float>(value) / 100.0F) | color(theme::success),
                text(std::to_string(value) + "%") | center | color(theme::muted),
            }), theme::accent) | size(WIDTH, EQUAL, 54), filler()}),
            filler(),
        }) | bgcolor(theme::background);
    });
    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Custom && progress.load() >= 100) {
            screen.ExitLoopClosure()();
        }
        return event == Event::Custom;
    });
    std::thread worker([&] {
        for (int value = 5; value <= 100; value += 5) {
            std::this_thread::sleep_for(std::chrono::milliseconds(duration_ms / 20));
            progress.store(value);
            screen.PostEvent(Event::Custom);
        }
    });
    screen.Loop(component);
    worker.join();
}

std::string hash_password_with_animation(const std::string& password,
                                         const std::string& message) {
    std::string encoded;
    std::thread worker([&] { encoded = security::hashPassword(password); });
    loading_animation(message);
    worker.join();
    return encoded;
}

bool confirm(const std::string& title, const std::string& message) {
    ScreenInteractive& screen = shared_screen();
    bool accepted = false;
    auto yes = Button("Yes, continue", [&] {
        accepted = true;
        screen.ExitLoopClosure()();
    }, outline_only_button());
    auto no = Button("Cancel", screen.ExitLoopClosure(), outline_only_button());
    auto container = Container::Horizontal({yes, no});
    auto renderer = Renderer(container, [&] {
        return vbox({
            filler(),
            hbox({
                filler(),
                titled(title, vbox({
                    paragraph(message) | center | color(theme::text),
                    separator() | color(theme::danger),
                    hbox({filler(), yes->Render() | color(theme::success), text("  "),
                          no->Render() | color(theme::danger), filler()}),
                }), theme::danger) | size(WIDTH, EQUAL, 62),
                filler(),
            }),
            filler(),
        }) | bgcolor(theme::background);
    });
    screen.Loop(renderer);
    return accepted;
}

std::vector<NavItem> navigation_for(const User& user) {
    if (user.role == "Administrator") {
        return {{"profile",  "👤  View Profile"},
                {"drivers",  "🪪  Driver Management"},
                {"vehicles", "🚛  Vehicle Management"},
                {"packages", "📦  Package Management"}, 
                {"dispatch", "⚡  Dispatch Packages"},
                {"tracking", "📍  Delivery Tracking"}, 
                {"reports",  "📊  Reports"},
                {"users",    "👥  User Management"}, 
                {"save",     "💾  Save Data"},
                {"load",     "📂  Load Data"}, 
                {"logout",   "🚪  Exit"}};
    }
    if (user.role == "Dispatcher") {
        return {{"profile",       "👤  View Profile"},
                {"drivers",       "🪪  View Drivers"},
                {"vehicles",      "🚛  View Vehicles"},
                {"addpackage",    "➕  Add Package"}, 
                {"dispatch",      "🎯  Assign Package"},
                {"tracking",      "🔄  Update Package Status"}, 
                {"searchpackage", "🔍  Search Package"},
                {"manifest",      "📋  Driver Manifest"}, 
                {"logout",        "🚪  Exit"}};
    }
    if (user.role == "Driver") {
        return {{"profile",    "👤  View Profile"},
                {"mypackages", "📦  View My Packages"},
                {"tracking",   "🔄  Update Delivery Status"},
                {"history",    "🕒  Delivery History"}, 
                {"logout",     "🚪  Exit"}};
    }
    return {{"profile",          "👤  My Profile"}, 
            {"customerpackages", "📦  My Packages"},
            {"customertracking", "📍  Track Delivery"}, 
            {"logout",           "🚪  Exit"}};
}
TableData table_for(DataStore& store, const User& user, const std::string& key,
                    const std::string& search) {
    TableData table;
    auto add = [&](int id, std::vector<std::string> row) {
        if (contains(row, search)) {
            table.ids.push_back(id);
            table.rows.push_back(std::move(row));
        }
    };

    if (key == "drivers" || key == "manifest") {
        table.headers = {"ID", "Name", "Phone", "License", "Status", "Vehicle"};
        table.widths = {7, 18, 14, 13, 13, 12};
        for (const auto& d : store.drivers) {
            add(d.id, {"DRV" + std::to_string(d.id), d.name, d.phone, d.licenseNo,
                       d.status, vehicle_name(store, d.vehicleId)});
        }
    } else if (key == "vehicles") {
        table.headers = {"ID", "Plate", "Type", "Capacity", "Status"};
        table.widths = {7, 16, 15, 14, 16};
        for (const auto& v : store.vehicles) {
            add(v.id, {"VEH" + std::to_string(v.id), v.plate, v.type,
                       number(v.capacityKg) + " kg", v.status});
        }
    } else if (key == "users" || key == "profile") {
        table.headers = {"ID", "Username", "Role", "Linked Driver"};
        table.widths = {7, 22, 20, 16};
        for (const auto& u : store.users) {
            if (key == "profile" && u.id != user.id) continue;
            add(u.id, {"USR" + std::to_string(u.id), u.username, u.role,
                       u.linkedId ? "DRV" + std::to_string(u.linkedId) : "-"});
        }
    } else {
        table.headers = {"ID", "Receiver", "Address", "Weight", "Status", "Driver"};
        table.widths = {7, 18, 25, 10, 13, 16};
        for (const auto& p : store.packages) {
            if (key == "dispatch" && p.status != "Pending") {
                continue;
            }
            if ((key == "mypackages" || key == "history" ||
                 (key == "tracking" && user.role == "Driver")) &&
                p.driverId != user.linkedId) {
                continue;
            }
            if ((key == "customerpackages" || key == "customertracking") &&
                p.customerId != user.id) continue;
            if (key == "history" && p.status != "Delivered" && p.status != "Failed" &&
                p.status != "Cancelled") {
                continue;
            }
            add(p.id, {"PKG" + std::to_string(p.id), p.receiverName, p.address,
                       number(p.weightKg) + " kg", p.status, driver_name(store, p.driverId)});
        }
    }
    return table;
}

Element table_element(const TableData& table, int selected) {
    auto make_row = [&](const std::vector<std::string>& cells, bool header, bool active) {
        Elements columns;
        for (std::size_t i = 0; i < cells.size(); ++i) {
            Element cell = text(cells[i]) | size(WIDTH, EQUAL, table.widths[i]) | xflex;
            if (header) {
                cell = cell | bold | color(theme::text);
            }
            columns.push_back(std::move(cell));
            if (i + 1 != cells.size()) {
                columns.push_back(separator() | color(theme::border));
            }
        }
        Element row = hbox(std::move(columns));
        if (active) {
            row = row | bold | color(theme::text) | bgcolor(theme::selection);
        } else if (!header) {
            row = row | color(theme::muted);
        }
        return row;
    };

    Elements rows;
    rows.push_back(make_row(table.headers, true, false));
    rows.push_back(separator() | color(theme::border));
    if (table.rows.empty()) {
        rows.push_back(text("No records match this view.") | center | color(theme::muted) | flex);
    } else {
        const int visible = 9;
        int first = std::max(0, selected - visible + 1);
        for (int i = first; i < static_cast<int>(table.rows.size()) && i < first + visible; ++i) {
            rows.push_back(make_row(table.rows[i], false, i == selected));
        }
    }
    return vbox(std::move(rows)) | frame;
}

TableData compact_table(const TableData& source) {
    if (source.headers.size() <= 3) {
        return source;
    }
    std::vector<std::size_t> columns = {0, 1};
    auto status = std::find(source.headers.begin(), source.headers.end(), "Status");
    columns.push_back(status == source.headers.end()
                          ? source.headers.size() - 1
                          : static_cast<std::size_t>(status - source.headers.begin()));
    TableData compact;
    compact.ids = source.ids;
    compact.widths = {8, 24, 14};
    for (std::size_t column : columns) {
        compact.headers.push_back(source.headers[column]);
    }
    for (const auto& row : source.rows) {
        compact.rows.push_back({row[columns[0]], row[columns[1]], row[columns[2]]});
    }
    return compact;
}

Element details_element(DataStore& store, const std::string& key, int id) {
    Elements lines;
    auto line = [&](const std::string& label, const std::string& value, Color value_color = theme::text) {
        lines.push_back(hbox({
            text(label) | size(WIDTH, EQUAL, 14) | color(theme::muted),
            text(value) | color(value_color),
        }));
    };
    if (key == "drivers" || key == "manifest") {
        if (auto* d = store.findDriver(id)) {
            line("Driver ID", "DRV" + std::to_string(d->id));
            line("Name", d->name);
            line("Phone", d->phone);
            line("License", d->licenseNo);
            line("Status", d->status, status_color(d->status));
            line("Vehicle", vehicle_name(store, d->vehicleId));
        }
    } else if (key == "vehicles") {
        if (auto* v = store.findVehicle(id)) {
            line("Vehicle ID", "VEH" + std::to_string(v->id));
            line("Plate", v->plate);
            line("Type", v->type);
            line("Capacity", number(v->capacityKg) + " kg");
            line("Status", v->status, status_color(v->status));
        }
    } else if (key == "users" || key == "profile") {
        if (auto* u = store.findUser(id)) {
            line("User ID", "USR" + std::to_string(u->id));
            line("Username", u->username);
            line("Role", u->role, theme::primary);
            line("Linked", u->linkedId ? "DRV" + std::to_string(u->linkedId) : "-");
        }
    } else if (auto* p = store.findPackage(id)) {
        line("Package ID", "PKG" + std::to_string(p->id));
        line("Receiver", p->receiverName);
        line("Phone", p->receiverPhone);
        line("Address", p->address);
        line("Status", p->status, status_color(p->status));
        line("Driver", driver_name(store, p->driverId));
        line("Vehicle", vehicle_name(store, p->vehicleId));
    }
    if (lines.empty()) {
        lines.push_back(text("Select a record to see its details.") | color(theme::muted));
    }
    return vbox(std::move(lines));
}

Element activity_element(DataStore& store) {
    struct Activity {
        std::string timestamp;
        std::string text;
        Color tone;
    };
    std::vector<Activity> activity;
    for (const auto& p : store.packages) {
        for (const auto& event : p.history) {
            activity.push_back({
                event.timestamp,
                "PKG" + std::to_string(p.id) + "  " + event.status +
                    (event.note.empty() ? "" : " - " + event.note),
                status_color(event.status),
            });
        }
    }
    std::sort(activity.begin(), activity.end(), [](const Activity& a, const Activity& b) {
        return a.timestamp > b.timestamp;
    });
    Elements lines;
    for (std::size_t i = 0; i < activity.size() && i < 6; ++i) {
        lines.push_back(hbox({
            text(activity[i].timestamp) | size(WIDTH, EQUAL, 21) | color(theme::muted),
            text(activity[i].text) | color(activity[i].tone),
        }));
    }
    if (lines.empty()) {
        lines.push_back(text("No recent activity.") | color(theme::muted));
    }
    return vbox(std::move(lines));
}

std::string actions_for(const std::string& key, const User& user) {
    if (key == "drivers" && user.role == "Administrator") return "[A] Add  [E] Edit  [D] Delete  [S] Search";
    if (key == "vehicles" && user.role == "Administrator") return "[A] Add  [E] Edit  [D] Delete  [S] Search";
    if (key == "users") return "[ENTER/V] View profile  [A] Add  [D] Delete  [S] Search";
    if (key == "packages") return "[A] Add  [D] Delete  [V] History  [S] Search";
    if (key == "addpackage") return "[ENTER] Add package";
    if (key == "dispatch") return "[ENTER/D] Assign selected package  [S] Search";
    if (key == "searchpackage") return "[ENTER/S] Search packages";
    if (key == "manifest") return "[ENTER/V] View selected driver manifest";
    if (key == "tracking" && user.role != "Customer") return "[ENTER/U] Update status  [V] History  [S] Search";
    if (key == "mypackages" || key == "history" || key == "customerpackages" || key == "customertracking") return "[ENTER/V] View tracking history  [S] Search";
    if (key == "profile") return "[ENTER] View profile  [ESC] Back";
    if (key == "save") return "[ENTER] Save database";
    if (key == "load") return "[ENTER] Reload database";
    if (key == "logout") return "[ENTER] Exit to Login / Register";
    return "[S] Search  [R] Refresh";
}

Color navigation_color(const std::string& key) {
    if (key == "drivers" || key == "profile" || key == "mypackages") return theme::primary;
    if (key == "vehicles" || key == "addpackage" || key == "customerpackages") return theme::success;
    if (key == "packages" || key == "dispatch" || key == "manifest") return theme::warning;
    if (key == "tracking" || key == "customertracking" || key == "reports") return theme::accent;
    if (key == "logout") return theme::danger;
    return theme::text;
}

class DashboardComponent final : public ComponentBase {
public:
    DashboardComponent(DataStore& store, User& user, DashboardState& state,
                       ScreenInteractive& screen)
        : store_(store), user_(user), state_(state), screen_(screen),
          nav_(navigation_for(user)) {}

    Element OnRender() override {
        const std::string key = nav_[state_.nav].key;
        TableData table = table_for(store_, user_, key, state_.search);
        clamp_row(table);
        const int id = table.ids.empty() ? 0 : table.ids[state_.row];
        state_.selected_id = id;

        Elements menu;
        for (int i = 0; i < static_cast<int>(nav_.size()); ++i) {
            const Color tone = navigation_color(nav_[i].key);
            Element entry = hbox({
                text(i == state_.nav ? "▌ " : "  ") | color(tone),
                text(nav_[i].label) | color(tone) | flex,
            });
            if (i == state_.nav) entry = entry | bold | bgcolor(theme::selection);
            menu.push_back(std::move(entry));
        }

        int available_drivers = 0;
        int active_drivers = 0;
        for (const auto& d : store_.drivers) {
            available_drivers += d.status == "Available";
            active_drivers += d.status == "OnDelivery";
        }
        int delivered = 0;
        int active_packages = 0;
        for (const auto& p : store_.packages) {
            delivered += p.status == "Delivered";
            active_packages += p.status == "Assigned" || p.status == "InTransit";
        }

        Element sidebar = vbox({
            titled("📋 MAIN MENU", vbox(std::move(menu)), theme::warning) | flex,
            titled("🖥️ SYSTEM INFO", vbox({
                hbox({text("System") | flex, text("🟢 Online") | color(theme::success)}),
                hbox({text("Version") | flex, text("3.0") | color(theme::primary)}),
                hbox({text("Drivers") | flex, text(std::to_string(store_.drivers.size())) | color(theme::primary)}),
                hbox({text("Vehicles") | flex, text(std::to_string(store_.vehicles.size())) | color(theme::success)}),
                hbox({text("Packages") | flex, text(std::to_string(store_.packages.size())) | color(theme::warning)}),
            }), theme::primary),
            titled(" ⌨️  SHORTCUTS", vbox({
                text("↑/↓  Navigate") | color(theme::primary),
                text("←/→  Switch panel") | color(theme::accent),
                text("↵    Select") | color(theme::success),
                text("Esc  Go back") | color(theme::warning),
            }), theme::accent),
        });

        Element stats = titled("▥ LIVE STATISTICS", vbox({
            text("DRIVER AVAILABILITY") | bold | color(theme::primary),
            metric("Available", available_drivers, static_cast<int>(store_.drivers.size()), theme::success),
            metric("On delivery", active_drivers, static_cast<int>(store_.drivers.size()), theme::warning),
            separator() | color(theme::border),
            text("PACKAGE PROGRESS") | bold | color(theme::primary),
            metric("Active", active_packages, static_cast<int>(store_.packages.size()), theme::primary),
            metric("Delivered", delivered, static_cast<int>(store_.packages.size()), theme::success),
            separator() | color(theme::border),
            hbox({
                text("TOTAL DRIVERS") | color(theme::muted) | flex,
                text(std::to_string(store_.drivers.size())) | bold | color(theme::success),
            }),
        })) | size(WIDTH, EQUAL, 31);

        Element wide_content = vbox({
            hbox({
                text(nav_[state_.nav].label) | bold | color(theme::primary) | flex,
                text(state_.search.empty() ? "🔍 Search: Not filtered" : "🔍 Search: " + state_.search) |
                    color(state_.search.empty() ? theme::muted : theme::warning),
            }),
            hbox({
                titled("📋 RECORDS", table_element(table, state_.row), theme::primary) | flex,
                stats,
            }) | flex,
            hbox({
                titled("📑 SELECTED DETAILS", details_element(store_, key, id), theme::warning) | flex,
                titled("🕒 RECENT ACTIVITIES", activity_element(store_), theme::accent) | flex,
            }) | size(HEIGHT, EQUAL, 10),
            text(actions_for(key, user_)) | center | bold | color(theme::primary) |
                border | color(theme::border),
        }) | flex;
const TableData compact = compact_table(table);
        Element compact_content = vbox({
            hbox({
                text(nav_[state_.nav].label) | bold | color(theme::primary) | flex,
                text(state_.search.empty() ? "" : "Filter: " + state_.search) |
                    color(theme::warning),
            }),
            titled("▤ RECORDS", table_element(compact, state_.row), theme::primary) | flex,
            text(actions_for(key, user_)) | center | bold | color(theme::primary) |
                border | color(theme::border),
        }) | flex;

        // The wide view needs room for the sidebar, full record table, and
        // statistics panel. Selecting it at 120 columns over-constrains FTXUI
        // and can collapse the dashboard to a few glyphs in VS Code terminals.
        constexpr int kWideDashboardMinColumns = 165;
        const bool compact_mode = Terminal::Size().dimx < kWideDashboardMinColumns;
        Element header_content =
            compact_mode
                ? hbox({
                      text(" FLEETFLOW") | bold | color(theme::primary) | flex,
                      text("  👤 " + user_.username) | color(theme::text),
                      text("  🟢 ONLINE") | color(theme::success),
                  })
                : hbox({
                      text("🕒 " + utils::now()) | color(theme::warning),
                      filler(),
                      text(" FLEETFLOW") | bold | color(theme::primary),
                      filler(),
                      text("👤 " + user_.username + " | " + user_.role) |
                          color(theme::text),
                      text("  🟢 ONLINE") | color(theme::success),
                  });

        Element header =
            header_content | border | color(theme::border) | size(HEIGHT, EQUAL, 3);

        Element footer = hbox({
            text("⚡ " + state_.notice) | color(theme::success),
            filler(),
            text(" 🗁  Database: " + store_.filePath) | color(theme::muted),
        }) | border | color(theme::border) | size(HEIGHT, EQUAL, 3);

        Element body = compact_mode
                           ? hbox({sidebar | size(WIDTH, EQUAL, 25), compact_content})
                           : hbox({sidebar | size(WIDTH, EQUAL, 30), wide_content});
        return vbox({
            header,
            body | flex,
            footer,
        }) | bgcolor(theme::background) | color(theme::text);
    }

    bool OnEvent(Event event) override {
        const std::string key = nav_[state_.nav].key;
        if (event == Event::ArrowLeft || event == Event::ArrowRight ||
            event == Event::Tab || event == Event::TabReverse) {
            state_.focus = 1 - state_.focus;
            state_.notice = state_.focus == 0 ? "Menu focused" : "Records focused";
            return true;
        }
        if (event == Event::ArrowUp || event == Event::ArrowDown) {
            const int direction = event == Event::ArrowUp ? -1 : 1;
            if (state_.focus == 0) {
                state_.nav = (state_.nav + direction + static_cast<int>(nav_.size())) %
                             static_cast<int>(nav_.size());
                state_.row = 0;
                state_.search.clear();
            } else {
                TableData table = table_for(store_, user_, key, state_.search);
                if (!table.rows.empty()) {
                    state_.row = (state_.row + direction + static_cast<int>(table.rows.size())) %
                                 static_cast<int>(table.rows.size());
                }
            }
            return true;
        }
        if (event == Event::Escape) {
            trigger(Action::Logout);
            return true;
        }
        if (event == Event::Return) {
            if (key == "save") trigger(Action::Save);
            else if (key == "load") trigger(Action::Load);
            else if (key == "logout") trigger(Action::Logout);
            else if (key == "addpackage") trigger(Action::Add);
            else if (key == "searchpackage") trigger(Action::Search);
            else if (key == "dispatch") trigger(Action::Dispatch);
            else if (key == "tracking") trigger(Action::Status);
            else trigger(Action::View);
            return true;
        }
        if (!event.is_character()) {
            return false;
        }
        const std::string c = lower(event.character());
        if (c == "a") trigger(Action::Add);
        else if (c == "e") trigger(Action::Edit);
        else if (c == "d") {
            trigger(key == "dispatch" ? Action::Dispatch : Action::Delete);
        } else if (c == "u") trigger(Action::Status);
        else if (c == "v") trigger(Action::View);
        else if (c == "s" || c == "/") trigger(Action::Search);
        else if (c == "r") {
            state_.notice = "Dashboard refreshed";
            return true;
        } else {
            return false;
        }
        return true;
    }

private:
    void clamp_row(const TableData& table) {
        if (table.rows.empty()) {
            state_.row = 0;
        } else {
            state_.row = std::clamp(state_.row, 0, static_cast<int>(table.rows.size()) - 1);
        }
    }

    void trigger(Action action) {
        state_.action = action;
        screen_.ExitLoopClosure()();
    }

    DataStore& store_;
    User& user_;
    DashboardState& state_;
    ScreenInteractive& screen_;
    std::vector<NavItem> nav_;
};

std::string current_key(const User& user, const DashboardState& state) {
    const auto nav = navigation_for(user);
    return nav[std::clamp(state.nav, 0, static_cast<int>(nav.size()) - 1)].key;
}

void show_history(DataStore& store, int package_id) {
    auto* package = store.findPackage(package_id);
    if (!package) {
        notify("NOT FOUND", "Select a package first.", theme::danger);
        return;
    }
    std::vector<std::string> entries;
    for (const auto& event : package->history) {
        entries.push_back(event.timestamp + "  " + event.status +
                          (event.note.empty() ? "" : " - " + event.note));
    }
    if (entries.empty()) {
        notify("PACKAGE HISTORY", "No tracking events are available.", theme::warning);
        return;
    }
    choose("HISTORY - PKG" + std::to_string(package_id), entries);
}

void show_manifest(DataStore& store, int driver_id) {
    Driver* driver = store.findDriver(driver_id);
    if (!driver) {
        notify("DRIVER MANIFEST", "Select a driver first.", theme::warning);
        return;
    }
    std::vector<std::string> packages;
    for (const auto& package : store.packages) {
        if (package.driverId == driver_id) {
            packages.push_back("PKG" + std::to_string(package.id) + "  " +
                               package.receiverName + "  |  " + package.address +
                               "  |  " + package.status);
        }
    }
    if (packages.empty()) packages.push_back("No packages assigned to this driver.");
    choose("MANIFEST - " + driver->name, packages);
}

void show_profile(DataStore& store, const User& user) {
    ScreenInteractive& screen = shared_screen();
    auto close = Button("Close", screen.ExitLoopClosure(), outline_only_button());
    auto container = CatchEvent(close, [&](Event event) {
        if (event != Event::Escape) return false;
        screen.ExitLoopClosure()();
        return true;
    });

    auto renderer = Renderer(container, [&] {
        Elements lines;
        auto line = [&](const std::string& label, const std::string& value,
                        Color value_color = theme::text) {
            lines.push_back(hbox({
                text(label) | size(WIDTH, EQUAL, 18) | color(theme::muted),
                text(value) | color(value_color) | flex,
            }));
        };

        line("User ID", "USR" + std::to_string(user.id), theme::primary);
        line("Username", user.username);
        line("Role", user.role, option_color(user.role));

        if (user.role == "Driver") {
            lines.push_back(separator() | color(theme::border));
            if (Driver* driver = store.findDriver(user.linkedId)) {
                line("Driver ID", "DRV" + std::to_string(driver->id), theme::primary);
                line("Full name", driver->name);
                line("Phone", driver->phone.empty() ? "Not provided" : driver->phone);
                line("License", driver->licenseNo);
                line("Status", driver->status, status_color(driver->status));
                line("Vehicle", vehicle_name(store, driver->vehicleId));
            } else {
                line("Driver record", "Not linked", theme::warning);
            }
        }

        if (user.role == "Customer") {
            int total = 0;
            int active = 0;
            int delivered = 0;
            for (const auto& package : store.packages) {
                if (package.customerId != user.id) continue;
                ++total;
                active += package.status == "Pending" || package.status == "Assigned" ||
                          package.status == "InTransit";
                delivered += package.status == "Delivered";
            }
            lines.push_back(separator() | color(theme::border));
            line("Total packages", std::to_string(total), theme::primary);
            line("Active deliveries", std::to_string(active), theme::warning);
            line("Delivered", std::to_string(delivered), theme::success);
        }

        lines.push_back(separator() | color(theme::border));
        lines.push_back(text("Your password is securely hidden.") |
                        center | color(theme::muted));
        lines.push_back(close->Render() | center | color(theme::primary));

        return vbox({
            filler(),
            hbox({
                filler(),
                titled("👤 MY PROFILE", vbox(std::move(lines)), theme::primary) |
                    size(WIDTH, EQUAL, 58),
                filler(),
            }),
            filler(),
        }) | bgcolor(theme::background);
    });
    screen.Loop(renderer);
}

void add_driver(DataStore& store) {
    std::vector<FormField> fields = {
        {"Name", "", "Full name"},
        {"Phone", "", "Phone number"},
        {"License", "", "License number"},
    };
    if (!run_form("ADD NEW DRIVER", fields)) return;
    const std::string name = utils::trim(fields[0].value);
    const std::string phone = utils::trim(fields[1].value);
    const std::string license = utils::trim(fields[2].value);
    if (name.empty()) {
        notify("VALIDATION", "Driver name is required.", theme::danger);
        return;
    }
    for (const auto& check : {validation::phone(phone, true), validation::license(license),
                              validation::uniqueLicense(store.drivers, license)}) {
        if (!check.ok) { notify("VALIDATION", check.message, theme::danger); return; }
    }
    Driver driver;
    driver.id = utils::nextId(store.drivers);
    driver.name = name;
    driver.phone = phone;
    driver.licenseNo = license;
    store.drivers.push_back(driver);
    const bool saved = store.save();
    notify(saved ? "SUCCESS" : "SAVE FAILED",
           saved ? "Driver " + driver.name + " was added." : store.lastError(),
           saved ? theme::success : theme::danger);
}

void edit_driver(DataStore& store, int id) {
    auto* driver = store.findDriver(id);
    if (!driver) return;
    std::vector<FormField> fields = {
        {"Name", driver->name, "Full name"},
        {"Phone", driver->phone, "Phone number"},
        {"License", driver->licenseNo, "License number"},
    };
    if (!run_form("EDIT DRIVER", fields)) return;
    const std::string name = utils::trim(fields[0].value);
    const std::string phone = utils::trim(fields[1].value);
    const std::string license = utils::trim(fields[2].value);
    if (name.empty()) { notify("VALIDATION", "Driver name is required.", theme::danger); return; }
    for (const auto& check : {validation::phone(phone, true), validation::license(license),
                              validation::uniqueLicense(store.drivers, license, id)}) {
        if (!check.ok) { notify("VALIDATION", check.message, theme::danger); return; }
    }
    driver->name = name;
    driver->phone = phone;
    driver->licenseNo = license;
    const int status = choose("DRIVER STATUS", {"Available", "OnDelivery", "OffDuty"});
    if (status >= 0) driver->status = std::vector<std::string>{"Available", "OnDelivery", "OffDuty"}[status];
    save_with_notice(store, "Driver details were updated.");
}

void add_vehicle(DataStore& store) {
    std::vector<FormField> fields = {
        {"Plate", "", "Registration plate"},
        {"Type", "", "Van, Truck, Motorbike"},
        {"Capacity (kg)", "", "Maximum load"},
    };
    if (!run_form("ADD NEW VEHICLE", fields)) return;
    const std::string plate = utils::trim(fields[0].value);
    const std::string type = utils::trim(fields[1].value);
    double capacity = 0;
    for (const auto& check : {validation::plate(plate), validation::uniquePlate(store.vehicles, plate),
                              validation::positiveNumber(fields[2].value, "Capacity", capacity)}) {
        if (!check.ok) { notify("VALIDATION", check.message, theme::danger); return; }
    }
    if (type.empty()) { notify("VALIDATION", "Vehicle type is required.", theme::danger); return; }
    Vehicle vehicle;
    vehicle.id = utils::nextId(store.vehicles);
    vehicle.plate = plate;
    vehicle.type = type;
    vehicle.capacityKg = capacity;
    store.vehicles.push_back(vehicle);
    save_with_notice(store, "Vehicle " + vehicle.plate + " was added.");
}

void edit_vehicle(DataStore& store, int id) {
    auto* vehicle = store.findVehicle(id);
    if (!vehicle) return;
    std::vector<FormField> fields = {
        {"Plate", vehicle->plate, "Registration plate"},
        {"Type", vehicle->type, "Vehicle type"},
        {"Capacity (kg)", number(vehicle->capacityKg), "Maximum load"},
    };
    if (!run_form("EDIT VEHICLE", fields)) return;
    const std::string plate = utils::trim(fields[0].value);
    const std::string type = utils::trim(fields[1].value);
    double capacity = 0;
    for (const auto& check : {validation::plate(plate), validation::uniquePlate(store.vehicles, plate, id),
                              validation::positiveNumber(fields[2].value, "Capacity", capacity)}) {
        if (!check.ok) { notify("VALIDATION", check.message, theme::danger); return; }
    }
    if (type.empty()) { notify("VALIDATION", "Vehicle type is required.", theme::danger); return; }
    vehicle->plate = plate;
    vehicle->type = type;
    vehicle->capacityKg = capacity;
    const int status = choose("VEHICLE STATUS", {"Available", "InUse", "Maintenance"});
    if (status >= 0) vehicle->status = std::vector<std::string>{"Available", "InUse", "Maintenance"}[status];
    save_with_notice(store, "Vehicle details were updated.");
}

void add_package(DataStore& store) {
    std::vector<FormField> fields = {
        {"Sender", "", "Sender name"},
        {"Receiver", "", "Receiver name"},
        {"Phone", "", "Receiver phone"},
        {"Address", "", "Delivery address"},
        {"Weight (kg)", "", "Package weight"},
    };
    if (!run_form("ADD NEW PACKAGE", fields)) return;
    const std::string sender = utils::trim(fields[0].value);
    const std::string receiver = utils::trim(fields[1].value);
    const std::string phone = utils::trim(fields[2].value);
    const std::string address = utils::trim(fields[3].value);
    double weight = 0;
    if (sender.empty() || receiver.empty() || address.empty()) {
        notify("VALIDATION", "Sender, receiver, and address are required.", theme::danger);
        return;
    }
    for (const auto& check : {validation::phone(phone, true),
                              validation::positiveNumber(fields[4].value, "Weight", weight)}) {
        if (!check.ok) { notify("VALIDATION", check.message, theme::danger); return; }
    }
    Package package;
    package.id = utils::nextId(store.packages);
    package.senderName = sender;
    package.receiverName = receiver;
    package.receiverPhone = phone;
    package.address = address;
    package.weightKg = weight;
    package.createdAt = utils::now();
    package.history.push_back({"Pending", package.createdAt, "Package created"});
    std::vector<User*> customers;
    std::vector<std::string> customer_options = {"Unassigned customer"};
    for (auto& account : store.users) {
        if (account.role == "Customer") {
            customers.push_back(&account);
            customer_options.push_back(account.username);
        }
    }
    const int customer = choose("PACKAGE CUSTOMER", customer_options);
    if (customer < 0) return;
    if (customer > 0) package.customerId = customers[customer - 1]->id;
    store.packages.push_back(package);
    save_with_notice(store, "Package PKG" + std::to_string(package.id) + " was created.");
}

void add_user(DataStore& store) {
    std::vector<FormField> fields = {
        {"Username", "", "Unique username"},
        {"Password", "", "Password", true},
        {"Phone", "", "Required for driver accounts"},
        {"License", "", "Required for driver accounts"},
    };
    if (!run_form("ADD NEW USER", fields)) return;
    const std::string username = utils::trim(fields[0].value);
    for (const auto& check : {validation::username(username), validation::password(fields[1].value),
                              validation::uniqueUsername(store.users, username)}) {
        if (!check.ok) { notify("VALIDATION", check.message, theme::danger); return; }
    }
    const int role = choose("USER ROLE", {"♛ Administrator", "◆ Dispatcher", "▰ Driver", "● Customer"});
    if (role < 0) return;
    if (role == 2) {
        for (const auto& check : {validation::phone(fields[2].value, true),
                                  validation::license(fields[3].value),
                                  validation::uniqueLicense(store.drivers, fields[3].value)}) {
            if (!check.ok) { notify("VALIDATION", check.message, theme::danger); return; }
        }
    }
    User user;
    user.id = utils::nextId(store.users);
    user.username = username;
    user.password = hash_password_with_animation(fields[1].value,
                                                 "Securing new user account...");
    user.role = std::vector<std::string>{"Administrator", "Dispatcher", "Driver", "Customer"}[role];
    if (user.role == "Driver") {
        Driver driver;
        driver.id = utils::nextId(store.drivers);
        driver.name = user.username;
        driver.phone = utils::trim(fields[2].value);
        driver.licenseNo = utils::trim(fields[3].value);
        store.drivers.push_back(driver);
        user.linkedId = driver.id;
    }
    store.users.push_back(user);
    save_with_notice(store, "User " + user.username + " was created.");
}

void dispatch_package(DataStore& store, int package_id, const User& actor) {
    Package* package = store.findPackage(package_id);
    if (!package || package->status != "Pending") {
        notify("DISPATCH", "Select a pending package first.", theme::warning);
        return;
    }
    std::vector<Driver*> drivers;
    std::vector<std::string> driver_options;
    for (auto& driver : store.drivers) {
        if (driver.status == "Available") {
            drivers.push_back(&driver);
            driver_options.push_back("DRV" + std::to_string(driver.id) + "  " + driver.name);
        }
    }
    std::vector<Vehicle*> vehicles;
    std::vector<std::string> vehicle_options;
    for (auto& vehicle : store.vehicles) {
        if (vehicle.status == "Available" && vehicle.capacityKg >= package->weightKg) {
            vehicles.push_back(&vehicle);
            vehicle_options.push_back("VEH" + std::to_string(vehicle.id) + "  " + vehicle.plate +
                                      " (" + number(vehicle.capacityKg) + " kg)");
        }
    }
    if (drivers.empty() || vehicles.empty()) {
        notify("NO CAPACITY", "No available driver or suitable vehicle was found.", theme::danger);
        return;
    }
    const int driver = choose("SELECT DRIVER", driver_options);
    if (driver < 0) return;
    const int vehicle = choose("SELECT VEHICLE", vehicle_options);
    if (vehicle < 0) return;
    const auto result = delivery::dispatch(store, package_id, drivers[driver]->id,
                                           vehicles[vehicle]->id, actor);
    const bool saved = result.ok && store.save();
    notify(saved ? "DISPATCHED" : "DISPATCH FAILED",
           result.ok ? (saved ? result.message : store.lastError()) : result.message,
           saved ? theme::success : theme::danger);
}

void update_status(DataStore& store, int package_id, const User& user) {
    Package* package = store.findPackage(package_id);
    if (!package || (user.role == "Driver" && package->driverId != user.linkedId)) {
        notify("STATUS", "Select one of your assigned packages first.", theme::warning);
        return;
    }
    std::vector<std::string> options = delivery::nextStatuses(package->status);
    if (user.role == "Driver") {
        options.erase(std::remove(options.begin(), options.end(), "Cancelled"), options.end());
    }
    if (options.empty()) {
        notify("STATUS", "No valid status update is available from " + package->status + ".",
               theme::warning);
        return;
    }
    const int selected = choose("UPDATE PACKAGE STATUS", options);
    if (selected < 0) return;
    const std::string status = options[selected];
    std::vector<FormField> note = {{"Note", "", "Optional status note"}};
    if (!run_form("STATUS NOTE", note, "Update")) return;
    const auto result = delivery::updateStatus(store, package_id, status,
                                               utils::trim(note[0].value), user);
    const bool saved = result.ok && store.save();
    notify(saved ? "STATUS UPDATED" : "STATUS FAILED",
           result.ok ? (saved ? result.message : store.lastError()) : result.message,
           saved ? theme::success : theme::danger);
}

void delete_record(DataStore& store, const std::string& key, int id, const User& current_user) {
    if (id == 0) return;
    if (!confirm("DELETE RECORD", "This operation cannot be undone. Delete the selected record?")) return;
    delivery::Result result{false, "Unsupported record type."};
    if (key == "drivers" || key == "manifest") result = delivery::deleteDriver(store, id);
    else if (key == "vehicles") result = delivery::deleteVehicle(store, id);
    else if (key == "packages") result = delivery::deletePackage(store, id);
    else if (key == "users" || key == "profile") {
        result = delivery::deleteUser(store, id, current_user.id);
    }
    const bool saved = result.ok && store.save();
    notify(saved ? "DELETED" : "DELETE FAILED",
           result.ok ? (saved ? result.message : store.lastError()) : result.message,
           saved ? theme::success : theme::danger);
}

void handle_action(DataStore& store, User& user, DashboardState& state) {
    const std::string key = current_key(user, state);
    loading_animation("Opening " + key + "...");
    switch (state.action) {
        case Action::Add:
            if (key == "drivers" && user.role == "Administrator") add_driver(store);
            else if (key == "vehicles" && user.role == "Administrator") add_vehicle(store);
            else if (key == "packages" || key == "addpackage") add_package(store);
            else if (key == "users" && user.role == "Administrator") add_user(store);
            break;
        case Action::Edit:
            if (key == "drivers" && user.role == "Administrator") edit_driver(store, state.selected_id);
            else if (key == "vehicles" && user.role == "Administrator") edit_vehicle(store, state.selected_id);
            break;
        case Action::Delete:
            if (user.role == "Administrator" ||
                (user.role == "Dispatcher" && key == "packages")) {
                delete_record(store, key, state.selected_id, user);
            }
            break;
        case Action::Dispatch:
            dispatch_package(store, state.selected_id, user);
            break;
        case Action::Status:
            if (user.role != "Customer") update_status(store, state.selected_id, user);
            break;
        case Action::View:
            if (key == "profile") {
                show_profile(store, user);
            } else if (key == "users" && user.role == "Administrator") {
                if (User* selected_user = store.findUser(state.selected_id)) {
                    show_profile(store, *selected_user);
                } else {
                    notify("VIEW PROFILE", "Select a user account first.", theme::warning);
                }
            } else if (key == "manifest") {
                show_manifest(store, state.selected_id);
            } else if (key == "tracking" || key == "history" || key == "mypackages" ||
                       key == "packages" || key == "customerpackages" ||
                       key == "customertracking") {
                show_history(store, state.selected_id);
            }
            break;
        case Action::Search: {
            std::vector<FormField> field = {{"Search", state.search, "ID, name, status, address"}};
            if (run_form("SEARCH RECORDS", field, "Apply")) {
                state.search = utils::trim(field[0].value);
                state.row = 0;
            }
            break;
        }
        case Action::Save: {
            const bool saved = store.save();
            notify("SAVE DATA", saved ? "Database saved successfully." : store.lastError(),
                   saved ? theme::success : theme::danger);
            break;
        }
        case Action::Load:
            if (confirm("LOAD DATA", "Reload the database and discard unsaved in-memory changes?")) {
                const bool loaded = store.load();
                notify("LOAD DATA", loaded ? "Database reloaded successfully." : store.lastError(),
                       loaded ? theme::success : theme::danger);
            }
            break;
        default:
            break;
    }
    state.notice = "Last action completed";
}

void dashboard(DataStore& store, User& user) {
    const int current_user_id = user.id;
    DashboardState state;
    while (true) {
        User* current_user = store.findUser(current_user_id);
        if (!current_user) {
            return;
        }
        state.action = Action::None;
        ScreenInteractive& screen = shared_screen();
        auto component = Make<DashboardComponent>(store, *current_user, state, screen);
        screen.Loop(component);
        if (state.action == Action::Logout) {
            if (confirm("LOG OUT", "End the current session and return to the sign-in screen?")) {
                return;
            }
            continue;
        }
        handle_action(store, *current_user, state);
        if (state.action == Action::Load) {
            return;
        }
    }
}

bool verify_password_with_animation(const std::string& password,
                                    const std::string& stored_password,
                                    const std::string& message) {
    auto screen = ScreenInteractive::Fullscreen();
    std::atomic<bool> finished = false;
    std::atomic<int> progress = 0;
    bool verified = false;

    auto renderer = Renderer([&] {
        const int value = progress.load();
        return vbox({
            filler(),
            hbox({
                filler(),
                titled("PROCESSING", vbox({
                    text(message) | center | bold | color(theme::primary),
                    separator() | color(theme::border),
                    gauge(static_cast<float>(value) / 100.0F) | color(theme::success),
                    text(std::to_string(value) + "%") | center | color(theme::muted),
                }), theme::accent) | size(WIDTH, EQUAL, 54),
                filler(),
            }),
            filler(),
        }) | bgcolor(theme::background);
    });

    auto component = CatchEvent(renderer, [&](Event event) {
        if (event != Event::Custom) return false;
        if (progress.load() >= 100) {
            screen.ExitLoopClosure()();
        }
        return true;
    });

    std::thread verifier([&] {
        verified = security::verifyPassword(password, stored_password);
        finished = true;
    });
    std::thread animator([&] {
        const auto started = std::chrono::steady_clock::now();
        while (!finished.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(18));
            progress.store(std::min(95, progress.load() + 5));
            screen.PostEvent(Event::Custom);
        }
        const auto elapsed = std::chrono::steady_clock::now() - started;
        const auto minimum = std::chrono::milliseconds(240);
        if (elapsed < minimum) {
            std::this_thread::sleep_for(minimum - elapsed);
        }
        progress.store(100);
        screen.PostEvent(Event::Custom);
    });

    screen.Loop(component);
    verifier.join();
    animator.join();
    return verified;
}

User* login(DataStore& store) {
    while (true) {
        std::vector<FormField> fields = {
            {"👤 Username", "", "Account username"},
            {"🔒 Password", "", "Account password", true},
        };
        if (!run_form("SIGN IN", fields, "Login")) return nullptr;
        User* user = store.findUserByName(utils::trim(fields[0].value));
        if (user && verify_password_with_animation(
                        fields[1].value, user->password,
                        "Signing in as " + user->role + "...")) {
            if (security::needsPasswordUpgrade(user->password)) {
                user->password = hash_password_with_animation(
                    fields[1].value, "Upgrading account security...");
                if (!store.save()) {
                    notify("SECURITY WARNING", "Signed in, but the upgraded password could not be saved: " +
                           store.lastError(), theme::warning);
                }
            }
            return user;
        }
        notify("LOGIN FAILED", "The username or password is incorrect.", theme::danger);
    }
}

bool register_user(DataStore& store) {
    std::vector<FormField> fields = {
        {"👤 Username", "", "Choose a username"},
        {"🔒 Password", "", "Choose a password", true},
        {"📞 Phone",    "", "Required for driver accounts"},
        {"🪪 License",  "", "Required for driver accounts"},
    };
    bool login_requested = false;
    if (!run_form("CREATE ACCOUNT", fields, "Register", "Login", &login_requested)) {
        return login_requested;
    }
    const std::string username = utils::trim(fields[0].value);
    for (const auto& check : {validation::username(username), validation::password(fields[1].value),
                              validation::uniqueUsername(store.users, username)}) {
        if (!check.ok) { notify("REGISTRATION", check.message, theme::danger); return false; }
    }
    const std::vector<std::string> roles = {"Driver", "Customer"};
    const int role = choose("ACCOUNT TYPE", {"🚚 Driver", "👤 Customer"});
    if (role < 0) return false;
    if (roles[role] == "Driver") {
        for (const auto& check : {validation::phone(fields[2].value, true),
                                  validation::license(fields[3].value),
                                  validation::uniqueLicense(store.drivers, fields[3].value)}) {
            if (!check.ok) { notify("REGISTRATION", check.message, theme::danger); return false; }
        }
    }
    User user;
    user.id = utils::nextId(store.users);
    user.username = username;
    user.password = hash_password_with_animation(fields[1].value,
                                                 "Securing new account...");
    user.role = roles[role];
    if (user.role == "Driver") {
        Driver driver;
        driver.id = utils::nextId(store.drivers);
        driver.name = username;
        driver.phone = utils::trim(fields[2].value);
        driver.licenseNo = utils::trim(fields[3].value);
        store.drivers.push_back(driver);
        user.linkedId = driver.id;
    }
    store.users.push_back(user);
    const bool saved = store.save();
    notify(saved ? "ACCOUNT CREATED" : "SAVE FAILED",
           saved ? "You can now sign in as " + username + "." : store.lastError(),
           saved ? theme::success : theme::danger);
    return false;
}

}  // namespace

void run(DataStore& store) {
    while (true) {
        const int option = choose("FLEETFLOW",
                                  {"  Login", "  Register", "   Exit"});
        if (option == 0) {
            if (User* user = login(store)) {
                dashboard(store, *user);
            }
        } else if (option == 1) {
            if (register_user(store)) {
                if (User* user = login(store)) {
                    dashboard(store, *user);
                }
            }
        } else {
            return;
        }
    }
}

}  // namespace terminal
