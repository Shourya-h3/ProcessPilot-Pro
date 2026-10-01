cmake_minimum_required(VERSION 3.14)
project(ProcessPilotPro VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler Warnings and Optimization
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -pedantic -pthread -O2)
endif()

# Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)

# --- 1. Common Library ---
add_library(pilot_common STATIC
    src/common/logger.cpp
)

# --- 2. Daemon Engine Library ---
add_library(pilot_daemon_engine STATIC
    src/daemon/config_parser.cpp
    src/daemon/dag_resolver.cpp
    src/daemon/process_manager.cpp
    src/daemon/health_checker.cpp
    src/daemon/self_healer.cpp
    src/daemon/ipc_server.cpp
    src/daemon/supervisor.cpp
)
target_link_libraries(pilot_daemon_engine PUBLIC pilot_common pthread)

# --- 3. Supervisor Daemon Binary (processpilotd) ---
add_executable(processpilotd src/daemon/main.cpp)
target_link_libraries(processpilotd PRIVATE pilot_daemon_engine)

# --- 4. CLI Control Tool (pilotctl) ---
add_executable(pilotctl
    src/cli/main.cpp
    src/cli/cli_commands.cpp
)
target_link_libraries(pilotctl PRIVATE pilot_common pthread)

# --- 5. Mock Worker Service (pilot_mock_svc) ---
add_executable(pilot_mock_svc services.d/mock_service.c)

# --- 6. Unit Tests ---
enable_testing()

add_executable(test_dag tests/test_dag.cpp)
target_link_libraries(test_dag PRIVATE pilot_daemon_engine)
add_test(NAME TestDAG COMMAND test_dag)

add_executable(test_parser tests/test_parser.cpp)
target_link_libraries(test_parser PRIVATE pilot_daemon_engine)
add_test(NAME TestParser COMMAND test_parser)

add_executable(test_healer tests/test_healer.cpp)
target_link_libraries(test_healer PRIVATE pilot_daemon_engine)
add_test(NAME TestHealer COMMAND test_healer)

add_executable(test_driver_harness tests/test_driver_harness.cpp)
target_link_libraries(test_driver_harness PRIVATE pilot_common)
add_test(NAME TestDriverHarness COMMAND test_driver_harness)
