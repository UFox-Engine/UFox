cmake_minimum_required(VERSION 4.0.0)
set(CMAKE_CXX_STANDARD 23)

project(UFoxEngine LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Use C++23 without compiler-specific extensions


# Let CMake scan and manage C++ module dependencies
# set(CMAKE_CXX_SCAN_FOR_MODULES ON)

# Optional: Output dir for binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

add_executable(UFoxEngine)

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


CPMAddPackage(NAME SDL3 GITHUB_REPOSITORY libsdl-org/SDL GIT_TAG release-3.4.2 OPTIONS "SDL_VULKAN ON" "SDL_STATIC OFF")
CPMAddPackage(NAME glfw GITHUB_REPOSITORY glfw/glfw GIT_TAG 3.4 OPTIONS "GLFW_BUILD_TESTS OFF" "GLFW_BUILD_EXAMPLES OFF" "GLFW_BUILD_DOCS OFF")
CPMAddPackage(NAME Vulkan-Headers GITHUB_REPOSITORY KhronosGroup/Vulkan-Headers GIT_TAG v1.4.352)
CPMAddPackage(NAME glm GITHUB_REPOSITORY g-truc/glm GIT_TAG 1.0.3)
CPMAddPackage(NAME ktx GITHUB_REPOSITORY KhronosGroup/KTX-Software GIT_TAG v4.4.2 OPTIONS "KTX_FEATURE_TOOLS ON" "KTX_FEATURE_TESTS OFF")
CPMAddPackage(NAME nlohmann_json VERSION 3.12.0
        # the git repo is incredibly large, so we download the archived include directory
        URL https://github.com/nlohmann/json/releases/download/v3.12.0/include.zip
        URL_HASH SHA256=b8cb0ef2dd7f57f18933997c9934bb1fa962594f701cd5a8d3c2c80541559372
)

CPMAddPackage(NAME SDL3_image GITHUB_REPOSITORY libsdl-org/SDL_image GIT_TAG release-3.4.0)
CPMAddPackage(NAME tinyobjloader GITHUB_REPOSITORY tinyobjloader/tinyobjloader GIT_TAG v1.0.6 DOWNLOAD_ONLY YES)


CPMAddPackage(NAME msdf-atlas-gen GITHUB_REPOSITORY Chlumsky/msdf-atlas-gen GIT_TAG v1.4
        OPTIONS
        "MSDF_ATLAS_BUILD_STANDALONE OFF"
        "MSDF_ATLAS_USE_SKIA OFF"
        "MSDF_ATLAS_USE_VCPKG OFF"
)

set(SLANG_VERSION 2026.22.1)

if(WIN32)
    set(SLANG_PLATFORM windows-x86_64)
    set(SLANG_LIB slang-compiler.dll)
elseif(APPLE)
    set(SLANG_PLATFORM macos-${CMAKE_SYSTEM_PROCESSOR})
    set(SLANG_LIB libslang-compiler.dylib)
else()
    set(SLANG_PLATFORM linux-${CMAKE_SYSTEM_PROCESSOR})
    set(SLANG_LIB libslang-compiler.so)
endif()

CPMAddPackage(
        NAME slang
        URL "https://github.com/shader-slang/slang/releases/download/v${SLANG_VERSION}/slang-${SLANG_VERSION}-${SLANG_PLATFORM}.zip"
        DOWNLOAD_ONLY YES
)



if(tinyobjloader_ADDED)
    # Because it's header-only we just create an interface target
    add_library(tinyobjloader INTERFACE)
    target_include_directories(tinyobjloader
            INTERFACE
            "${tinyobjloader_SOURCE_DIR}"
    )

    # Optional: very common defines used in many projects
    target_compile_definitions(tinyobjloader
            INTERFACE
            TINYOBJLOADER_IMPLEMENTATION   # if you want to compile .cc in one TU
            # TINYOBJLOADER_USE_DOUBLE     # ← uncomment if you need double precision
    )
endif()


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


if(slang_ADDED)
    add_library(slang::slang SHARED IMPORTED)
    set_target_properties(slang::slang PROPERTIES
            IMPORTED_LOCATION "${slang_SOURCE_DIR}/bin/${SLANG_LIB}"
            INTERFACE_INCLUDE_DIRECTORIES "${slang_SOURCE_DIR}/include"
    )
    if(WIN32)
        set_target_properties(slang::slang PROPERTIES
                IMPORTED_IMPLIB "${slang_SOURCE_DIR}/lib/slang-compiler.lib"
        )
    endif()

    # copy next to the exe
    add_custom_command(TARGET UFoxEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:slang::slang>" "$<TARGET_FILE_DIR:UFoxEngine>"
    )
endif()

include(GenerateExportHeader)

add_library(UFOX_ENGINE_LIB SHARED)

target_sources(UFOX_ENGINE_LIB
        PUBLIC
        FILE_SET CXX_MODULES
        FILES
        src/ufox_key.cppm
        src/ufox_nanoid.cppm
        src/ufox_core.cppm
        src/ufox_gpu_lib.cppm
        src/ufox_windowing.cppm
)

target_compile_features(UFOX_ENGINE_LIB PUBLIC cxx_std_23)

set_target_properties(UFOX_ENGINE_LIB PROPERTIES
        PREFIX ""
        OUTPUT_NAME "UFoxEngine"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

generate_export_header(UFOX_ENGINE_LIB
        BASE_NAME UFOX_ENGINE
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/ufox_engine_export.h
)

target_sources(UFOX_ENGINE_LIB
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
        FILES ${CMAKE_CURRENT_BINARY_DIR}/ufox_engine_export.h
)

add_library(ufox_beta SHARED)

target_sources(ufox_beta
        PUBLIC
        FILE_SET CXX_MODULES
        FILES
        src/ufox_beta.cppm
)

target_compile_features(ufox_beta PUBLIC cxx_std_23) 

# Visibility (important for shared libs)
set_target_properties(ufox_beta PROPERTIES
        PREFIX ""
        OUTPUT_NAME "ufox_beta"
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

generate_export_header(ufox_beta
        BASE_NAME UFOX_BETA
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/ufox_beta_export.h
)

target_sources(ufox_beta
        PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_BINARY_DIR}
        FILES ${CMAKE_CURRENT_BINARY_DIR}/ufox_beta_export.h
)

add_library(ufox_mathf STATIC)
target_sources(ufox_mathf
        PUBLIC
        FILE_SET CXX_MODULES
        FILES
        src/ufox_mathf.cppm
)
target_link_libraries(ufox_mathf PUBLIC glm-module)



target_link_libraries(ufox_beta PRIVATE ufox_mathf)

set(LIBS)
list(APPEND LIBS
        Vulkan-Headers
        Freetype::Freetype
        ktx
        nlohmann_json
        glfw
        glm-module
        SDL3_image::SDL3_image
        SDL3::SDL3 tinyobjloader
        msdf-atlas-gen::msdf-atlas-gen
        slang::slang
        ufox_beta
        ufox_mathf
        UFOX_ENGINE_LIB
)

# Regular sources
target_sources(UFoxEngine
        PRIVATE
        src/main.cpp
)


add_library(UFox-modules)
target_sources(UFox-modules
        PUBLIC
        FILE_SET CXX_MODULES TYPE CXX_MODULES
        FILES
        src/ufox_engine_app.cppm
        src/ufox_Lib.cppm
        src/ufox_gpu_core.cppm
        src/ufox_input.cppm
        src/ufox_engine_lib.cppm
        src/ufox_engine_core.cppm
        src/ufox_geometry_lib.cppm
        src/ufox_geometry_core.cppm
        src/ufox_render_lib.cppm
        src/ufox_render_core.cppm
        src/ufox_font_lib.cppm
        src/ufox_font_core.cppm
        src/ufox_gui_lib.cppm
        src/ufox_gui_core.cppm

)

target_link_libraries(UFox-modules PRIVATE ${LIBS})

# Add USE_SDL define if enabled
if(USE_SDL)
    target_compile_definitions(UFox-modules PRIVATE USE_SDL=1)
    target_compile_definitions(UFoxEngine PRIVATE USE_SDL=1)
    target_compile_definitions(UFOX_ENGINE_LIB PRIVATE USE_SDL=1)
    message(STATUS "Enable SDL3 support")
endif()

# Link everything
target_link_libraries(UFoxEngine PRIVATE ${LIBS} UFox-modules)

set(RESOURCES_SOURCE_DIR "${CMAKE_SOURCE_DIR}/res")
set(RESOURCES_DEST_DIR   "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/res")

file(MAKE_DIRECTORY ${RESOURCES_DEST_DIR})

file(GLOB_RECURSE RESOURCE_FILES LIST_DIRECTORIES false "${RESOURCES_SOURCE_DIR}/*")

add_custom_target(CopyResources ALL
        COMMENT "Copying project resources → ${RESOURCES_DEST_DIR}"
)

foreach(RESOURCE_FILE ${RESOURCE_FILES})
    file(RELATIVE_PATH REL_PATH ${RESOURCES_SOURCE_DIR} ${RESOURCE_FILE})
    set(DEST_FILE "${RESOURCES_DEST_DIR}/${REL_PATH}")
    get_filename_component(DEST_DIR ${DEST_FILE} DIRECTORY)

    add_custom_command(
            TARGET CopyResources POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${DEST_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${RESOURCE_FILE}" "${DEST_FILE}"
            COMMENT "Copying ${REL_PATH}"
            VERBATIM
    )
endforeach()

add_dependencies(UFoxEngine CopyResources)

add_subdirectory(thirdparty/nanoid)
add_dependencies(UFoxEngine nanoid)
target_link_libraries(UFOX_ENGINE_LIB PRIVATE nanoid SDL3::SDL3 glm-module glfw Vulkan-Headers ufox_mathf)
target_link_libraries(ufox_beta PRIVATE UFOX_ENGINE_LIB)

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
    get_filename_component(SPIRV_DIR "${SPIRV}" DIRECTORY)

    message(STATUS ${GLSL})

    add_custom_command(
            OUTPUT ${SPIRV}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SPIRV_DIR}"
            COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
            DEPENDS ${GLSL}
            COMMENT "Compiling GLSL Shader: ${GLSL}"
            VERBATIM
    )

    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

function (add_slang_shader_target TARGET)
    cmake_parse_arguments ("SHADER" "" "" "SOURCES" ${ARGN})
    set (SHADERS_DIR "${PROJECT_SOURCE_DIR}/bin/res/shaders")
    get_filename_component(FILE_NAME ${SHADER_SOURCES} NAME)
    set(FULL_PATH "${SHADERS_DIR}/${FILE_NAME}.spv")

    set (ENTRY_POINTS -entry vertMain -entry fragMain)

    add_custom_command (
            OUTPUT  ${FULL_PATH}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADERS_DIR}"
            COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES}
            -target spirv
            -profile spirv_1_6+spvShaderNonUniformEXT
            -emit-spirv-directly
            -fvk-use-entrypoint-name
            ${ENTRY_POINTS}
            -o "${FULL_PATH}"
            DEPENDS ${SHADER_SOURCES}
            COMMENT "Compiling Slang Shader: ${SHADER_SOURCES}"
            VERBATIM
    )

    add_custom_target (${TARGET} DEPENDS ${FULL_PATH})
    add_dependencies(${TARGET} CopyResources)
endfunction()


add_custom_target(
        glsl-shaders
        ALL DEPENDS ${SPIRV_BINARY_FILES}
)

add_dependencies(glsl-shaders CopyResources)
add_dependencies(UFoxEngine glsl-shaders)

foreach(SLANG ${SHADER_SLANG_SOURCES})
    get_filename_component(FILE_NAME ${SLANG} NAME)
    add_slang_shader_target(slang_${FILE_NAME} SOURCES ${SLANG})
    add_dependencies(UFoxEngine slang_${FILE_NAME})
endforeach()

