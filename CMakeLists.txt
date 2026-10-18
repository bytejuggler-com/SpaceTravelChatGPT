cmake_minimum_required(VERSION 3.24)
project(Astra VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT WIN32)
    message(FATAL_ERROR "This first release targets Windows with Direct3D 12 / DXIL.")
endif()
set(SDL3_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/SDL3-3.4.18/cmake" CACHE PATH "SDL3 CMake directory")
find_package(SDL3 CONFIG REQUIRED)
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/imgui")
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)
file(GLOB DXC_CANDIDATES "C:/Program Files (x86)/Windows Kits/10/bin/*/x64/dxc.exe")
list(SORT DXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
find_program(DXC_EXECUTABLE dxc HINTS ${DXC_CANDIDATES})
if(NOT DXC_EXECUTABLE AND DXC_CANDIDATES)
    list(GET DXC_CANDIDATES 0 DXC_EXECUTABLE)
endif()
if(NOT DXC_EXECUTABLE)
    message(FATAL_ERROR "DXC is missing. Install the Windows 10/11 SDK, or set DXC_EXECUTABLE.")
endif()
function(compile_shader NAME PROFILE)
    set(OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders/${NAME}.dxil")
    add_custom_command(OUTPUT "${OUT}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders"
        COMMAND "${DXC_EXECUTABLE}" -T ${PROFILE} -E main -O3 -Ges
            -I "${CMAKE_CURRENT_SOURCE_DIR}/shaders" -Fo "${OUT}"
            "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${NAME}.hlsl"
        DEPENDS "shaders/${NAME}.hlsl" "shaders/common.hlsli" VERBATIM)
    set(SHADER_OUTPUTS ${SHADER_OUTPUTS} "${OUT}" PARENT_SCOPE)
endfunction()
compile_shader(fullscreen vs_6_0)
compile_shader(departure ps_6_0)
compile_shader(cruise ps_6_0)
compile_shader(solar ps_6_0)
compile_shader(galaxy ps_6_0)
compile_shader(particles vs_6_0)
compile_shader(starlight ps_6_0)
compile_shader(bloom ps_6_0)
compile_shader(composite ps_6_0)
add_custom_target(shaders ALL DEPENDS ${SHADER_OUTPUTS})
add_executable(Astra WIN32 src/main.cpp src/core.cpp src/audio.cpp src/renderer.cpp src/ui.cpp)
target_include_directories(Astra PRIVATE src)
target_link_libraries(Astra PRIVATE imgui SDL3::SDL3)
target_compile_definitions(Astra PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(Astra PRIVATE /W4 /permissive- /utf-8)
endif()
add_dependencies(Astra shaders)
add_custom_command(TARGET Astra POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:Astra>)
add_custom_target(stage_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_BINARY_DIR}/shaders" "$<TARGET_FILE_DIR:Astra>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/config" "$<TARGET_FILE_DIR:Astra>/config"
    DEPENDS Astra shaders)
enable_testing()
add_executable(astra_tests tests/core_tests.cpp src/core.cpp src/audio.cpp)
target_include_directories(astra_tests PRIVATE src)
target_link_libraries(astra_tests PRIVATE SDL3::SDL3)
add_custom_command(TARGET astra_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:astra_tests>)
add_test(NAME timeline_and_soundtrack COMMAND astra_tests)
