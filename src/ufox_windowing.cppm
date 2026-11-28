//
// Created by b-boy on 28.11.2026.
//
module;
#ifdef USE_SDL

#include <SDL3/SDL.h>

#include <SDL3/SDL_vulkan.h>


#else

#include <GLFW/glfw3.h>

#endif

#include <vulkan/vulkan_raii.hpp>
#include <ufox_engine_export.h>

export module ufox_windowing;

import ufox_mathf;
import ufox_gpu_lib;

namespace ufox::windowing {
  export struct UFOX_ENGINE_EXPORT WindowResource {
  #ifdef USE_SDL
    using WindowHandle = SDL_Window;
    using DestroyFunc = decltype(&SDL_DestroyWindow);
  #else
    using WindowHandle = GLFWwindow;
    using DestroyFunc = decltype(&glfwDestroyWindow);
  #endif

    explicit WindowResource(const vk::raii::Instance& instance, WindowHandle* wnd): handle{wnd,
  #ifdef USE_SDL
        SDL_DestroyWindow}
  #else
      glfwDestroyWindow}
  #endif
    {
      VkSurfaceKHR _surface;
      int x = 0, y = 0, width = 0, height = 0;
  #ifdef USE_SDL
      if ( !SDL_Vulkan_CreateSurface( wnd, *instance, nullptr, &_surface )) {
        throw std::runtime_error( "Failed to create window surface!" );
      }

      SDL_GetWindowSize(wnd, &width, &height);
      SDL_GetWindowPosition(wnd, &x, &y);
  #else

      if (const VkResult err = glfwCreateWindowSurface( *instance, wnd, nullptr, &_surface ); err != VK_SUCCESS )
        throw std::runtime_error( "Failed to create window!" );

      glfwGetFramebufferSize(wnd, &width, &height);
      extent = vk::Extent2D{static_cast<uint32_t> (width), static_cast<uint32_t> (height)};
      glfwGetWindowPos(wnd, &x, &y);

  #endif
      extent = vk::Extent2D{static_cast<uint32_t> (width), static_cast<uint32_t> (height)};
      position = vk::Offset2D{x, y};
      surface.emplace(instance, _surface);
    }


    std::unique_ptr<WindowHandle, DestroyFunc>          handle;
    vk::Offset2D                                        position;
    vk::Extent2D                                        extent;
    std::optional<vk::raii::SurfaceKHR>                 surface{};
    std::optional<gpu::SwapchainResource>               swapchainResource{};

    // Getters
    [[nodiscard]] WindowHandle* getHandle() const { return handle.get(); }

    template<mathf::Arithmetic T>
bool getExtent(T& width, T& height) const {
      int w = 0, h = 0;
#ifdef USE_SDL
      SDL_GetWindowSize(handle.get(), &w, &h);
#else
      glfwGetFramebufferSize(handle.get(), &w, &h);
#endif
      width  = static_cast<T>(w);
      height = static_cast<T>(h);

      return w < 1 || h < 1;   // test the ints: no float/unsigned surprises
    }
  };

  export UFOX_ENGINE_EXPORT WindowResource CreateWindow(const vk::raii::Instance& instance,const std::string_view  windowName,const vk::Extent2D extent ) {
#ifdef USE_SDL
        struct sdlContext {
            sdlContext() {
                if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD)) {
                    throw std::runtime_error("Failed to initialize SDL");
                }
            }

            ~sdlContext() { SDL_Quit(); }
        };

        static auto sdlCtx = sdlContext();

        (void)sdlCtx;


        Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
        SDL_Window* window = SDL_CreateWindow(windowName.data(), static_cast<int>(extent.width), static_cast<int>(extent.height), flags);

        if (!window) {
            throw std::runtime_error("Failed to create SDL window");
        }


        return WindowResource{instance,window};

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

        return WindowResource{instance,window};

#endif

    }
}