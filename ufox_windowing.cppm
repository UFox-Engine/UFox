//
// Created by Puwiwad on 05.10.2025.
//
module;
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string>
#include <stdexcept>
#include <memory>
#include <optional>
#include <vulkan/vulkan_raii.hpp>

export module ufox_windowing;

import glm;
import ufox_lib;



export namespace ufox::windowing::sdl {

    struct SDLException : std::runtime_error {explicit SDLException(const std::string& msg) : std::runtime_error(msg + ": " + SDL_GetError()) {}};

    enum class WindowFlag : Uint32 {
        None = 0,
        Borderless = SDL_WINDOW_BORDERLESS,
        Resizable = SDL_WINDOW_RESIZABLE,
        Maximized = SDL_WINDOW_MAXIMIZED,
        Minimized = SDL_WINDOW_MINIMIZED,
        Vulkan = SDL_WINDOW_VULKAN,
        OpenGL = SDL_WINDOW_OPENGL,
        Fullscreen = SDL_WINDOW_FULLSCREEN,

    };

    inline WindowFlag operator|(WindowFlag a, WindowFlag b) {
        return static_cast<WindowFlag>(static_cast<Uint32>(a) | static_cast<Uint32>(b));
    }
    inline WindowFlag operator|=(WindowFlag& a, WindowFlag b) {
        a = a | b;
        return a;
    }

    struct UFoxWindowCreateInfo {
        std::string title;
        WindowFlag flags;
    };

    struct UFoxWindowResource {
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{nullptr, SDL_DestroyWindow};
        Panel panel{};
    };

    void UpdatePanel(UFoxWindowResource& resource) {
        SDL_GetWindowSize(resource.window.get(), &resource.panel.width, &resource.panel.height);
        resource.panel.print();
    }

    void Show(const UFoxWindowResource& resource) {
        SDL_ShowWindow(resource.window.get());
    }

    void Hide(const UFoxWindowResource& resource) {
        SDL_HideWindow(resource.window.get());
    }

    UFoxWindowResource CreateWindowResource(const UFoxWindowCreateInfo& info) {
        UFoxWindowResource resource{};

        if (!SDL_Init(SDL_INIT_VIDEO)) throw SDLException("Failed to initialize SDL");

        SDL_DisplayID primary = SDL_GetPrimaryDisplay();
        SDL_Rect usableBounds{};
        SDL_GetDisplayUsableBounds(primary, &usableBounds);

        // Create window while ensuring proper cleanup on failure
        auto flags = static_cast<Uint32>(info.flags);
        SDL_Window *rawWindow = SDL_CreateWindow(info.title.c_str(),
            usableBounds.w - 4, usableBounds.h - 34, flags);

        if (!rawWindow) throw SDLException("Failed to create window");

        resource.window.reset(rawWindow);

        SDL_SetWindowPosition(resource.window.get(), 2, 32);

        UpdatePanel(resource);

        if (flags & SDL_WINDOW_VULKAN && !SDL_Vulkan_LoadLibrary(nullptr)) throw SDLException("Failed to load Vulkan library");
        return resource;
    }

}
