#include "apadana/package_manager.hpp"
#include "apadana/process_manager.hpp"
#include "apadana/service_manager.hpp"
#include "apadana/storage_manager.hpp"
#include "apadana/system_info.hpp"
#include "apadana/user_manager.hpp"

#include <gtk/gtk.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace {

enum class Action { Refresh, UserCreate, UserLock, UserUnlock, PackageInstalled, PackageSearch, PackageUpgrades,
	PackageInstall, PackageRemove, PackageUpdate, PackageUpgrade, ProcessTerminate, ProcessKill, ServiceStart,
	ServiceStop, ServiceRestart, ServiceEnable, ServiceDisable };

struct Page {
	GtkListStore* model{};
	GtkTreeView* view{};
};

struct Gui {
	GtkWidget* window{};
	GtkWidget* stack{};
	GtkWidget* status{};
	GtkWidget* package_query{};
	std::vector<Page> pages;
	std::unique_ptr<apadana::PackageManagerBackend> package_backend;
	apadana::UserManager users;
	apadana::ProcessManager processes;
	apadana::ServiceManager services;
	apadana::StorageManager storage;
	int package_view{};
	std::string package_search;
};

struct ActionContext { Gui* gui; std::size_t page; Action action; };

std::string bytes(std::uint64_t value) { return apadana::format_bytes(value); }

void set_status(Gui& gui, const std::string& message) {
	gtk_label_set_text(GTK_LABEL(gui.status), message.c_str());
}

void set_rows(Page& page, const std::vector<std::vector<std::string>>& rows) {
	gtk_list_store_clear(page.model);
	for (const auto& row : rows) {
		GtkTreeIter iter;
		gtk_list_store_append(page.model, &iter);
		for (std::size_t i = 0; i < row.size() && i < 6; ++i)
			gtk_list_store_set(page.model, &iter, static_cast<gint>(i), row[i].c_str(), -1);
	}
}

void refresh(Gui& gui) {
	const auto snapshot = apadana::collect_system_snapshot();
	const auto& platform = snapshot.platform;
	set_rows(gui.pages[0], {{"Operating system", platform.operating_system}, {"Distribution", platform.distribution},
	                        {"Kernel", platform.kernel}, {"Architecture", platform.architecture},
	                        {"Uptime", apadana::format_duration(snapshot.uptime_seconds)},
	                        {"CPU cores", std::to_string(snapshot.logical_cpu_count)}, {"Load average", std::to_string(snapshot.load_average)},
	                        {"Memory", bytes(snapshot.memory.total_bytes - snapshot.memory.available_bytes) + " / " + bytes(snapshot.memory.total_bytes)},
	                        {"Root filesystem", bytes(snapshot.root_disk.capacity_bytes - snapshot.root_disk.available_bytes) + " / " + bytes(snapshot.root_disk.capacity_bytes)}});

	std::vector<std::vector<std::string>> rows;
	for (const auto& user : gui.users.list_users()) rows.push_back({user.name, std::to_string(user.uid), std::to_string(user.gid), user.home,
	    user.shell, user.locked ? (*user.locked ? "Locked" : "Unlocked") : "Unknown"});
	set_rows(gui.pages[1], rows);

	rows.clear();
	if (gui.package_backend && gui.package_backend->available()) {
		std::vector<apadana::PackageRecord> packages;
		if (gui.package_view == 1) packages = gui.package_backend->search(gui.package_search);
		else if (gui.package_view == 2) packages = gui.package_backend->upgradable_packages();
		else packages = gui.package_backend->installed_packages();
		for (const auto& item : packages) rows.push_back({item.name, item.version, item.description,
		    item.installed ? "Installed" : "Available", item.upgradable ? "Upgrade available" : "", {}});
	}
	set_rows(gui.pages[2], rows);

	rows.clear();
	for (const auto& process : gui.processes.list_processes()) rows.push_back({std::to_string(process.pid), process.user,
	    apadana::process_state_name(process.state), std::to_string(process.cpu_percent), bytes(process.resident_bytes), process.command});
	set_rows(gui.pages[3], rows);

	rows.clear();
	for (const auto& service : gui.services.list_services()) rows.push_back({service.name, service.description,
	    service.load_state, service.active_state, service.sub_state, service.unit_file_state});
	set_rows(gui.pages[4], rows);

	rows.clear();
	for (const auto& fs : gui.storage.list_filesystems()) rows.push_back({fs.device, fs.mount_point, fs.type,
	    bytes(fs.total_bytes - fs.available_bytes), bytes(fs.total_bytes), {}});
	set_rows(gui.pages[5], rows);
	set_rows(gui.pages[6], {{"Network interfaces", std::to_string(snapshot.network.interface_count)},
	    {"Active interfaces", std::to_string(snapshot.network.active_interface_count)}, {"Received", bytes(snapshot.network.received_bytes)},
	    {"Transmitted", bytes(snapshot.network.transmitted_bytes)}, {"Address space randomization", snapshot.security.address_space_randomization ? "Enabled" : "Disabled"},
	    {"Firewall tooling", snapshot.security.firewall_tool_available ? "Available" : "Not detected"}});
	set_status(gui, "System information refreshed");
}

