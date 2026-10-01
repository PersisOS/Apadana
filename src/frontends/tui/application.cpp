#include "apadana/tui.hpp"

#include "pages.hpp"
#include "theme.hpp"
#include "widgets.hpp"

#include "apadana/package_manager.hpp"
#include "apadana/process_manager.hpp"
#include "apadana/service_manager.hpp"
#include "apadana/storage_manager.hpp"
#include "apadana/system_info.hpp"
#include "apadana/user_manager.hpp"

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>

#ifndef APADANA_VERSION_MAJOR
#define APADANA_VERSION_MAJOR 0
#endif
#ifndef APADANA_VERSION_MINOR
#define APADANA_VERSION_MINOR 3
#endif
#ifndef APADANA_VERSION_PATCH
#define APADANA_VERSION_PATCH 0
#endif

namespace apadana {
namespace {

using tui::PackageUiState;
using tui::Page;
using tui::Status;
using tui::TableUiState;
namespace ui = apadana::ui;

constexpr int sidebar_width = 22;

// ---------------------------------------------------------------- chrome ---

void draw_chrome(const Page active, const Status& status) {
	erase();
	if (LINES < 18 || COLS < 72) {
		attron(COLOR_PAIR(ui::Warning) | A_BOLD);
		ui::text_at(1, 2, "Terminal too small");
		attroff(COLOR_PAIR(ui::Warning) | A_BOLD);
		ui::text_at(3, 2, "Resize to at least 72x18. Press q to quit.");
		refresh();
		return;
	}

	const auto& pages = tui::page_meta();
	const tui::PageMeta* current = &pages.front();
	for (const auto& meta : pages) {
		if (meta.page == active) {
			current = &meta;
		}
	}

	// Header bar.
	ui::fill(0, 0, COLS, ui::Header, true);
	attron(COLOR_PAIR(ui::Header) | A_BOLD);
	ui::text_at(0, 2, "* APADANA");
	ui::text_at(0, 13, "SYSTEM CONTROL CENTER");
	const std::string tagline(current->tagline);
	ui::text_at(0, COLS - static_cast<int>(tagline.size()) - 3, tagline);
	attroff(COLOR_PAIR(ui::Header) | A_BOLD);

	// Sidebar.
	for (int row = 1; row < LINES; ++row) {
		ui::fill(row, 0, sidebar_width, ui::Sidebar);
	}
	attron(COLOR_PAIR(ui::SidebarTitle) | A_BOLD);
	ui::text_at(2, 2, " MODULES ");
	attroff(COLOR_PAIR(ui::SidebarTitle) | A_BOLD);
	for (std::size_t index = 0; index < pages.size(); ++index) {
		const int y = 4 + static_cast<int>(index) * 2;
		const std::string label = std::string(pages[index].icon) + " " + std::string(pages[index].label);
		if (pages[index].page == active) {
			ui::fill(y, 1, sidebar_width - 1, ui::Selection, true);
			attron(COLOR_PAIR(ui::Selection) | A_BOLD);
			ui::text_at(y, 2, label, sidebar_width - 3);
			attroff(COLOR_PAIR(ui::Selection) | A_BOLD);
		} else {
			attron(COLOR_PAIR(ui::Sidebar) | A_BOLD);
			ui::text_at(y, 2, label, sidebar_width - 3);
			attroff(COLOR_PAIR(ui::Sidebar) | A_BOLD);
		}
	}
	attron(COLOR_PAIR(ui::Sidebar) | A_DIM);
	ui::text_at(LINES - 2, 2, "v" + std::to_string(APADANA_VERSION_MAJOR) + "." + std::to_string(APADANA_VERSION_MINOR) + "." +
	    std::to_string(APADANA_VERSION_PATCH));
	attroff(COLOR_PAIR(ui::Sidebar) | A_DIM);

	// Footer: hints, or the latest status message when present.
	ui::fill(LINES - 1, 0, COLS, ui::Sidebar);
	if (status.text.empty()) {
		attron(COLOR_PAIR(ui::Sidebar) | A_DIM);
		ui::text_at(LINES - 1, 2, "Tab/Arrows switch module   Up/Down select   PgUp/PgDn   ? help   q quit");
		attroff(COLOR_PAIR(ui::Sidebar) | A_DIM);
	} else {
		attron(COLOR_PAIR(status.color_pair) | A_BOLD);
		ui::text_at(LINES - 1, 2, status.text, COLS - 4);
		attroff(COLOR_PAIR(status.color_pair) | A_BOLD);
	}
}

// ------------------------------------------------------------- utilities ---

// Generic table navigation: returns true when the key was consumed.
bool handle_table_key(const int key, std::size_t& selected, const std::size_t count) {
	switch (key) {
	case KEY_UP:
	case 'k':
		if (selected > 0) --selected;
		return true;
	case KEY_DOWN:
	case 'j':
		if (count != 0 && selected + 1 < count) ++selected;
		return true;
	case KEY_PPAGE:
		selected = selected > 10 ? selected - 10 : 0;
		return true;
	case KEY_NPAGE:
		if (count != 0) selected = std::min(count - 1, selected + 10);
		return true;
	case KEY_HOME:
	case 'g':
		selected = 0;
		return true;
	case KEY_END:
	case 'G':
		if (count != 0) selected = count - 1;
		return true;
	default:
		return false;
	}
}

// Runs a privileged action off-screen, then restores the interface.
template <typename Action>
auto run_privileged(Action&& action) {
	ui::suspend_terminal();
	auto result = action();
	ui::resume_terminal();
	return result;
}

void help_overlay(const Page page) {
	std::vector<ui::HelpEntry> entries{
	    {"Tab, Left/Right", "switch module"},
	    {"Up/Down, k/j", "move selection"},
	    {"PgUp/PgDn", "scroll by 10"},
	    {"g / G", "jump to first / last row"},
	    {"?", "this help"},
	    {"q", "quit Apadana"},
	};
	if (page == Page::Overview || page == Page::Diagnostics) {
		entries.push_back({"r", "refresh"});
	} else if (page == Page::Users) {
		entries.push_back({"a", "create user account"});
		entries.push_back({"l / u", "lock / unlock account"});
		entries.push_back({"r", "refresh accounts"});
	} else if (page == Page::Packages) {
		entries.push_back({"s", "search packages"});
		entries.push_back({"a / g", "installed / upgradable view"});
		entries.push_back({"i / d", "install / remove selection"});
		entries.push_back({"u / U", "update index / upgrade all"});
		entries.push_back({"r", "refresh view"});
	} else if (page == Page::Processes) {
		entries.push_back({"t", "send SIGTERM"});
		entries.push_back({"K", "send SIGKILL"});
		entries.push_back({"r", "refresh list"});
	} else if (page == Page::Services) {
		entries.push_back({"s / x", "start / stop unit"});
		entries.push_back({"R", "restart unit"});
		entries.push_back({"e / d", "enable / disable at boot"});
		entries.push_back({"r", "refresh units"});
	} else if (page == Page::Storage) {
		entries.push_back({"r", "refresh mounts"});
	}
	ui::show_help("Apadana key bindings", entries);
}

void load_package_view(PackageUiState& state, const PackageManagerBackend& backend) {
	switch (state.view) {
	case PackageUiState::View::Installed:
		state.packages = backend.installed_packages();
		break;
	case PackageUiState::View::Search:
		state.packages = backend.search(state.query);
		break;
	case PackageUiState::View::Upgradable:
		state.packages = backend.upgradable_packages();
		break;
	}
	state.loaded   = true;
	state.selected = 0;
	state.offset   = 0;
}

} // namespace

int Application::run() {
	ui::Session terminal;
	auto snapshot = collect_system_snapshot();
	UserManager user_manager;
	AptPackageManager apt;
	ProcessManager process_manager;
	ServiceManager service_manager;
	auto users = user_manager.list_users();
	auto package_backend = create_package_backend();
	std::size_t selected_user = 0;
	std::size_t user_offset   = 0;
	PackageUiState package_state;
	TableUiState<ProcessRecord> process_state;
	TableUiState<ServiceRecord> service_state;
	StorageManager storage_manager;
	TableUiState<FilesystemInfo> storage_state;
	bool processes_loaded = false;
	bool services_loaded  = false;
	bool storage_loaded   = false;
	std::size_t page_index = 0;
	Status status;

	bool running = true;
	while (running) {
		const Page page = tui::page_meta()[page_index].page;
		if (page == Page::Packages && package_backend != nullptr && package_backend->available() && !package_state.loaded) {
			load_package_view(package_state, *package_backend);
		}
		if (page == Page::Processes && !processes_loaded) {
			process_state.records = process_manager.list_processes();
			processes_loaded      = true;
		}
		if (page == Page::Services && !services_loaded) {
			service_state.records = service_manager.list_services();
			services_loaded       = true;
		}
		if (page == Page::Storage && !storage_loaded) {
			storage_state.records = storage_manager.list_filesystems();
			storage_loaded        = true;
		}

		draw_chrome(page, status);
		if (LINES >= 18 && COLS >= 72) {
			switch (page) {
			case Page::Overview: tui::draw_overview(snapshot); break;
			case Page::Users: tui::draw_users(users, user_manager, selected_user, user_offset); break;
			case Page::Packages: tui::draw_packages(package_state, package_backend.get()); break;
			case Page::Processes: tui::draw_processes(process_state, process_manager); break;
			case Page::Services: tui::draw_services(service_state, service_manager); break;
			case Page::Storage: tui::draw_storage(storage_state, storage_manager); break;
			case Page::Diagnostics: tui::draw_diagnostics(snapshot); break;
			}
		}
		refresh();
		status.text.clear();

		const int key = getch();
		if (key == 'q' || key == 'Q') {
			running = false;
			break;
		}
		if (key == KEY_LEFT) {
			page_index = page_index == 0 ? tui::page_meta().size() - 1 : page_index - 1;
			continue;
		}
		if (key == KEY_RIGHT || key == '\t') {
			page_index = (page_index + 1) % tui::page_meta().size();
			continue;
		}
		if (key == '?') {
			help_overlay(page);
			continue;
		}

		// ------------------------------------------------ module handling ---
		switch (page) {
		case Page::Overview:
			if (key == 'r') {
				snapshot = collect_system_snapshot();
				status = {"System snapshot refreshed", ui::Success};
			}
			break;

		case Page::Users: {
			if (handle_table_key(key, selected_user, users.size())) {
				break;
			}
			if (key == 'r') {
				users = user_manager.list_users();
				selected_user = users.empty() ? 0 : std::min(selected_user, users.size() - 1);
				status = {"User list refreshed", ui::Success};
			} else if (key == 'a') {
				const auto username = ui::prompt_text("New username: ", 32);
				if (!UserManager::valid_username(username)) {
					status = {"Invalid username", ui::Danger};
				} else if (ui::confirm("Create account '" + username + "' with a home directory?")) {
					const auto result = run_privileged([&] { return user_manager.create_user(username); });
					status = {result.message, result.success ? ui::Success : ui::Danger};
					users = user_manager.list_users();
					selected_user = users.empty() ? 0 : std::min(selected_user, users.size() - 1);
				}
			} else if (!users.empty() && (key == 'l' || key == 'u')) {
				const auto& account = users[selected_user];
				const bool locking  = key == 'l';
				if (ui::confirm(std::string(locking ? "Lock" : "Unlock") + " account '" + account.name + "'?")) {
					const auto result =
					    run_privileged([&] { return locking ? user_manager.lock_user(account.name) : user_manager.unlock_user(account.name); });
					status = {result.message, result.success ? ui::Success : ui::Danger};
					users = user_manager.list_users();
				}
			}
			break;
		}

		case Page::Packages: {
			if (package_backend == nullptr || !package_backend->available()) {
				break;
			}
			auto& apt = *package_backend;
			if (handle_table_key(key, package_state.selected, package_state.packages.size())) {
				break;
			}
			if (key == 's') {
				const auto query = ui::prompt_text("Search APT: ", 100);
				if (!query.empty()) {
					package_state.view  = PackageUiState::View::Search;
					package_state.query = query;
					load_package_view(package_state, apt);
					status = {"APT search completed", ui::Success};
				}
			} else if (key == 'a') {
				package_state.view = PackageUiState::View::Installed;
				load_package_view(package_state, apt);
				status = {"Showing installed packages", ui::Success};
			} else if (key == 'g') {
				package_state.view = PackageUiState::View::Upgradable;
				load_package_view(package_state, apt);
				status = {"Showing available upgrades", ui::Success};
			} else if (key == 'r') {
				load_package_view(package_state, apt);
				status = {"Package view refreshed", ui::Success};
			} else if (key == 'u') {
				if (ui::confirm("Refresh the APT package index?")) {
					const auto result = run_privileged([&] { return apt.update_index(); });
					status = {result.message, result.success ? ui::Success : ui::Danger};
					load_package_view(package_state, apt);
				}
			} else if (key == 'U') {
				if (ui::confirm("Upgrade all packages using APT?")) {
					const auto result = run_privileged([&] { return apt.upgrade_all(); });
					status = {result.message, result.success ? ui::Success : ui::Danger};
					load_package_view(package_state, apt);
				}
			} else if (!package_state.packages.empty() && key == 'i') {
				const auto package = package_state.packages[package_state.selected].name;
				if (ui::confirm("Install package '" + package + "'?")) {
					const auto result = run_privileged([&] { return apt.install(package); });
					status = {result.message, result.success ? ui::Success : ui::Danger};
					load_package_view(package_state, apt);
				}
			} else if (!package_state.packages.empty() && key == 'd') {
				const auto& record = package_state.packages[package_state.selected];
				if (!record.installed) {
					status = {"Select an installed package before removing", ui::Warning};
				} else if (ui::confirm("Remove package '" + record.name + "'?")) {
					const auto result = run_privileged([&] { return apt.remove(record.name); });
					status = {result.message, result.success ? ui::Success : ui::Danger};
					load_package_view(package_state, apt);
				}
			}
			break;
		}

		case Page::Processes: {
			if (handle_table_key(key, process_state.selected, process_state.records.size())) {
				break;
			}
			if (key == 'r') {
				process_state.records = process_manager.list_processes();
				process_state.selected =
				    process_state.records.empty() ? 0 : std::min(process_state.selected, process_state.records.size() - 1);
				status = {"Process list refreshed", ui::Success};
			} else if (!process_state.records.empty() && (key == 't' || key == 'K')) {
				const auto process = process_state.records[process_state.selected];
				const bool force   = key == 'K';
				if (ui::confirm(std::string(force ? "Force kill" : "Terminate") + " PID " + std::to_string(process.pid) + " (" +
				                process.command + ")?")) {
					const auto result = process_manager.terminate(process.pid, force);
					status = {result.message, result.success ? ui::Success : ui::Danger};
					process_state.records = process_manager.list_processes();
					process_state.selected =
					    process_state.records.empty() ? 0 : std::min(process_state.selected, process_state.records.size() - 1);
				}
			}
			break;
		}

		case Page::Services: {
			if (handle_table_key(key, service_state.selected, service_state.records.size())) {
				break;
			}
			if (key == 'r') {
				service_state.records = service_manager.list_services();
				service_state.selected =
				    service_state.records.empty() ? 0 : std::min(service_state.selected, service_state.records.size() - 1);
				status = {"Service list refreshed", ui::Success};
			} else if (!service_state.records.empty() && (key == 's' || key == 'x' || key == 'R' || key == 'e' || key == 'd')) {
				const auto service = service_state.records[service_state.selected].name;
				const char* action = key == 's' ? "Start" : key == 'x' ? "Stop" : key == 'R' ? "Restart" : key == 'e' ? "Enable" : "Disable";
				if (ui::confirm(std::string(action) + " service '" + service + "'?")) {
					const auto result = run_privileged([&] {
						switch (key) {
						case 's': return service_manager.start(service);
						case 'x': return service_manager.stop(service);
						case 'R': return service_manager.restart(service);
						case 'e': return service_manager.enable(service);
						default: return service_manager.disable(service);
						}
					});
					status = {result.message, result.success ? ui::Success : ui::Danger};
					service_state.records = service_manager.list_services();
					service_state.selected =
					    service_state.records.empty() ? 0 : std::min(service_state.selected, service_state.records.size() - 1);
				}
			}
			break;
		}

		case Page::Storage:
			if (handle_table_key(key, storage_state.selected, storage_state.records.size())) {
				break;
			}
			if (key == 'r') {
				storage_state.records = storage_manager.list_filesystems();
				storage_state.selected =
				    storage_state.records.empty() ? 0 : std::min(storage_state.selected, storage_state.records.size() - 1);
				status = {"Filesystem list refreshed", ui::Success};
			}
			break;

		case Page::Diagnostics:
			if (key == 'r') {
				snapshot = collect_system_snapshot();
				status = {"Diagnostics refreshed", ui::Success};
			}
			break;
		}

		ui::keep_visible(selected_user, user_offset);
		ui::keep_visible(package_state.selected, package_state.offset);
		ui::keep_visible(process_state.selected, process_state.offset);
		ui::keep_visible(service_state.selected, service_state.offset);
		ui::keep_visible(storage_state.selected, storage_state.offset);
	}
	return 0;
}

} // namespace apadana
