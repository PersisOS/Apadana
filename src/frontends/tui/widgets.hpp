#pragma once

#include "theme.hpp"

#include <curses.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace apadana::ui {

// RAII ncurses session: raw/cbreak mode, no echo, colors enabled.
class Session {
	public:
		Session();
		~Session();
		Session(const Session&) = delete;
		Session& operator=(const Session&) = delete;
};

void text_at(int y, int x, std::string_view value, int max_width = -1);

// Fills width columns at (y, x) with a background color.
void fill(int y, int x, int width, int color_pair, bool bold = false);

// A usage bar rendered with ASCII blocks, e.g. [######..............]
std::string percent_bar(double ratio, int width);

// Picks Success/Warning/Danger for a usage ratio.
int usage_color(double ratio);

// Modal confirmation on the last line. Returns true when confirmed with y/Y.
bool confirm(const std::string& question);

// Single-line text entry on the last line.
std::string prompt_text(const std::string& label, std::size_t maximum_length);

// Temporarily leaves curses mode so child processes can use the terminal.
void suspend_terminal();
void resume_terminal();

struct HelpEntry {
		std::string key;
		std::string description;
};

// Centered modal listing key bindings; dismiss with any key.
void show_help(const std::string& title, const std::vector<HelpEntry>& entries);

// Number of table rows that fit between the header area and the footer.
std::size_t visible_rows();

// Adjusts offset so the selected row stays on screen.
void keep_visible(std::size_t selected, std::size_t& offset);

// Section title with a dim subtitle, drawn in the content area.
void heading_area(int y, int x, std::string_view title, std::string_view subtitle);

// Dimmed key-hint list, e.g. {"r refresh", "q quit"}, drawn on one line.
void hint_line(int y, int x, const std::vector<std::string>& hints);

struct Column {
		std::string title;
		int width; // <= 0: take the remaining space.
};

struct Cell {
		std::string text;
		int color_pair{Muted};
		bool bold{};
};

// Generic table: header row, per-cell coloring, selection highlight, scrollbar.
template <typename Rows, typename CellFn>
void draw_table(int top, int left, int width, const Rows& rows, const std::vector<Column>& columns, std::size_t selected, std::size_t offset, CellFn&& cells) {
	// Resolve column geometry once.
	std::vector<int> offsets(columns.size());
	std::vector<int> widths(columns.size());
	int fixed = 0;
	for (const auto& column : columns) {
		if (column.width > 0) {
			fixed += column.width + 2;
		}
	}
	const int flex_width = width - fixed - 2;
	int x = 0;
	for (std::size_t index = 0; index < columns.size(); ++index) {
		offsets[index] = x;
		widths[index]  = columns[index].width > 0 ? columns[index].width : flex_width;
		x += widths[index] + 2;
	}

	attron(A_BOLD | COLOR_PAIR(Accent));
	for (std::size_t index = 0; index < columns.size(); ++index) {
		text_at(top, left + offsets[index], columns[index].title, widths[index]);
	}
	attroff(A_BOLD | COLOR_PAIR(Accent));
	attron(COLOR_PAIR(Muted) | A_DIM);
	text_at(top + 1, left, std::string(static_cast<std::size_t>(width - 2), '-'));
	attroff(COLOR_PAIR(Muted) | A_DIM);

	const int row_area = static_cast<int>(visible_rows());
	for (int row = 0; row < row_area; ++row) {
		const std::size_t index = offset + static_cast<std::size_t>(row);
		if (index >= rows.size()) {
			break;
		}
		const bool is_selected = index == selected;
		if (is_selected) {
			fill(top + 2 + row, left - 1, width, Selection, true);
		}
		const auto row_cells = cells(index, rows);
		for (std::size_t column = 0; column < row_cells.size() && column < columns.size(); ++column) {
			const auto& cell = row_cells[column];
			if (!is_selected) {
				if (cell.bold) attron(COLOR_PAIR(cell.color_pair) | A_BOLD);
				else attron(COLOR_PAIR(cell.color_pair));
			} else {
				attron(A_BOLD);
			}
			text_at(top + 2 + row, left + offsets[column], cell.text, widths[column]);
			if (!is_selected) {
				if (cell.bold) attroff(COLOR_PAIR(cell.color_pair) | A_BOLD);
				else attroff(COLOR_PAIR(cell.color_pair));
			} else {
				attroff(A_BOLD);
			}
		}
	}

	// Scrollbar on the right edge of the table.
	if (rows.size() > static_cast<std::size_t>(row_area) && width > 3) {
		const int track = row_area;
		const int thumb = std::max(1, track * row_area / static_cast<int>(rows.size()));
		const int position = rows.empty() ? 0
		    : static_cast<int>(offset * static_cast<std::size_t>(std::max(1, track - thumb)) /
			       std::max<std::size_t>(rows.size() - static_cast<std::size_t>(row_area), 1));
		attron(COLOR_PAIR(Muted) | A_DIM);
		for (int row = 0; row < track; ++row) {
			text_at(top + 2 + row, left + width - 2, ".");
		}
		attroff(COLOR_PAIR(Muted) | A_DIM);
		attron(COLOR_PAIR(Accent));
		for (int row = position; row < position + thumb && row < track; ++row) {
			text_at(top + 2 + row, left + width - 2, "#");
		}
		attroff(COLOR_PAIR(Accent));
	}
}

} // namespace apadana::ui
