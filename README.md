# Apadana

Apadana is a distro-agnostic Unix/Linux system manager inspired by neofetch,
Flatpak, and control-panel software. Its goal is to expose system information,
processes, services, packages, storage, networking, and useful administrative
tools through one approachable terminal interface.

## Design principles

- Detect capabilities instead of assuming a distribution.
- Keep package managers and other platform tools behind a common backend
  interface (`PackageManagerBackend`); APT, DNF, and Pacman are implemented and
  the first available backend is selected automatically.
- Support multiple package ecosystems on the same machine.
- Separate unprivileged inspection from privileged mutations.
- Never construct privileged shell commands from unchecked user input.

## Current status

The current release provides a colorful ncurses control-panel interface, a
neofetch-like system overview, local user-account management, package backends
for APT, DNF, and Pacman with automatic detection, live process
inspection/control, systemd service control, mounted-filesystem usage, and a
read-only diagnostics view for network traffic and security posture.

## Build

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
./build/apadana
```

Apadana requires CMake 3.20 or newer and a C++17 compiler. The terminal
frontend additionally requires the wide-character ncurses development package
(`libncurses-dev` on Debian-based systems, `ncurses` on Arch Linux).

The application is organized as a shared `apadana_core` library with separate
terminal and graphical frontends. The terminal executable remains
`./build/apadana` (CMake target `apadana_cli`). To include the GTK3 frontend,
install GTK3 development files (`libgtk-3-dev` on Debian-based systems) and
build the `apadana-gtk` target; when GTK3 is missing, CMake skips that target
and still builds the terminal application. Configure with
`-DAPADANA_BUILD_GTK_GUI=OFF` to disable GTK discovery explicitly.
To build only the graphical frontend, configure with
`-DAPADANA_BUILD_CLI=OFF`; this avoids the ncurses dependency as well.

User-management backends are selected by capability: shadow-utils on Linux or
`pw` on FreeBSD. Unsupported systems retain read-only account browsing.

## TUI controls

- Left/Right or Tab: change control-panel module
- Up/Down or j/k: move through tables
- PageUp/PageDown: scroll tables by ten rows
- g/G or Home/End: jump to the first or last row
- ?: open the key-binding help overlay
- q: quit

Every table shows a scroll position indicator on its right edge, and action
results are reported in the colored status bar. Diagnostics inspects network
interface activity, byte counters, ASLR status, and available firewall
tooling; press `r` to refresh.

User controls:

- a: create a local user with a home directory
- l/u: lock or unlock the selected account
- r: refresh user accounts

Package controls (APT, DNF, or Pacman, depending on the host):

- s: search available packages
- a: show installed packages
- g: show available upgrades
- i/d: install or remove the selected package
- u: refresh the package index
- U: upgrade all packages
- r: refresh the current package view

Storage controls:

- Up/Down: browse mounted filesystems and their usage bars
- r: refresh the mount list

Process controls:

- r: refresh and sort processes by lifetime CPU usage
- t: send SIGTERM to the selected process
- K: send SIGKILL to the selected process

Systemd service controls:

- r: refresh service units
- s/x: start or stop the selected service
- R: restart the selected service
- e/d: enable or disable the selected service at boot

Account mutations always ask for confirmation. Apadana executes the native
account tool directly (never through a shell) and uses sudo or pkexec when
privilege elevation is required. Root-account locking is intentionally blocked.

Process and service mutations always ask for confirmation. Process signaling is
performed directly without invoking a shell, and PID 1 and Apadana itself are
protected. Service commands validate unit names and use sudo or pkexec when
privilege elevation is required.

## Planned modules

- Additional package backends such as Zypper, APK, and Flatpak
- Networking, logs, and system utilities

## License

MIT
