cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_CXX_STANDARD 23)



project(UFoxEngine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD_REQUIRED OFF)
set(CMAKE_CXX_EXTENSIONS OFF)






# Use C++23 without compiler-specific extensions


# Let CMake scan and manage C++ module dependencies
# set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Optional: Output dir for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

# SPDX-License-Identifier: MIT
#
# SPDX-FileCopyrightText: Copyright (c) 2019-2023 Lars Melchior and contributors

set(CPM_DOWNLOAD_VERSION 0.42.0)
set(CPM_HASH_SUM "2020b4fc42dba44817983e06342e682ecfc3d2f484a581f11cc5731fbe4dce8a")

if(CPM_SOURCE_CACHE)
    set(CPM_DOWNLOAD_LOCATION "${CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
elseif(DEFINED ENV{CPM_SOURCE_CACHE})
    set(CPM_DOWNLOAD_LOCATION "$ENV{CPM_SOURCE_CACHE}/cpm/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
else()
    set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")
endif()

# Expand relative path. This is important if the provided path contains a tilde (~)
get_filename_component(CPM_DOWNLOAD_LOCATION ${CPM_DOWNLOAD_LOCATION} ABSOLUTE)

file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION} EXPECTED_HASH SHA256=${CPM_HASH_SUM}
)

include(${CPM_DOWNLOAD_LOCATION})


CPMAddPackage(NAME SDL3 GITHUB_REPOSITORY libsdl-org/SDL GIT_TAG release-3.2.24 OPTIONS "SDL_VULKAN ON" "SDL_STATIC OFF")
CPMAddPackage(NAME glfw GITHUB_REPOSITORY glfw/glfw GIT_TAG 3.4 OPTIONS "GLFW_BUILD_TESTS OFF" "GLFW_BUILD_EXAMPLES OFF" "GLFW_BUILD_DOCS OFF")
CPMAddPackage(NAME Vulkan-Headers GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers GIT_TAG v1.4.328)
CPMAddPackage(NAME glm GITHUB_REPOSITORY g-truc/glm GIT_TAG 1.0.1)
CPMAddPackage(NAME ktx GITHUB_REPOSITORY KhronosGroup/KTX-Software GIT_TAG v4.4.2 OPTIONS "KTX_FEATURE_TOOLS OFF" "KTX_FEATURE_TESTS OFF")
CPMAddPackage(NAME nlohmann_json VERSION 3.12.0
        # the git repo is incredibly large, so we download the archived include directory
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip
        URL_HASH SHA256=b8cb0ef2dd7f57f18933997c9934bb1fa962594f701cd5a8d3c2c80541559372
)

if (nlohmann_json_ADDED)
    add_library(nlohmann_json INTERFACE IMPORTED)
    target_include_directories(nlohmann_json INTERFACE ${nlohmann_json_SOURCE_DIR}/include)
endif()

#target_compile_definitions(Vulkan-Headers INTERFACE
        #"VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=OFF"
#)

find_package(Freetype REQUIRED)

# Enable USE_SDL define
option(USE_SDL "Enable SDL3 support" ON)



add_library(glm-module)
target_sources(glm-module
        PUBLIC
        FILE_SET CXX_MODULES
        BASE_DIRS ${glm_SOURCE_DIR}
        FILES ${glm_SOURCE_DIR}/glm/glm.cppm
)

target_link_libraries(glm-module
        PUBLIC
        glm::glm-header-only # One or the other
        #glm::glm # One or the other
)

set(LIBS)
list(APPEND LIBS SDL3::SDL3 glfw Vulkan-Headers glm-module Freetype::Freetype ktx nlohmann_json)


add_executable(UFoxEngine)

# Regular sources
target_sources(UFoxEngine
        PRIVATE
        src/main.cpp
)


# Add USE_SDL define if enabled
if(USE_SDL)
    target_compile_definitions(UFoxEngine PRIVATE USE_SDL=1)
    message(STATUS "Enable SDL3 support")
endif()

# Explicitly declare module interface files
target_sources(UFoxEngine
        PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
        src/hello_world.cppm
        src/ufox_Lib.cppm
        src/ufox_graphic_device.cppm
        src/ufox_input.cppm
        src/ufox_geometry.cppm
        src/ufox_render.cppm
        src/ufox_resource_manager.cppm
        src/ufox_gui.cppm
        src/ufox_engine.cppm
)

# Link everything
target_link_libraries(UFoxEngine PRIVATE ${LIBS})

find_program(GLSL_VALIDATOR glslangValidator HINTS $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
## find all the shader files under the shaders folder
file(GLOB_RECURSE GLSL_SOURCE_FILES
        "${PROJECT_SOURCE_DIR}/res/shaders/*.frag"
        "${PROJECT_SOURCE_DIR}/res/shaders/*.vert"
        "${PROJECT_SOURCE_DIR}/res/shaders/*.comp"
)

## find all the shader files under the shaders folder
file(GLOB_RECURSE SHADER_SLANG_SOURCES
        "${PROJECT_SOURCE_DIR}/res/shaders/*.slang"
)

## iterate each shader
foreach(GLSL ${GLSL_SOURCE_FILES})
    message(STATUS "BUILDING SHADER")
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/bin/res/shaders/${FILE_NAME}.spv")
    message(STATUS ${GLSL})
    ##execute glslang command to compile that specific shader
    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)



function (add_slang_shader_target TARGET)
    cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
    set (SHADERS_DIR "${PROJECT_SOURCE_DIR}/bin/res/shaders")
    get_filename_component(FILE_NAME ${SHADER_SOURCES} NAME)
    set(FULL_PATH "${SHADERS_DIR}/${FILE_NAME}.spv")
    set (ENTRY_POINTS -entry vertMain -entry fragMain)
    add_custom_command (
            OUTPUT ${SHADERS_DIR}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    )
    add_custom_command (
            OUTPUT  ${FULL_PATH}
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${FILE_NAME}.spv
            WORKING_DIRECTORY ${SHADERS_DIR}
            DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
            COMMENT "Compiling Slang Shaders, target: ${SHADER_SOURCES}, out: ${FULL_PATH}"
            VERBATIM
    )
    add_custom_target (${TARGET} DEPENDS ${FULL_PATH})
endfunction()


add_custom_target(
        glsl-shaders
        ALL DEPENDS ${SPIRV_BINARY_FILES}
)


add_dependencies(UFoxEngine glsl-shaders)

foreach(SLANG ${SHADER_SLANG_SOURCES})
    get_filename_component(FILE_NAME ${SLANG} NAME)
    add_slang_shader_target(slang_${FILE_NAME} SOURCES ${SLANG})
    add_dependencies(UFoxEngine slang_${FILE_NAME})
endforeach()
