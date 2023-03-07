cmake_minimum_required(VERSION 3.24)
project(game)

set(CMAKE_CXX_STANDARD 23)

find_package(SDL2 REQUIRED)
find_package(Vulkan REQUIRED)

add_executable(game src/main.cpp src/numeric.hpp src/enum.hpp src/result.hpp src/gpu.hpp src/vulkan.hpp)
target_link_libraries(game PUBLIC SDL2::SDL2 Vulkan::Vulkan)

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