bool confirm(Gui& gui, const std::string& message) {
	GtkWidget* dialog = gtk_message_dialog_new(GTK_WINDOW(gui.window), GTK_DIALOG_MODAL,
	    GTK_MESSAGE_WARNING, GTK_BUTTONS_YES_NO, "%s", message.c_str());
	const gint response = gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);
	return response == GTK_RESPONSE_YES;
}

std::string prompt(Gui& gui, const std::string& title, const std::string& label_text) {
	GtkWidget* dialog = gtk_dialog_new_with_buttons(title.c_str(), GTK_WINDOW(gui.window), GTK_DIALOG_MODAL,
	    "_Cancel", GTK_RESPONSE_CANCEL, "_OK", GTK_RESPONSE_OK, nullptr);
	GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	GtkWidget* label = gtk_label_new(label_text.c_str());
	GtkWidget* entry = gtk_entry_new();
	gtk_widget_set_margin_start(label, 12);
	gtk_widget_set_margin_end(label, 12);
	gtk_widget_set_margin_top(label, 10);
	gtk_widget_set_margin_start(entry, 12);
	gtk_widget_set_margin_end(entry, 12);
	gtk_widget_set_margin_bottom(entry, 10);
	gtk_box_pack_start(GTK_BOX(content), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(content), entry, FALSE, FALSE, 0);
	gtk_widget_show_all(dialog);
	std::string value;
	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
		value = gtk_entry_get_text(GTK_ENTRY(entry));
	gtk_widget_destroy(dialog);
	return value;
}

bool selected_value(Gui& gui, std::size_t page, std::string& value) {
	GtkTreeSelection* selection = gtk_tree_view_get_selection(gui.pages[page].view);
	GtkTreeModel* model = nullptr;
	GtkTreeIter iter;
	if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
		set_status(gui, "Select a row first");
		return false;
	}
	gchar* text = nullptr;
	gtk_tree_model_get(model, &iter, 0, &text, -1);
	value = text == nullptr ? "" : text;
	g_free(text);
	return true;
}

void finish_action(Gui& gui, bool success, const std::string& message) {
	if (success) refresh(gui);
	set_status(gui, message);
}

