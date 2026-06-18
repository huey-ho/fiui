cmake_minimum_required(VERSION 3.24)

project(fiui_package_smoke LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(fiui CONFIG REQUIRED)

add_executable(fiui_package_smoke main.cpp)
target_link_libraries(fiui_package_smoke PRIVATE fiui::fiui)

if(WIN32)
    add_custom_command(TARGET fiui_package_smoke POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:fiui::fiui>
            $<TARGET_FILE_DIR:fiui_package_smoke>
    )
endif()

enable_testing()
add_test(NAME fiui_package_smoke COMMAND fiui_package_smoke)
set_tests_properties(fiui_package_smoke PROPERTIES TIMEOUT 30)
