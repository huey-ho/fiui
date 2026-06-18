cmake_minimum_required(VERSION 3.24)

project(fiui VERSION 0.1.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

option(FIUI_BUILD_EXAMPLES "Build fiui examples" ON)
option(FIUI_BUILD_TESTS "Build fiui tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(fiui SHARED
    src/app/app.cpp
    src/backend/d3d11_backend.cpp
    src/core/widget_impl.cpp
    src/diagnostics/diagnostics.cpp
    src/dirty/dirty_tracker.cpp
    src/event/event_system.cpp
    src/image/image_system.cpp
    src/layout/layout_system.cpp
    src/menu/menu_system.cpp
    src/platform/platform_system.cpp
    src/render/frame.cpp
    src/render/render_system.cpp
    src/resource/resource_system.cpp
    src/runtime/runtime.cpp
    src/scheduler/frame_scheduler.cpp
    src/style/style.cpp
    src/style/style_system.cpp
    src/text/text_system.cpp
    src/widget/widget.cpp
)

add_library(fiui::fiui ALIAS fiui)

target_include_directories(fiui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(fiui PRIVATE FIUI_BUILDING_DLL)

if(MSVC)
    target_compile_options(fiui PRIVATE /W4 /permissive-)
else()
    target_compile_options(fiui PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    target_compile_definitions(fiui PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(fiui PRIVATE user32 gdi32 d3d11 d3dcompiler d2d1 dxgi dwrite windowscodecs)
endif()

if(FIUI_BUILD_EXAMPLES)
    add_executable(fiui_hello examples/hello/main.cpp)
    target_link_libraries(fiui_hello PRIVATE fiui)

    add_executable(fiui_settings_panel examples/settings_panel/main.cpp)
    target_link_libraries(fiui_settings_panel PRIVATE fiui)

    add_executable(fiui_diagnostics_demo examples/diagnostics_demo/main.cpp)
    target_link_libraries(fiui_diagnostics_demo PRIVATE fiui)

    add_executable(fiui_control_center_demo examples/control_center_demo/main.cpp)
    target_link_libraries(fiui_control_center_demo PRIVATE fiui)
    add_custom_command(TARGET fiui_control_center_demo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/examples/control_center_demo/assets
            $<TARGET_FILE_DIR:fiui_control_center_demo>
    )
endif()

if(FIUI_BUILD_TESTS)
    enable_testing()

    add_executable(fiui_core_tests tests/core/core_tests.cpp)
    target_link_libraries(fiui_core_tests PRIVATE fiui)
    target_include_directories(fiui_core_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    add_test(NAME fiui_core_tests COMMAND fiui_core_tests)
    set_tests_properties(fiui_core_tests PROPERTIES TIMEOUT 30)

    add_executable(fiui_stability_tests tests/stability/stability_tests.cpp)
    target_link_libraries(fiui_stability_tests PRIVATE fiui)
    target_include_directories(fiui_stability_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    if(WIN32)
        target_link_libraries(fiui_stability_tests PRIVATE user32)
    endif()
    add_test(NAME fiui_stability_tests COMMAND fiui_stability_tests)
    set_tests_properties(fiui_stability_tests PROPERTIES TIMEOUT 30)

    if(TARGET fiui_settings_panel)
        add_test(NAME fiui_settings_panel_example COMMAND fiui_settings_panel)
        set_tests_properties(fiui_settings_panel_example PROPERTIES TIMEOUT 30)
    endif()

    if(TARGET fiui_control_center_demo)
        add_test(NAME fiui_control_center_demo COMMAND fiui_control_center_demo)
        set_tests_properties(fiui_control_center_demo PROPERTIES TIMEOUT 30)
    endif()
endif()

install(TARGETS fiui
    EXPORT fiuiTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY doc/ DESTINATION ${CMAKE_INSTALL_DOCDIR})
install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/fiuiConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/fiuiConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fiui
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/fiuiConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

install(EXPORT fiuiTargets
    NAMESPACE fiui::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fiui
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/fiuiConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/fiuiConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/fiui
)
