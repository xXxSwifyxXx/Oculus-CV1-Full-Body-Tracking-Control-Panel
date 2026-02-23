# Build configuration for ODTKRA industrial refactor.
#
# This file intentionally keeps target wiring explicit so maintainers can quickly reason about
# dependency boundaries between core runtime logic, entrypoint applications, and tests.
#
# Key intent:
# - Build shared core logic once (odtkra_core).
# - Link thin entrypoints (odtkra_agent, odtkra_control_panel) against that core.
# - Keep test target lightweight and always available in CI.
#
cmake_minimum_required(VERSION 3.22)
project(ODTKRAIndustrial LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(odtkra_core
    src/core/anti_sleep_service.cpp
    src/core/config.cpp
    src/core/diagnostics.cpp
    src/core/logger.cpp
    src/core/oculus_discovery.cpp
    src/core/path_utils.cpp
    src/core/process_utils.cpp
    src/core/steam_discovery.cpp
    src/core/touchlink_installer.cpp
)

target_include_directories(odtkra_core PUBLIC src)
target_link_libraries(odtkra_core PUBLIC user32 shell32 advapi32 urlmon)

add_executable(odtkra_agent apps/agent/main.cpp)
target_link_libraries(odtkra_agent PRIVATE odtkra_core)

add_executable(odtkra_control_panel WIN32 apps/control_panel/main.cpp)
target_link_libraries(odtkra_control_panel PRIVATE odtkra_core)

enable_testing()
add_executable(odtkra_tests
    tests/test_config.cpp
    tests/test_diagnostics.cpp
    tests/test_main.cpp
)
target_link_libraries(odtkra_tests PRIVATE odtkra_core)
add_test(NAME odtkra_tests COMMAND odtkra_tests)
