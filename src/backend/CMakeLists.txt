cmake_minimum_required(VERSION 3.20)

# Library with common backend logic
add_library(workbench_core STATIC
    src/common/system_info.cpp
    src/fea/thermal_1d.cpp
    src/fea/thermal_batch.cpp
    src/cad/stl_parser.cpp
    src/cad/ansys_parser.cpp
    src/multicore/thread_pool.cpp
    src/embedded/device_adapter.cpp
    src/ml/backprop.cpp
    src/server/http_server.cpp
)

target_include_directories(workbench_core PUBLIC
    include
)

target_compile_features(workbench_core PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(workbench_core PRIVATE /W4 /O2)
    target_link_libraries(workbench_core PUBLIC ws2_32)
else()
    target_compile_options(workbench_core PRIVATE -Wall -Wextra -Wpedantic -O3)
    find_package(Threads REQUIRED)
    target_link_libraries(workbench_core PUBLIC Threads::Threads)
endif()

# Main Application Server Executable
add_executable(workbench_server
    src/main.cpp
)
target_link_libraries(workbench_server PRIVATE workbench_core)
target_compile_features(workbench_server PRIVATE cxx_std_17)
if(MSVC)
    target_link_libraries(workbench_server PRIVATE shell32)
endif()

# Test Suite Executable
add_executable(workbench_tests
    ${CMAKE_SOURCE_DIR}/tests/test_main.cpp
)
target_link_libraries(workbench_tests PRIVATE workbench_core)
target_compile_features(workbench_tests PRIVATE cxx_std_17)

add_test(NAME AllWorkbenchTests COMMAND workbench_tests)

