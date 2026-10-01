#include "apadana/tui.hpp"

#include <iostream>
#include <string_view>
#include <unistd.h>

namespace {
constexpr std::string_view version = "0.3.0";

void print_help() {
	std::cout << "Usage: apadana [OPTION]\n\n"
		     "A distro-agnostic Unix/Linux system manager.\n\n"
		     "  --version   show version\n"
		     "  --help      show this help\n";
}
} // namespace

int main(const int argc, char** argv) {
	for (int index = 1; index < argc; ++index) {
		const std::string_view argument(argv[index]);
		if (argument == "--help" || argument == "-h") {
			print_help();
			return 0;
		}
		if (argument == "--version" || argument == "-V") {
			std::cout << "Apadana " << version << '\n';
			return 0;
		}
		std::cerr << "apadana: unknown option: " << argument << '\n';
		return 2;
	}
	if (::isatty(STDIN_FILENO) == 0 || ::isatty(STDOUT_FILENO) == 0) {
		std::cerr << "apadana: the TUI requires an interactive terminal\n";
		return 1;
	}
	return apadana::Application{}.run();
}
