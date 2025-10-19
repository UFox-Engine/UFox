//
// Created by Puwiwad on 05.10.2025.
//
module;
#include <iostream>
#include <string>
#include <stdexcept>
#include <memory>
#include <optional>
#include <vulkan/vulkan_raii.hpp>

#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif

export module ufox_windowing;

import ufox_lib;



export namespace ufox::windowing {
    WindowResource CreateWindow( std::string const & windowName, vk::Extent2D const & extent ) {
#ifdef USE_SDL

        struct sdlContext {
            sdlContext() {
                if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
                    debug::log(debug::LogLevel::eError, "Failed to initialize SDL: {}", SDL_GetError());
                    throw std::runtime_error("Failed to initialize SDL");
                }

            }
            ~sdlContext() { SDL_Quit(); }
        };
        static auto sdlCtx = sdlContext();
        (void)sdlCtx;

        // SDL_DisplayID primary = SDL_GetPrimaryDisplay();
        // SDL_Rect usableBounds{};
        // SDL_GetDisplayUsableBounds(primary, &usableBounds);

        Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
        SDL_Window* window = SDL_CreateWindow(windowName.c_str(), static_cast<int>(extent.width), static_cast<int>(extent.height), flags);
        if (!window) {
            throw std::runtime_error("Failed to create SDL window");
        }

        //SDL_SetWindowPosition(window, 2, 32);
        debug::log(debug::LogLevel::eInfo, "Created SDL window: {} ({}x{})", windowName, static_cast<int>(extent.width), static_cast<int>(extent.height));
        return WindowResource{window};
#else

        struct glfwContext {
            glfwContext() {
                glfwInit();
                glfwSetErrorCallback([](int error, const char* msg) {
                    debug::log(debug::LogLevel::eError, "GLFW error ({}): {}", error, msg);
                });
            }
            ~glfwContext() { glfwTerminate(); }
        };
        static auto glfwCtx = glfwContext();
        (void)glfwCtx;

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        GLFWwindow* window = glfwCreateWindow(static_cast<int>(extent.width), static_cast<int>(extent.height), windowName.c_str(), nullptr, nullptr);
        if (!window) {
            debug::log(debug::LogLevel::eError, "Failed to create GLFW window: {}", windowName);
            throw std::runtime_error("Failed to create GLFW window");
        }
        debug::log(debug::LogLevel::eInfo, "Created GLFW window: {} ({}x{})", windowName, static_cast<int>(extent.width), static_cast<int>(extent.height));
        return WindowResource{window};
#endif
    }
}