void on_action(GtkButton*, gpointer data) {
	auto& context = *static_cast<ActionContext*>(data);
	Gui& gui = *context.gui;
	std::string selected;
	switch (context.action) {
	case Action::Refresh:
		refresh(gui);
		return;
	case Action::UserCreate: {
		const auto name = prompt(gui, "Create user account", "New username (a home directory will be created):");
		if (name.empty()) return;
		if (!apadana::UserManager::valid_username(name)) { set_status(gui, "Invalid username"); return; }
		if (!confirm(gui, "Create account '" + name + "' with a home directory?")) return;
		const auto result = gui.users.create_user(name);
		finish_action(gui, result.success, result.message);
		return;
	}
	case Action::UserLock:
	case Action::UserUnlock: {
		if (!selected_value(gui, context.page, selected)) return;
		const bool lock = context.action == Action::UserLock;
		if (!confirm(gui, std::string(lock ? "Lock" : "Unlock") + " account '" + selected + "'?")) return;
		const auto result = lock ? gui.users.lock_user(selected) : gui.users.unlock_user(selected);
		finish_action(gui, result.success, result.message);
		return;
	}
	case Action::PackageInstalled: gui.package_view = 0; gui.package_search.clear(); break;
	case Action::PackageSearch:
		gui.package_search = gtk_entry_get_text(GTK_ENTRY(gui.package_query));
		if (gui.package_search.empty()) { set_status(gui, "Enter a package search term"); return; }
		gui.package_view = 1;
		break;
	case Action::PackageUpgrades: gui.package_view = 2; gui.package_search.clear(); break;
	case Action::PackageInstall:
	case Action::PackageRemove: {
		if (!gui.package_backend || !selected_value(gui, context.page, selected)) return;
		const bool install = context.action == Action::PackageInstall;
		if (!confirm(gui, std::string(install ? "Install" : "Remove") + " package '" + selected + "'?")) return;
		const auto result = install ? gui.package_backend->install(selected) : gui.package_backend->remove(selected);
		finish_action(gui, result.success, result.message);
		return;
	}
	case Action::PackageUpdate:
	case Action::PackageUpgrade: {
		if (!gui.package_backend) { set_status(gui, "No supported package manager is available"); return; }
		const bool update = context.action == Action::PackageUpdate;
		if (!confirm(gui, update ? "Refresh the package index?" : "Upgrade all installed packages?")) return;
		const auto result = update ? gui.package_backend->update_index() : gui.package_backend->upgrade_all();
		finish_action(gui, result.success, result.message);
		return;
	}
	case Action::ProcessTerminate:
	case Action::ProcessKill: {
		if (!selected_value(gui, context.page, selected)) return;
		const int pid = std::stoi(selected);
		const bool force = context.action == Action::ProcessKill;
		if (!confirm(gui, std::string(force ? "Force kill" : "Terminate") + " process " + selected + "?")) return;
		const auto result = gui.processes.terminate(pid, force);
		finish_action(gui, result.success, result.message);
		return;
	}
	case Action::ServiceStart:
	case Action::ServiceStop:
	case Action::ServiceRestart:
	case Action::ServiceEnable:
	case Action::ServiceDisable: {
		if (!selected_value(gui, context.page, selected)) return;
		const auto action = context.action;
		const char* verb = action == Action::ServiceStart ? "Start" : action == Action::ServiceStop ? "Stop" :
		    action == Action::ServiceRestart ? "Restart" : action == Action::ServiceEnable ? "Enable" : "Disable";
		if (!confirm(gui, std::string(verb) + " service '" + selected + "'?")) return;
		apadana::ServiceActionResult result;
		if (action == Action::ServiceStart) result = gui.services.start(selected);
		else if (action == Action::ServiceStop) result = gui.services.stop(selected);
		else if (action == Action::ServiceRestart) result = gui.services.restart(selected);
		else if (action == Action::ServiceEnable) result = gui.services.enable(selected);
		else result = gui.services.disable(selected);
		finish_action(gui, result.success, result.message);
		return;
	}
	}
	refresh(gui);
}

void destroy_action(gpointer data, GClosure*) { delete static_cast<ActionContext*>(data); }

void add_action(Gui& gui, std::size_t page, GtkWidget* box, const char* title, Action action) {
	GtkWidget* button = gtk_button_new_with_label(title);
	auto* context = new ActionContext{&gui, page, action};
	g_signal_connect_data(button, "clicked", G_CALLBACK(on_action), context, destroy_action, G_CONNECT_DEFAULT);
	gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
}

