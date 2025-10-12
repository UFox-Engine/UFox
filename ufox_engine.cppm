//
// Created by Puwiwad on 05.10.2025.
//
module;
#include <iostream>
#include <SDL3/SDL.h>
#include <vulkan/vulkan_raii.hpp>
export module ufox_engine;
import ufox_lib;
import ufox_windowing;
import ufox_graphic;

export namespace ufox {
    // Constants for configuration
    constexpr const char *ENGINE_NAME = "UFox";
    constexpr auto ENGINE_VERSION = VK_MAKE_VERSION(1, 0, 0);
    constexpr auto API_VERSION = vk::ApiVersion14;

    class Engine {
    public:
        Engine() = default;

        ~Engine() = default;

        void Init() {
            try {
                InitializeWindow();
                InitializeGPU();
            } catch (const std::exception &e) {
                HandleError(e);
            }
        }

        void Run() {
            bool isRunning = true;
            SDL_Event event;

            while (isRunning) {
                while (SDL_PollEvent(&event)) {
                    isRunning = HandleEvent(event);
                }
            }
        }

    private:
        struct EngineResources {
            windowing::sdl::UFoxWindowResource mainWindow;
            gpu::vulkan::GraphicDeviceResource gpu;
        };

        void InitializeWindow() {
            windowing::sdl::UFoxWindowCreateInfo windowInfo{
                ENGINE_NAME,
                windowing::sdl::WindowFlag::Vulkan | windowing::sdl::WindowFlag::Resizable
            };
            resources.mainWindow = windowing::sdl::CreateWindowResource(windowInfo);
        }

        void InitializeGPU() {
            auto createInfo = CreateGraphicDeviceInfo();
            resources.gpu = gpu::vulkan::CreateGraphicDeviceResource(
                createInfo,
                resources.mainWindow
            );
        }

        static gpu::vulkan::GraphicDeviceCreateInfo CreateGraphicDeviceInfo() {
            gpu::vulkan::GraphicDeviceCreateInfo createInfo{};
            createInfo.appInfo.pApplicationName = ENGINE_NAME;
            createInfo.appInfo.applicationVersion = ENGINE_VERSION;
            createInfo.appInfo.pEngineName = ENGINE_NAME;
            createInfo.appInfo.engineVersion = ENGINE_VERSION;
            createInfo.appInfo.apiVersion = API_VERSION;
            return createInfo;
        }

        bool HandleEvent(const SDL_Event &event) {
            switch (event.type) {
                case SDL_EVENT_QUIT:
                    return false;
                case SDL_EVENT_WINDOW_RESIZED:
                    windowing::sdl::UpdatePanel(resources.mainWindow);
                    break;
                default: ;
            }
            return true;
        }

        static void HandleError(const std::exception &e) {
            std::cerr << "Engine error: " << e.what() << std::endl;
            throw;
        }

        EngineResources resources;
    };
}