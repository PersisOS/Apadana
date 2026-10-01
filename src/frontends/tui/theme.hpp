#pragma once

namespace apadana::ui {

// Color pair indices used across the interface.
enum ColorPair {
	Header = 1,
	Sidebar,
	SidebarTitle,
	Selection,
	Success,
	Warning,
	Danger,
	Muted,
	Accent,
	Heading,
};

// Initializes the ncurses color palette. Safe to call once after initscr().
void init_theme();

} // namespace apadana::ui