void add_page(Gui& gui, const char* name, const char* title, const std::vector<const char*>& headers) {
	Page page;
	page.model = gtk_list_store_new(static_cast<gint>(headers.size()), G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
	                                G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
	page.view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(page.model)));
	gtk_tree_view_set_headers_visible(page.view, TRUE);
	for (std::size_t i = 0; i < headers.size(); ++i) {
		GtkCellRenderer* renderer = gtk_cell_renderer_text_new();
		GtkTreeViewColumn* column = gtk_tree_view_column_new_with_attributes(headers[i], renderer, "text", static_cast<gint>(i), nullptr);
		gtk_tree_view_column_set_resizable(column, TRUE);
		gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_AUTOSIZE);
		gtk_tree_view_append_column(page.view, column);
	}
	GtkWidget* panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	GtkWidget* actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_widget_set_margin_start(actions, 10);
	gtk_widget_set_margin_end(actions, 10);
	gtk_widget_set_margin_top(actions, 10);
	GtkWidget* scroller = gtk_scrolled_window_new(nullptr, nullptr);
	gtk_container_add(GTK_CONTAINER(scroller), GTK_WIDGET(page.view));
	gtk_box_pack_start(GTK_BOX(panel), actions, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(panel), scroller, TRUE, TRUE, 0);
	const std::size_t index = gui.pages.size();
	gui.pages.push_back(page);
	gtk_stack_add_titled(GTK_STACK(gui.stack), panel, name, title);
	add_action(gui, index, actions, "Refresh", Action::Refresh);
	if (index == 1) {
		add_action(gui, index, actions, "Create user", Action::UserCreate);
		add_action(gui, index, actions, "Lock", Action::UserLock);
		add_action(gui, index, actions, "Unlock", Action::UserUnlock);
	} else if (index == 2) {
		gui.package_query = gtk_entry_new();
		gtk_entry_set_placeholder_text(GTK_ENTRY(gui.package_query), "Search packages");
		gtk_widget_set_size_request(gui.package_query, 170, -1);
		gtk_box_pack_start(GTK_BOX(actions), gui.package_query, FALSE, FALSE, 0);
		add_action(gui, index, actions, "Search", Action::PackageSearch);
		add_action(gui, index, actions, "Installed", Action::PackageInstalled);
		add_action(gui, index, actions, "Upgrades", Action::PackageUpgrades);
		add_action(gui, index, actions, "Install", Action::PackageInstall);
		add_action(gui, index, actions, "Remove", Action::PackageRemove);
		add_action(gui, index, actions, "Update index", Action::PackageUpdate);
		add_action(gui, index, actions, "Upgrade all", Action::PackageUpgrade);
	} else if (index == 3) {
		add_action(gui, index, actions, "Terminate", Action::ProcessTerminate);
		add_action(gui, index, actions, "Force kill", Action::ProcessKill);
	} else if (index == 4) {
		add_action(gui, index, actions, "Start", Action::ServiceStart);
		add_action(gui, index, actions, "Stop", Action::ServiceStop);
		add_action(gui, index, actions, "Restart", Action::ServiceRestart);
		add_action(gui, index, actions, "Enable", Action::ServiceEnable);
		add_action(gui, index, actions, "Disable", Action::ServiceDisable);
	}
}

void on_refresh(GtkButton*, gpointer data) { refresh(*static_cast<Gui*>(data)); }

void activate(GtkApplication* app, gpointer) {
	static Gui gui;
	gui.package_backend = apadana::create_package_backend();
	gui.window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(gui.window), "Apadana System Manager");
	gtk_window_set_default_size(GTK_WINDOW(gui.window), 1200, 760);
	GtkWidget* root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	GtkWidget* header = gtk_header_bar_new();
	gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Apadana");
	gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "System control center");
	gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
	GtkWidget* refresh_button = gtk_button_new_from_icon_name("view-refresh-symbolic", GTK_ICON_SIZE_BUTTON);
	gtk_widget_set_tooltip_text(refresh_button, "Refresh system information");
	gtk_header_bar_pack_end(GTK_HEADER_BAR(header), refresh_button);
	gtk_window_set_titlebar(GTK_WINDOW(gui.window), header);
	GtkWidget* body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gui.stack = gtk_stack_new();
	gtk_stack_set_transition_type(GTK_STACK(gui.stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
	gtk_stack_set_transition_duration(GTK_STACK(gui.stack), 120);
	add_page(gui, "overview", "Overview", {"Property", "Value"});
	add_page(gui, "users", "Users", {"Name", "UID", "GID", "Home", "Shell", "Status"});
	add_page(gui, "packages", "Packages", {"Name", "Version", "Description", "Install state", "Updates"});
	add_page(gui, "processes", "Processes", {"PID", "User", "State", "CPU %", "Memory", "Command"});
	add_page(gui, "services", "Services", {"Name", "Description", "Load", "Active", "Substate", "Enabled"});
	add_page(gui, "storage", "Storage", {"Device", "Mount point", "Type", "Used", "Capacity"});
	add_page(gui, "diagnostics", "Diagnostics", {"Check", "Value"});
	GtkWidget* sidebar = gtk_stack_sidebar_new();
	gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(gui.stack));
	gtk_widget_set_size_request(sidebar, 190, -1);
	gtk_box_pack_start(GTK_BOX(body), sidebar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(body), gui.stack, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(root), body, TRUE, TRUE, 0);
	gui.status = gtk_label_new("");
	gtk_widget_set_halign(gui.status, GTK_ALIGN_START);
	gtk_widget_set_margin_start(gui.status, 12);
	gtk_widget_set_margin_top(gui.status, 7);
	gtk_widget_set_margin_bottom(gui.status, 7);
	gtk_box_pack_start(GTK_BOX(root), gui.status, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(gui.window), root);
	g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh), &gui);
	refresh(gui);
	gtk_widget_show_all(gui.window);
}

} // namespace

int main(int argc, char** argv) {
	GtkApplication* app = gtk_application_new("org.apadana.SystemManager", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
	const int result = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);
	return result;
}
