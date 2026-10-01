#include "pages.hpp"

#include "theme.hpp"
#include "widgets.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <sstream>
#include <string>

namespace apadana::tui {
namespace {

using ui::Cell;
using ui::Column;

constexpr int sidebar_width = 22;
int content_left() { return sidebar_width + 2; }
int content_width() { return std::max(10, COLS - sidebar_width - 4); }

const std::vector<PageMeta>& meta() {
	static const std::vector<PageMeta> pages{
	    {Page::Overview, "Overview", "*", "System at a glance"},
	    {Page::Users, "Users", "+", "Local accounts and access"},
	    {Page::Packages, "Packages", "#", "Install, remove, upgrade"},
	    {Page::Processes, "Processes", ">", "Inspect and signal tasks"},
	    {Page::Services, "Services", "~", "systemd unit control"},
	    {Page::Storage, "Storage", "|", "Mounted filesystem usage"},
	    {Page::Diagnostics, "Diagnostics", "=", "Network and security"},
	};
	return pages;
}

} // namespace

const std::vector<PageMeta>& page_meta() {
	return meta();
}

std::string format_percent(const double value) {
	std::ostringstream output;
	output.setf(std::ios::fixed);
	output.precision(1);
	output << value << '%';
	return output.str();
}

void draw_overview(const SystemSnapshot& snapshot) {
	const int left = content_left();
	const int info = left + 23;
	static constexpr std::array<std::string_view, 7> logo{
	    "       ___       ", "   ___/___\\___   ", "  |  _  _  _  |  ", "  | | || || | |  ",
	    "  |_|_||_||_|_|  ", "  A P A D A N A  ", " system  manager ",
	};
	attron(COLOR_PAIR(ui::Accent) | A_BOLD);
	for (std::size_t row = 0; row < logo.size(); ++row) {
		ui::text_at(4 + static_cast<int>(row), left, logo[row], 20);
	}
	attroff(COLOR_PAIR(ui::Accent) | A_BOLD);

	const char* login = std::getenv("USER");
	attron(COLOR_PAIR(ui::Heading) | A_BOLD);
	ui::text_at(4, info, std::string(login != nullptr ? login : "user") + "@" + snapshot.platform.hostname);
	attroff(COLOR_PAIR(ui::Heading) | A_BOLD);
	attron(COLOR_PAIR(ui::Muted) | A_DIM);
	ui::text_at(5, info, std::string(38, '-'));
	attroff(COLOR_PAIR(ui::Muted) | A_DIM);

	struct InfoRow {
			std::string label;
			std::string value;
	};
	const char* shell_env = std::getenv("SHELL");
	std::string shell = shell_env != nullptr ? shell_env : "unknown";
	if (const auto slash = shell.find_last_of('/'); slash != std::string::npos) {
		shell = shell.substr(slash + 1);
	}
	const std::vector<InfoRow> rows{
	    {"OS       ", snapshot.platform.distribution},
	    {"Kernel   ", snapshot.platform.kernel + " (" + snapshot.platform.architecture + ")"},
	    {"Uptime   ", format_duration(snapshot.uptime_seconds)},
	    {"CPU      ", std::to_string(snapshot.logical_cpu_count) + " logical cores"},
	    {"Shell    ", shell},
	    {"Packages ", std::to_string(snapshot.installed_package_count) + " installed / " +
		             std::to_string(snapshot.package_managers.size()) + " backends"},
	    {"Load     ", std::to_string(snapshot.load_average).substr(0, 4)},
	};
	for (std::size_t index = 0; index < rows.size(); ++index) {
		attron(COLOR_PAIR(ui::Accent) | A_BOLD);
		ui::text_at(7 + static_cast<int>(index), info, rows[index].label);
		attroff(COLOR_PAIR(ui::Accent) | A_BOLD);
		ui::text_at(7 + static_cast<int>(index), info + 10, rows[index].value, COLS - info - 12);
	}

	// Health panel with color-coded usage bars.
	const auto memory_used =
	    snapshot.memory.total_bytes >= snapshot.memory.available_bytes ? snapshot.memory.total_bytes - snapshot.memory.available_bytes : 0;
	const auto disk_used = snapshot.root_disk.capacity_bytes >= snapshot.root_disk.available_bytes
				   ? snapshot.root_disk.capacity_bytes - snapshot.root_disk.available_bytes
				   : 0;
	const double memory_ratio = snapshot.memory.total_bytes == 0 ? 0.0 : static_cast<double>(memory_used) / static_cast<double>(snapshot.memory.total_bytes);
	const double disk_ratio = snapshot.root_disk.capacity_bytes == 0 ? 0.0 : static_cast<double>(disk_used) / static_cast<double>(snapshot.root_disk.capacity_bytes);

	attron(COLOR_PAIR(ui::Heading) | A_BOLD);
	ui::text_at(15, left, "SYSTEM HEALTH");
	attroff(COLOR_PAIR(ui::Heading) | A_BOLD);

	struct Bar {
			std::string label;
			double ratio;
			std::string detail;
	};
	const std::vector<Bar> bars{
	    {"Memory", memory_ratio, format_bytes(memory_used) + " / " + format_bytes(snapshot.memory.total_bytes)},
	    {"Disk  ", disk_ratio, format_bytes(disk_used) + " / " + format_bytes(snapshot.root_disk.capacity_bytes)},
	};
	for (std::size_t index = 0; index < bars.size(); ++index) {
		const int y = 17 + static_cast<int>(index) * 2;
		attron(COLOR_PAIR(ui::Accent) | A_BOLD);
		ui::text_at(y, left, bars[index].label);
		attroff(COLOR_PAIR(ui::Accent) | A_BOLD);
		attron(COLOR_PAIR(ui::usage_color(bars[index].ratio)));
		ui::text_at(y, left + 8, ui::percent_bar(bars[index].ratio, 22));
		attroff(COLOR_PAIR(ui::usage_color(bars[index].ratio)));
		ui::text_at(y, left + 33, bars[index].detail);
	}

	const int summary_y = 21;
	attron(COLOR_PAIR(ui::Success) | A_BOLD);
	ui::text_at(summary_y, left, "Network " + std::to_string(snapshot.network.active_interface_count) + "/" +
	    std::to_string(snapshot.network.interface_count) + " active   RX " + format_bytes(snapshot.network.received_bytes) +
	    "   TX " + format_bytes(snapshot.network.transmitted_bytes));
	attroff(COLOR_PAIR(ui::Success) | A_BOLD);

	const int aslr_pair = snapshot.security.address_space_randomization ? ui::Success : ui::Danger;
	attron(COLOR_PAIR(aslr_pair) | A_BOLD);
	ui::text_at(summary_y + 1, left,
	    std::string("Security ASLR ") + (snapshot.security.address_space_randomization ? "enabled " : "DISABLED") +
	        "   firewall tool " + (snapshot.security.firewall_tool_available ? "detected" : "not found"));
	attroff(COLOR_PAIR(aslr_pair) | A_BOLD);

	attron(COLOR_PAIR(ui::Muted) | A_DIM);
	ui::text_at(LINES - 2, left, "r refresh snapshot   ? help");
	attroff(COLOR_PAIR(ui::Muted) | A_DIM);
}

void draw_users(const std::vector<UserAccount>& users, const UserManager& manager, const std::size_t selected, const std::size_t offset) {
	ui::heading_area(2, content_left(), "User accounts", "Local identities / backend: " + manager.backend_name());
	const std::vector<Column> columns{{"USERNAME", 20}, {"UID", 7}, {"TYPE", 8}, {"STATE", 8}, {"HOME", 0}};
	ui::draw_table(5, content_left(), content_width(), users, columns, selected, offset,
	    [](const std::size_t index, const std::vector<UserAccount>& list) {
		    const auto& user = list[index];
		    const Cell state = !user.locked.has_value() ? Cell{"unknown", ui::Muted}
		                     : (*user.locked ? Cell{"locked", ui::Danger, true} : Cell{"active", ui::Success, true});
		    return std::vector<Cell>{
		        {user.name, ui::Header, true},
		        {std::to_string(user.uid), ui::Muted},
		        {user.uid == 0 ? "root" : (user.system_account ? "system" : "login"), ui::Muted},
		        state,
		        {user.home, ui::Muted},
		    };
	    });
	if (users.empty()) {
		attron(COLOR_PAIR(ui::Muted) | A_DIM);
		ui::text_at(8, content_left(), "No local accounts found.");
		attroff(COLOR_PAIR(ui::Muted) | A_DIM);
	}
	ui::hint_line(LINES - 2, content_left(), {"a add", "l lock", "u unlock", "r refresh", "? help"});
}

void draw_packages(const PackageUiState& state, const PackageManagerBackend* backend) {
	if (backend == nullptr || !backend->available()) {
		ui::heading_area(2, content_left(), "Packages", "No supported package backend on this host");
		attron(COLOR_PAIR(ui::Warning) | A_BOLD);
		ui::text_at(6, content_left(), "NO SUPPORTED PACKAGE BACKEND");
		attroff(COLOR_PAIR(ui::Warning) | A_BOLD);
		attron(COLOR_PAIR(ui::Muted));
		ui::text_at(8, content_left(), "Supported backends: APT, DNF, Pacman. More distro backends will follow.");
		attroff(COLOR_PAIR(ui::Muted));
		return;
	}

	std::string title;
	switch (state.view) {
	case PackageUiState::View::Installed: title = "Installed packages"; break;
	case PackageUiState::View::Search: title = "Search: " + state.query; break;
	case PackageUiState::View::Upgradable: title = "Available upgrades"; break;
	}
	ui::heading_area(2, content_left(), title, backend->backend_name() + " package control / " + std::to_string(state.packages.size()) + " results");
	const std::vector<Column> columns{{"PACKAGE", 28}, {"VERSION", 22}, {"DESCRIPTION", 0}};
	ui::draw_table(5, content_left(), content_width(), state.packages, columns, state.selected, state.offset,
	    [](const std::size_t index, const std::vector<PackageRecord>& list) {
		    const auto& package = list[index];
		    const int version_pair = package.upgradable ? ui::Warning : (package.installed ? ui::Success : ui::Muted);
		    return std::vector<Cell>{
		        {package.name, ui::Header, true},
		        {package.version.empty() ? (package.installed ? "installed" : "available") : package.version, version_pair, package.upgradable},
		        {package.description, ui::Muted},
		    };
	    });
	if (state.packages.empty()) {
		attron(COLOR_PAIR(ui::Muted) | A_DIM);
		ui::text_at(8, content_left(), state.loaded ? "No packages found." : "Loading package information...");
		attroff(COLOR_PAIR(ui::Muted) | A_DIM);
	}
	ui::hint_line(LINES - 2, content_left(),
	    {"s search", "a installed", "g upgrades", "i install", "d remove", "u update index", "U upgrade all", "r refresh"});
}

void draw_processes(const TableUiState<ProcessRecord>& state, const ProcessManager& manager) {
	if (!manager.available()) {
		ui::heading_area(2, content_left(), "Processes", "Process inspection requires a procfs-compatible host");
		attron(COLOR_PAIR(ui::Warning) | A_BOLD);
		ui::text_at(6, content_left(), "PROCESS BACKEND UNAVAILABLE");
		attroff(COLOR_PAIR(ui::Warning) | A_BOLD);
		return;
	}
	ui::heading_area(2, content_left(), "Processes", std::to_string(state.records.size()) + " processes / sorted by lifetime CPU usage");
	const std::vector<Column> columns{{"PID", 7}, {"USER", 11}, {"STATE", 9}, {"CPU", 7}, {"MEMORY", 9}, {"COMMAND", 0}};
	ui::draw_table(5, content_left(), content_width(), state.records, columns, state.selected, state.offset,
	    [](const std::size_t index, const std::vector<ProcessRecord>& list) {
		    const auto& process = list[index];
		    const int state_pair = process.state == 'R' ? ui::Success : (process.state == 'Z' ? ui::Danger : ui::Muted);
		    return std::vector<Cell>{
		        {std::to_string(process.pid), ui::Header, true},
		        {process.user, ui::Muted},
		        {process_state_name(process.state), state_pair, process.state == 'Z'},
		        {format_percent(process.cpu_percent), ui::Accent},
		        {format_bytes(process.resident_bytes), ui::Muted},
		        {process.command, ui::Muted},
		    };
	    });
	ui::hint_line(LINES - 2, content_left(), {"r refresh", "t terminate (SIGTERM)", "K force kill (SIGKILL)", "? help"});
}

void draw_services(const TableUiState<ServiceRecord>& state, const ServiceManager& manager) {
	if (!manager.available()) {
		ui::heading_area(2, content_left(), "Services", "No supported service manager was detected");
		attron(COLOR_PAIR(ui::Warning) | A_BOLD);
		ui::text_at(6, content_left(), "SYSTEMD UNAVAILABLE");
		attroff(COLOR_PAIR(ui::Warning) | A_BOLD);
		return;
	}
	ui::heading_area(2, content_left(), "Services", manager.backend_name() + " service control / " + std::to_string(state.records.size()) + " units");
	const std::vector<Column> columns{{"SERVICE", 25}, {"ACTIVE", 9}, {"SUB", 9}, {"STARTUP", 9}, {"DESCRIPTION", 0}};
	ui::draw_table(5, content_left(), content_width(), state.records, columns, state.selected, state.offset,
	    [](const std::size_t index, const std::vector<ServiceRecord>& list) {
		    const auto& service = list[index];
		    const int active_pair = service.active_state == "active" ? ui::Success : (service.active_state == "failed" ? ui::Danger : ui::Muted);
		    return std::vector<Cell>{
		        {service.name, ui::Header, true},
		        {service.active_state, active_pair, true},
		        {service.sub_state, ui::Muted},
		        {service.unit_file_state.empty() ? "unknown" : service.unit_file_state, ui::Muted},
		        {service.description, ui::Muted},
		    };
	    });
	if (state.records.empty()) {
		attron(COLOR_PAIR(ui::Warning));
		ui::text_at(8, content_left(), "No services returned. systemd may not be running on this host.");
		attroff(COLOR_PAIR(ui::Warning));
	}
	ui::hint_line(LINES - 2, content_left(), {"r refresh", "s start", "x stop", "R restart", "e enable", "d disable"});
}

void draw_storage(const TableUiState<FilesystemInfo>& state, const StorageManager& manager) {
	if (!manager.available()) {
		ui::heading_area(2, content_left(), "Storage", "Filesystem inspection requires a mounted /proc");
		attron(COLOR_PAIR(ui::Warning) | A_BOLD);
		ui::text_at(6, content_left(), "STORAGE BACKEND UNAVAILABLE");
		attroff(COLOR_PAIR(ui::Warning) | A_BOLD);
		return;
	}
	ui::heading_area(2, content_left(), "Storage", std::to_string(state.records.size()) + " mounted filesystems");
	const std::vector<Column> columns{{"DEVICE", 16}, {"MOUNT POINT", 24}, {"TYPE", 9}, {"USAGE", 24}, {"USED / TOTAL", 0}};
	ui::draw_table(5, content_left(), content_width(), state.records, columns, state.selected, state.offset,
	    [](const std::size_t index, const std::vector<FilesystemInfo>& list) {
		    const auto& filesystem = list[index];
		    const auto used = filesystem.total_bytes >= filesystem.available_bytes
					  ? filesystem.total_bytes - filesystem.available_bytes
					  : 0;
		    const double ratio = filesystem.total_bytes == 0
						? 0.0
						: static_cast<double>(used) / static_cast<double>(filesystem.total_bytes);
		    return std::vector<Cell>{
		        {filesystem.device, ui::Header, true},
		        {filesystem.mount_point, ui::Muted},
		        {filesystem.type, ui::Muted},
		        {ui::percent_bar(ratio, 22), ui::usage_color(ratio), ratio >= 0.7},
		        {format_bytes(used) + " / " + format_bytes(filesystem.total_bytes), ui::Muted},
		    };
	    });
	if (state.records.empty()) {
		attron(COLOR_PAIR(ui::Muted) | A_DIM);
		ui::text_at(8, content_left(), "No regular filesystems with capacity were found.");
		attroff(COLOR_PAIR(ui::Muted) | A_DIM);
	}
	ui::hint_line(LINES - 2, content_left(), {"r refresh"});
}

void draw_diagnostics(const SystemSnapshot& snapshot) {
	const int left = content_left();
	ui::heading_area(2, left, "Diagnostics", "Read-only network and security posture");

	attron(COLOR_PAIR(ui::Heading) | A_BOLD);
	ui::text_at(5, left, "NETWORK SUMMARY");
	attroff(COLOR_PAIR(ui::Heading) | A_BOLD);
	struct Row {
			std::string label;
			std::string value;
			int pair;
			bool bold;
	};
	const std::vector<Row> network{
	    {"Interfaces ", std::to_string(snapshot.network.interface_count), ui::Muted, false},
	    {"Active     ", std::to_string(snapshot.network.active_interface_count), ui::Success, true},
	    {"Received   ", format_bytes(snapshot.network.received_bytes), ui::Accent, false},
	    {"Transmitted", format_bytes(snapshot.network.transmitted_bytes), ui::Accent, false},
	};
	for (std::size_t index = 0; index < network.size(); ++index) {
		attron(COLOR_PAIR(ui::Accent));
		ui::text_at(7 + static_cast<int>(index), left, network[index].label);
		attroff(COLOR_PAIR(ui::Accent));
		attron(COLOR_PAIR(network[index].pair) | (network[index].bold ? A_BOLD : 0));
		ui::text_at(7 + static_cast<int>(index), left + 13, network[index].value);
		attroff(COLOR_PAIR(network[index].pair) | (network[index].bold ? A_BOLD : 0));
	}

	attron(COLOR_PAIR(ui::Heading) | A_BOLD);
	ui::text_at(13, left, "SECURITY POSTURE");
	attroff(COLOR_PAIR(ui::Heading) | A_BOLD);
	const int aslr_pair = snapshot.security.address_space_randomization ? ui::Success : ui::Danger;
	attron(COLOR_PAIR(aslr_pair) | A_BOLD);
	ui::text_at(15, left, std::string("ASLR            ") + (snapshot.security.address_space_randomization ? "enabled" : "DISABLED"));
	attroff(COLOR_PAIR(aslr_pair) | A_BOLD);
	const int firewall_pair = snapshot.security.firewall_tool_available ? ui::Success : ui::Warning;
	attron(COLOR_PAIR(firewall_pair) | A_BOLD);
	ui::text_at(16, left, std::string("Firewall tool   ") + (snapshot.security.firewall_tool_available ? "detected" : "not detected"));
	attroff(COLOR_PAIR(firewall_pair) | A_BOLD);

	attron(COLOR_PAIR(ui::Muted) | A_DIM);
	ui::text_at(19, left, "These checks describe host configuration; they do not change system state.");
	attroff(COLOR_PAIR(ui::Muted) | A_DIM);
	ui::hint_line(LINES - 2, left, {"r refresh diagnostics"});
}

} // namespace apadana::tui
