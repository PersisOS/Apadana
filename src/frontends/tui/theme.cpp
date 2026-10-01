#include "theme.hpp"

#include <curses.h>

namespace apadana::ui {

void init_theme() {
	if (!has_colors()) {
		return;
	}
	start_color();
	use_default_colors();

	// Chrome: blue panels for the header bar, sidebar, and footer.
	init_pair(Header, COLOR_WHITE, COLOR_BLUE);
	init_pair(Sidebar, COLOR_WHITE, COLOR_BLUE);
	init_pair(SidebarTitle, COLOR_BLACK, COLOR_CYAN);
	init_pair(Selection, COLOR_BLACK, COLOR_CYAN);

	// Semantic colors on the default background.
	init_pair(Success, COLOR_GREEN, -1);
	init_pair(Warning, COLOR_YELLOW, -1);
	init_pair(Danger, COLOR_RED, -1);
	init_pair(Muted, COLOR_WHITE, -1);
	init_pair(Accent, COLOR_CYAN, -1);
	init_pair(Heading, COLOR_MAGENTA, -1);
}

} // namespace apadana::ui
