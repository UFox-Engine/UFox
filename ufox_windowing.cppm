//
// Created by b-boy on 05.10.2025.
//
module;
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <string>
#include <stdexcept>
#include <memory>

export module ufox_windowing;

import glm;
import ufox_lib;

export namespace ufox::windowing {
    constexpr glm::vec2 DEFAULT_WINDOW_SIZE = {800, 600};


}


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


    class UFoxWindow {
    public:
        explicit UFoxWindow(const std::string& title, WindowFlag flags = WindowFlag::None)
            : UFoxWindow(title, static_cast<Uint32>(flags)) {}

        UFoxWindow(const std::string &title, Uint32 flags):_window{nullptr, SDL_DestroyWindow} {
            if (!SDL_Init(SDL_INIT_VIDEO)) throw SDLException("Failed to initialize SDL");

            SDL_DisplayID primary = SDL_GetPrimaryDisplay();
            SDL_Rect usableBounds{};
            SDL_GetDisplayUsableBounds(primary, &usableBounds);

            // Create window while ensuring proper cleanup on failure
            SDL_Window *rawWindow = SDL_CreateWindow(title.c_str(),
                usableBounds.w - 4, usableBounds.h - 34, flags);

            if (!rawWindow) throw SDLException("Failed to create window");

            _window.reset(rawWindow);

            SDL_SetWindowPosition(_window.get(), 2, 32);

            UpdateRootPanel();

            if (flags & SDL_WINDOW_VULKAN && !SDL_Vulkan_LoadLibrary(nullptr)) throw SDLException("Failed to load Vulkan library");
        }
        ~UFoxWindow() = default;

        void Show() const {
            SDL_ShowWindow(_window.get());
        }

        void Hide() const {
            SDL_HideWindow(_window.get());
        }


    private:
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> _window;
        Panel _rootPanel{};

        void UpdateRootPanel() {
            SDL_GetWindowPosition(_window.get(), &_rootPanel.x, &_rootPanel.y);
            SDL_GetWindowSize(_window.get(), &_rootPanel.width, &_rootPanel.height);
            _rootPanel.print();
        }
    };




}
