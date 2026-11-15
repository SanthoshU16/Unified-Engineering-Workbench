cmake_minimum_required(VERSION 3.20)

project(UnifiedEngineeringWorkbench
    VERSION 1.0.0
    DESCRIPTION "Unified Engineering Workbench - R&D Engineering Assessment"
    LANGUAGES CXX
)

if(MSVC)
    # Keep the standalone Windows runtime independent of a separately installed VC++ runtime.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
enable_testing()

add_subdirectory(src/backend)

if(WIN32)
    add_executable(Launch WIN32 packaging/windows/launcher.cpp)
    target_compile_features(Launch PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(Launch PRIVATE /W4 /O2)
    endif()

    add_custom_target(package_windows_runtime
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/dist/windows-x64"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:Launch>" "${CMAKE_SOURCE_DIR}/dist/windows-x64/Launch.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:workbench_server>" "${CMAKE_SOURCE_DIR}/dist/windows-x64/workbench_server.exe"
        DEPENDS Launch workbench_server
        COMMENT "Copying the standalone Windows x64 runtime to dist/windows-x64"
    )
endif()
