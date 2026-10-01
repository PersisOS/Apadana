#include "widgets.hpp"

#include "theme.hpp"

#include <algorithm>
#include <clocale>
#include <cstdlib>
#include <curses.h>
#include <string>

namespace apadana::ui {

namespace {

constexpr int sidebar_width = 22;

} // namespace

Session::Session() {
	std::setlocale(LC_ALL, "");
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	init_theme();
}

Session::~Session() {
	endwin();
}

void text_at(const int y, const int x, std::string_view value, const int max_width) {
	if (y < 0 || y >= LINES || x < 0 || x >= COLS) {
		return;
	}
	const int available = max_width >= 0 ? std::min(max_width, COLS - x) : COLS - x;
	if (available <= 0) {
		return;
	}
	const auto length = std::min(value.size(), static_cast<std::size_t>(available));
	if (move(y, x) == ERR) {
		return;
	}
	// Draw character by character so newlines, tabs, and other control
	// characters never reach curses, where they would move the cursor
	// and corrupt the layout.
	for (const char character : value.substr(0, length)) {
		addch(static_cast<chtype>(static_cast<unsigned char>(character) < 0x20 || static_cast<unsigned char>(character) == 0x7f ? ' '
										     : static_cast<unsigned char>(character)));
	}
}

void fill(const int y, const int x, const int width, const int color_pair, const bool bold) {
	if (y < 0 || y >= LINES) {
		return;
	}
	const int clamped_x = std::max(0, x);
	const int clamped_width = std::min(width - (clamped_x - x), COLS - clamped_x);
	if (clamped_width <= 0) {
		return;
	}
	attron(COLOR_PAIR(color_pair) | (bold ? A_BOLD : 0));
	for (int column = 0; column < clamped_width; ++column) {
		mvaddch(y, clamped_x + column, ' ');
	}
	attroff(COLOR_PAIR(color_pair) | (bold ? A_BOLD : 0));
}

std::string percent_bar(const double ratio, const int width) {
	const int filled = std::clamp(static_cast<int>(ratio * static_cast<double>(width)), 0, width);
	std::string bar = "[";
	bar.append(static_cast<std::size_t>(filled), '#');
	bar.append(static_cast<std::size_t>(width - filled), '.');
	bar += ']';
	return bar;
}

int usage_color(const double ratio) {
	if (ratio >= 0.9) return Danger;
	if (ratio >= 0.7) return Warning;
	return Success;
}

bool confirm(const std::string& question) {
	fill(LINES - 1, 0, COLS, Warning);
	attron(COLOR_PAIR(Warning) | A_BOLD);
	text_at(LINES - 1, 2, question + " [y/N]");
	attroff(COLOR_PAIR(Warning) | A_BOLD);
	refresh();
	const int key = getch();
	return key == 'y' || key == 'Y';
}

std::string prompt_text(const std::string& label, const std::size_t maximum_length) {
	std::vector<char> buffer(maximum_length + 1, '\0');
	fill(LINES - 1, 0, COLS, Header);
	attron(COLOR_PAIR(Header) | A_BOLD);
	text_at(LINES - 1, 2, label);
	attroff(COLOR_PAIR(Header) | A_BOLD);
	echo();
	curs_set(1);
	move(LINES - 1, 2 + static_cast<int>(label.size()));
	getnstr(buffer.data(), static_cast<int>(maximum_length));
	noecho();
	curs_set(0);
	return buffer.data();
}

void suspend_terminal() {
	def_prog_mode();
	endwin();
}

void resume_terminal() {
	reset_prog_mode();
	refresh();
}

void show_help(const std::string& title, const std::vector<HelpEntry>& entries) {
	const int width = 56;
	const int height = static_cast<int>(entries.size()) + 6;
	const int top = std::max(0, (LINES - height) / 2);
	const int left = std::max(0, (COLS - width) / 2);

	for (int row = 0; row < height; ++row) {
		fill(top + row, left, width, Header);
	}
	// Border (plain ASCII for maximum terminal compatibility).
	attron(COLOR_PAIR(Header) | A_BOLD);
	for (int row = 0; row < height; ++row) {
		mvaddch(top + row, left, row == 0 || row == height - 1 ? '-' : '|');
		mvaddch(top + row, left + width - 1, row == 0 || row == height - 1 ? '-' : '|');
	}
	for (int column = 1; column < width - 1; ++column) {
		mvaddch(top, left + column, '-');
		mvaddch(top + height - 1, left + column, '-');
	}
	mvaddch(top, left, '+');
	mvaddch(top, left + width - 1, '+');
	mvaddch(top + height - 1, left, '+');
	mvaddch(top + height - 1, left + width - 1, '+');
	attroff(COLOR_PAIR(Header) | A_BOLD);

	attron(COLOR_PAIR(Header) | A_BOLD);
	text_at(top + 1, left + 3, title, width - 6);
	attroff(COLOR_PAIR(Header) | A_BOLD);

	for (std::size_t index = 0; index < entries.size(); ++index) {
		attron(COLOR_PAIR(Success) | A_BOLD);
		text_at(top + 3 + static_cast<int>(index), left + 3, entries[index].key, 12);
		attroff(COLOR_PAIR(Success) | A_BOLD);
		attron(COLOR_PAIR(Header));
		text_at(top + 3 + static_cast<int>(index), left + 16, entries[index].description, width - 20);
		attroff(COLOR_PAIR(Header));
	}
	attron(COLOR_PAIR(Header) | A_DIM);
	text_at(top + height - 2, left + 3, "press any key to close");
	attroff(COLOR_PAIR(Header) | A_DIM);
	refresh();
	getch();
}

void heading_area(const int y, const int x, const std::string_view title, const std::string_view subtitle) {
	attron(A_BOLD | COLOR_PAIR(Heading));
	text_at(y, x, title);
	attroff(A_BOLD | COLOR_PAIR(Heading));
	attron(A_DIM | COLOR_PAIR(Muted));
	text_at(y + 1, x, subtitle, std::max(0, COLS - x - 2));
	attroff(A_DIM | COLOR_PAIR(Muted));
}

void hint_line(const int y, const int x, const std::vector<std::string>& hints) {
	std::string line;
	for (std::size_t index = 0; index < hints.size(); ++index) {
		if (index > 0) {
			line += "  ";
		}
		line += hints[index];
	}
	attron(A_DIM | COLOR_PAIR(Muted));
	text_at(y, x, line, std::max(0, COLS - x - 2));
	attroff(A_DIM | COLOR_PAIR(Muted));
}

std::size_t visible_rows() {
	return static_cast<std::size_t>(std::max(1, LINES - 11));
}

void keep_visible(const std::size_t selected, std::size_t& offset) {
	const std::size_t visible = visible_rows();
	if (selected < offset) {
		offset = selected;
	} else if (selected >= offset + visible) {
		offset = selected - visible + 1;
	}
}

} // namespace apadana::ui
