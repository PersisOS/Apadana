#pragma once

#include "apadana/package_manager.hpp"
#include "apadana/process_manager.hpp"
#include "apadana/service_manager.hpp"
#include "apadana/storage_manager.hpp"
#include "apadana/system_info.hpp"
#include "apadana/user_manager.hpp"

#include "widgets.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace apadana::tui {

enum class Page { Overview, Users, Packages, Processes, Services, Storage, Diagnostics };

struct PageMeta {
		Page page;
		std::string_view label;
		std::string_view icon;
		std::string_view tagline;
};

// Sidebar ordering and labels for every module.
const std::vector<PageMeta>& page_meta();

struct PackageUiState {
		enum class View { Installed, Search, Upgradable };
		View view{View::Installed};
		std::vector<PackageRecord> packages;
		std::string query;
		std::size_t selected{};
		std::size_t offset{};
		bool loaded{};
};

template <typename Record> struct TableUiState {
		std::vector<Record> records;
		std::size_t selected{};
		std::size_t offset{};
};

// Status message shown in the footer; color conveys success/failure.
struct Status {
		std::string text;
		int color_pair{ui::Success};
};

void draw_overview(const SystemSnapshot& snapshot);
void draw_users(const std::vector<UserAccount>& users, const UserManager& manager, std::size_t selected, std::size_t offset);
void draw_packages(const PackageUiState& state, const PackageManagerBackend* backend);
void draw_processes(const TableUiState<ProcessRecord>& state, const ProcessManager& manager);
void draw_services(const TableUiState<ServiceRecord>& state, const ServiceManager& manager);
void draw_storage(const TableUiState<FilesystemInfo>& state, const StorageManager& manager);
void draw_diagnostics(const SystemSnapshot& snapshot);

std::string format_percent(double value);

} // namespace apadana::tui
