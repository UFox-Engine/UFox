cmake_minimum_required(VERSION 3.31)
project(UFoxEngine LANGUAGES CXX)

# Use C++23 without compiler-specific extensions
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
CPMAddPackage(NAME Vulkan-Headers GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers GIT_TAG v1.4.328)
CPMAddPackage(NAME glm GITHUB_REPOSITORY g-truc/glm GIT_TAG 1.0.1)
target_compile_definitions(Vulkan-Headers INTERFACE
        "VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=OFF"
)

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
list(APPEND LIBS SDL3::SDL3 Vulkan-Headers glm-module)

add_executable(UFoxEngine)

# Regular sources
target_sources(UFoxEngine
        PRIVATE
        main.cpp
)



# Explicitly declare module interface files
target_sources(UFoxEngine
        PRIVATE
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
        hello_world.cppm ufox_Lib.cppm ufox_windowing.cppm
)



# Link everything
target_link_libraries(UFoxEngine PRIVATE ${LIBS})

