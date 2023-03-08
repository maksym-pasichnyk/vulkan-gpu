cmake_minimum_required(VERSION 3.24)
project(game)
include(FetchContent)

set(CMAKE_CXX_STANDARD 23)

# find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

FetchContent_Declare(glfw GIT_REPOSITORY https://github.com/glfw/glfw.git)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.89.3.zip CONFIGURE_COMMAND "" BUILD_COMMAND "")
FetchContent_MakeAvailable(imgui)

add_library(imgui STATIC "${imgui_SOURCE_DIR}/imgui.h" "${imgui_SOURCE_DIR}/imgui.cpp" "${imgui_SOURCE_DIR}/imconfig.h" "${imgui_SOURCE_DIR}/imgui_draw.cpp" "${imgui_SOURCE_DIR}/imgui_tables.cpp" "${imgui_SOURCE_DIR}/imgui_internal.h" "${imgui_SOURCE_DIR}/imgui_widgets.cpp" "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h" "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp")

target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")
target_compile_definitions(imgui PUBLIC -DIMGUI_DEFINE_MATH_OPERATORS)
target_compile_definitions(imgui PUBLIC -DIMGUI_USER_CONFIG=<${CMAKE_CURRENT_SOURCE_DIR}/src/imgui_config_override.hpp>)

add_executable(game src/main.cpp src/numeric.hpp src/enum.hpp src/result.hpp src/gpu.hpp src/VulkanRenderer.hpp src/ImGuiRenderer.hpp src/imgui_config_override.hpp)
target_link_libraries(game PUBLIC #[[SDL2::SDL2]] Vulkan::Vulkan imgui glfw)
target_compile_definitions(game PUBLIC -DGLFW_INCLUDE_NONE -DGLFW_INCLUDE_VULKAN)

function(target_compile_shaders TARGET_NAME)
    foreach(SHADER ${ARGN})
        # Compile shader to SPIR-V
        add_custom_command(
            OUTPUT ${SHADER}.spv
            COMMAND glslc ${SHADER} -o ${SHADER}.spv
            DEPENDS ${SHADER}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        )

        # Add SPIR-V to target
        target_sources(${TARGET_NAME} PRIVATE ${SHADER}.spv)

        # Add SPIR-V to CMake's dependency graph
        set_source_files_properties(${SHADER}.spv PROPERTIES GENERATED TRUE)
    endforeach()
endfunction()

file(GLOB_RECURSE SHADER_SOURCES CONFIGURE_DEPENDS
    "shaders/*.vert"
    "shaders/*.frag"
    "shaders/*.comp"
)

target_compile_shaders(game ${SHADER_SOURCES})