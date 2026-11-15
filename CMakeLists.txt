cmake_minimum_required(VERSION 3.20)

project(UnifiedEngineeringWorkbench
    VERSION 1.0.0
    DESCRIPTION "Unified Engineering Workbench - R&D Engineering Assessment"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
enable_testing()

add_subdirectory(backend)
