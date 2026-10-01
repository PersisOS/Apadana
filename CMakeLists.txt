cmake_minimum_required(VERSION 3.20)

project(
    Apadana
    VERSION 0.3.0
    DESCRIPTION "A distro-agnostic Unix/Linux system manager"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(APADANA_BUILD_TESTS "Build Apadana tests" ON)
option(APADANA_BUILD_CLI "Build the ncurses terminal frontend" ON)
option(APADANA_BUILD_GTK_GUI "Build the GTK3 graphical frontend when GTK3 is available" ON)

add_library(apadana_core
    src/core/command.cpp
    src/core/package_manager.cpp
    src/core/platform.cpp
    src/core/process_manager.cpp
    src/core/service_manager.cpp
    src/core/storage_manager.cpp
    src/core/system_info.cpp
    src/core/user_manager.cpp
)

target_include_directories(apadana_core PUBLIC include)
set_target_properties(apadana_core PROPERTIES CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(apadana_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
elseif(MSVC)
    target_compile_options(apadana_core PRIVATE /W4 /permissive-)
endif()

if(APADANA_BUILD_CLI)
    set(CURSES_NEED_WIDE TRUE)
    find_package(Curses REQUIRED)
    add_executable(apadana_cli
        src/cli/main.cpp
        src/frontends/tui/application.cpp
        src/frontends/tui/pages.cpp
        src/frontends/tui/theme.cpp
        src/frontends/tui/widgets.cpp
    )
    set_target_properties(apadana_cli PROPERTIES OUTPUT_NAME apadana)
    target_include_directories(apadana_cli PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(apadana_cli PRIVATE apadana_core ${CURSES_LIBRARIES})
endif()

# The GUI is a separate frontend over apadana_core. Keep GTK optional so the
# existing terminal application can still be built on headless systems.
if(APADANA_BUILD_GTK_GUI)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GTK3 QUIET IMPORTED_TARGET gtk+-3.0)
    endif()
    if(GTK3_FOUND)
        add_executable(apadana-gtk src/gtk/main.cpp)
        target_link_libraries(apadana-gtk PRIVATE apadana_core PkgConfig::GTK3)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(apadana-gtk PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
        elseif(MSVC)
            target_compile_options(apadana-gtk PRIVATE /W4 /permissive-)
        endif()
        install(TARGETS apadana-gtk RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    else()
        message(STATUS "GTK3 development files not found; skipping apadana-gtk")
    endif()
endif()

if(APADANA_BUILD_CLI)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(apadana_cli PRIVATE -Wall -Wextra -Wpedantic -Wconversion)
    elseif(MSVC)
        target_compile_options(apadana_cli PRIVATE /W4 /permissive-)
    endif()
endif()

include(GNUInstallDirs)
if(APADANA_BUILD_CLI)
    install(TARGETS apadana_cli RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()

if(APADANA_BUILD_TESTS)
    enable_testing()
    add_executable(apadana_tests tests/core_tests.cpp)
    target_link_libraries(apadana_tests PRIVATE apadana_core)
    add_test(NAME apadana_core_tests COMMAND apadana_tests)
endif()
