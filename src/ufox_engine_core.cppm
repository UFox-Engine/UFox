//
// Created by b-boy on 25.02.2026.
//

module;

#include <fstream>
#include <memory>

#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif

#include <iostream>
#include <optional>
#include <string>
#include <vulkan/vulkan_raii.hpp>

export module ufox_engine_core;

import ufox_lib;
import ufox_engine_lib;
import ufox_graphic_device;


export namespace ufox::engine {


#ifdef USE_SDL
constexpr bool FramebufferResizeCallback(void* userData,SDL_Event* event);
#else
constexpr void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
#endif



  class UFoxWindow {
  public:
    gpu::vulkan::GraphicDeviceCreateInfo            gpuCreateInfo{};
    gpu::vulkan::GPUResources                       gpuResource{};
    std::optional<windowing::WindowResource>        windowResource{};
    std::optional<gpu::vulkan::FrameResource>       frameResource{};

    [[nodiscard]] constexpr bool isRunning() const { return running; }

    void addResizeEventHandlers(ResizeEventHandler cb, void* user) {
      resizeEventHandlers.emplace_back(cb, user);
    }

    void removeOnResizeEventHandlers(void* user) {  // simple by pointer
      std::erase_if(resizeEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void addSystemInitEventHandlers(SystemInitEventHandler cb, void* user) {
      systemInitEventHandlers.emplace_back(cb, user);
    }

    void removeSystemInitEventHandlers(void* user) {
      std::erase_if(systemInitEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void addResourceInitEventHandlers(ResourceInitEventHandler cb, void* user) {
      resourceInitEventHandlers.emplace_back(cb, user);
    }

    void removeResourceInitEventHandlers(void* user) {
      std::erase_if(resourceInitEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void addStartEventHandlers(StartEventHandler cb, void* user) {
      startEventHandlers.emplace_back(cb, user);
    }

    void removeStartEventHandlers(void* user) {
      std::erase_if(startEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void addUpdateBufferEventHandlers(UpdateBufferEventHandler cb, void* user) {
      updateBufferEventHandlers.emplace_back(cb, user);
    }

    void removeUpdateBufferEventHandlers(void* user) {
      std::erase_if(updateBufferEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void addDrawCanvasEventHandlers(DrawCanvasEventHandler cb, void* user) {
      renderPassEventHandlers.emplace_back(cb, user);
    }

    void removeDrawCanvasEventHandlers(void* user) {
      std::erase_if(renderPassEventHandlers, [user](auto& p){ return p.second == user; });
    }

    static std::vector<char> ReadFile(const std::string& filename) {
      std::ifstream file(filename, std::ios::ate | std::ios::binary);

      if (!file.is_open()) {
        throw std::runtime_error("failed to open file!");
      }

      std::vector<char> buffer(file.tellg());

      file.seekg(0, std::ios::beg);
      file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

      file.close();

      return buffer;
    }

    [[nodiscard]] uint32_t getImageCount() const {
      return windowResource->swapchainResource.value().getImageCount();
    }

    void pauseFrame() {
      pauseRendering = true;
    }

    template<Arithmetic T>
    void onSize(const T& width, const T& height) {
      const auto fw = static_cast<float>(width);
      const auto fh = static_cast<float>(height);

      framebufferResized = true;
      recreateSwapchain();

      for (auto [callback, used] : resizeEventHandlers) {
        if (callback) callback(fw, fh, used);
      }
      drawFrame();
    }

    void run() {
#ifdef USE_SDL
      running = true;
      SDL_Event event;

      executeInitEvents();
      executeResourceInitEvents();

      while (running) {
        executeStartEvents();
        sdlPollEvents(event, running);
        drawFrame();
      }
#else
      executeInitEvents();
      executeResourceInitEvents();

      while (!glfwWindowShouldClose(windowResource->getHandle())) {
        executeStartEvents();
        glfwPollEvents();
        drawFrame();
      }
    }
#endif
  }

  private:
    std::vector<std::pair<SystemInitEventHandler, void*>> systemInitEventHandlers{};
    std::vector<std::pair<ResourceInitEventHandler, void*>> resourceInitEventHandlers{};
    std::vector<std::pair<ResizeEventHandler, void*>> resizeEventHandlers{};
    std::vector<std::pair<StartEventHandler, void*>> startEventHandlers{};
    std::vector<std::pair<UpdateBufferEventHandler, void*>> updateBufferEventHandlers{};
    std::vector<std::pair<DrawCanvasEventHandler, void*>> renderPassEventHandlers{};

    bool running {false};
    bool framebufferResized {false};
    bool pauseRendering {false};
    bool startFrameExecuted {false};

    void executeInitEvents() {
      for (auto [callback, used] : systemInitEventHandlers) {
        if (callback) callback(used);
      }
    }

    void executeResourceInitEvents() {
      for (auto [callback, used] : resourceInitEventHandlers) {
        if (callback) callback(used);
      }
    }

    void executeStartEvents() {
      if (startFrameExecuted) return;
      for (auto [callback, used] : startEventHandlers) {
        if (callback) callback(used);
      }
      startFrameExecuted = true;
    }

    void executeUpdateBufferEvents(const uint32_t& imageIndex) {
      for (auto [callback, used] : updateBufferEventHandlers) {
        if (callback) callback(imageIndex, used);
      }
    }

    void executeRenderPassEvents(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex) {
      for (auto [callback, used] : renderPassEventHandlers) {
        if (callback) callback(cmb, imageIndex, *windowResource, used);
      }
    }

    void recordCommandBuffer(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex) {
      std::array<vk::ClearValue, 2> clearValues{};
      clearValues[0].color        = vk::ClearColorValue{0.0f, 0.0f, 0.0f,1.0f};
      clearValues[1].depthStencil = vk::ClearDepthStencilValue{0.0f, 0};
      vk::Extent2D extent = windowResource->extent;
      cmb.begin({});
      vk::ImageSubresourceRange range{};
      range.aspectMask     = vk::ImageAspectFlagBits::eColor;
      range.baseMipLevel   = 0;
      range.levelCount     = VK_REMAINING_MIP_LEVELS;
      range.baseArrayLayer = 0;
      range.layerCount     = VK_REMAINING_ARRAY_LAYERS;
      gpu::vulkan::TransitionImageLayout(
          cmb, windowResource->swapchainResource->getCurrentImage(),
          vk::PipelineStageFlagBits2::eTopOfPipe,
          vk::PipelineStageFlagBits2::eColorAttachmentOutput ,
          {},
          vk::AccessFlagBits2::eColorAttachmentWrite,
          vk::ImageLayout::eUndefined,
          vk::ImageLayout::eColorAttachmentOptimal,range
          );


      executeRenderPassEvents(cmb, imageIndex);

      gpu::vulkan::TransitionImageLayout(cmb, windowResource->swapchainResource->getCurrentImage(),
          vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, range);
      cmb.end();
    }

    vk::Extent2D recreateSwapchain() {
        // Handle window minimization
        int width = 0, height = 0;
        pauseRendering = windowResource->getExtent(width, height);
        const auto size = vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        if (pauseRendering) return size;

        gpuResource.device->waitIdle();
        windowResource->extent = size;
        windowResource->swapchainResource->Clear();
        gpu::vulkan::ReMakeSwapchainResource(*windowResource->swapchainResource, gpuResource, *windowResource, vk::ImageUsageFlagBits::eColorAttachment);

        return size;
      }

    void drawFrame() {
      while ( vk::Result::eTimeout == gpuResource.device->waitForFences( *frameResource->getCurrentDrawFence(), vk::True, UINT64_MAX ) )
        ;

      if (pauseRendering) {return;}

      auto [result, imageIndex] = windowResource->swapchainResource->swapChain->acquireNextImage(UINT64_MAX, frameResource->getCurrentPresentCompleteSemaphore(), nullptr);
      windowResource->swapchainResource->currentImageIndex = imageIndex;
      gpuResource.device->resetFences(*frameResource->getCurrentDrawFence());
      if (result == vk::Result::eErrorOutOfDateKHR) {
        debug::log(debug::LogLevel::eInfo, "Recreating swapchain due to acquireNextImage failure");
        recreateSwapchain();
        return;
      }
      if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
      }

      executeUpdateBufferEvents(imageIndex);

      const vk::raii::CommandBuffer& cmb = frameResource->getCurrentCommandBuffer();
      cmb.reset();
      recordCommandBuffer(cmb, imageIndex);


      vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
      vk::SubmitInfo submitInfo{};
      submitInfo.setWaitSemaphoreCount(1)
                .setPWaitSemaphores(&*frameResource->getCurrentPresentCompleteSemaphore())
                .setPWaitDstStageMask(&waitDestinationStageMask)
                .setCommandBufferCount(1)
                .setPCommandBuffers(&*cmb)
                .setSignalSemaphoreCount(1)
                .setPSignalSemaphores(&*windowResource->swapchainResource->getCurrentRenderFinishedSemaphore());
      gpuResource.graphicsQueue->submit(submitInfo, frameResource->getCurrentDrawFence());
      vk::PresentInfoKHR presentInfo{};
      presentInfo.setWaitSemaphoreCount(1)
                 .setPWaitSemaphores(&*windowResource->swapchainResource->getCurrentRenderFinishedSemaphore())
                 .setSwapchainCount(1)
                 .setPSwapchains(&**windowResource->swapchainResource->swapChain)
                 .setPImageIndices(&windowResource->swapchainResource->currentImageIndex);

      result = gpuResource.presentQueue->presentKHR(presentInfo);
      if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
        framebufferResized = false;
        recreateSwapchain();
        return;
      }
      if (result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present swapchain image");
      }
      frameResource->ContinueNextFrame();
    }

    void sdlPollEvents(SDL_Event& event , bool& running) {
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT: {
          if (gpuResource.device) {
            gpuResource.device->waitIdle();
          }
          pauseRendering = true;
          if (windowResource->swapchainResource) {
            windowResource->swapchainResource->Clear();
          }

          running = false;
          SDL_RemoveEventWatch(FramebufferResizeCallback, this);
          break;
        }
        case SDL_EVENT_WINDOW_MOVED: {
          windowResource->position = vk::Offset2D{event.window.data1,event.window.data2};
          break;
        }
        case SDL_EVENT_WINDOW_MINIMIZED: {
          pauseRendering = true;

          break;
        }
        case SDL_EVENT_WINDOW_RESTORED: {
          if (pauseRendering)
            pauseRendering = false;

          break;
        }
        case SDL_EVENT_MOUSE_BUTTON_DOWN:{
          // input::MouseButton button = input::MouseButton::eNone;
          // input::ActionPhase phase = input::ActionPhase::eStart;
          //
          // if (event.button.button == SDL_BUTTON_LEFT) {button = input::MouseButton::eLeft;}
          // else if (event.button.button == SDL_BUTTON_RIGHT) {button = input::MouseButton::eRight;}
          // else if (event.button.button == SDL_BUTTON_MIDDLE) {button = input::MouseButton::eMiddle;}
          //
          // input::CatchMouseButton(*inputResource, button, phase, 1,0);

          break;
        }
        case SDL_EVENT_MOUSE_BUTTON_UP: {
          // input::MouseButton button = input::MouseButton::eNone;
          // input::ActionPhase phase = input::ActionPhase::eEnd;
          //
          // if (event.button.button == SDL_BUTTON_LEFT) {button = input::MouseButton::eLeft;}
          // else if (event.button.button == SDL_BUTTON_RIGHT) {button = input::MouseButton::eRight;}
          // else if (event.button.button == SDL_BUTTON_MIDDLE) {button = input::MouseButton::eMiddle;}
          //
          // input::CatchMouseButton(*inputResource, button, phase,0,0);

          break;
        }

        default: {}
        }
      }
    }
  };

#ifdef USE_SDL
  constexpr bool FramebufferResizeCallback(void* userData,SDL_Event* event) {
    if (event->type == SDL_EVENT_WINDOW_RESIZED) {
      auto* app = static_cast<UFoxWindow*>(userData);
      app->onSize(event->window.data1, event->window.data2);
    }
    return true;
  }
#else
  constexpr void FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto app = static_cast<UFoxWindow*>(glfwGetWindowUserPointer(window));
    app->onSize(width, height);
  }
#endif


  std::unique_ptr<UFoxWindow> CreateUFoxWindow(const std::string& title, const uint32_t& width, const uint32_t& height) {
    auto window = std::make_unique<UFoxWindow>();

    window->gpuCreateInfo = gpu::vulkan::CreateGraphicDeviceInfo();
    window->gpuResource.context.emplace();
    window->gpuResource.instance.emplace(gpu::vulkan::MakeInstance(window->gpuResource, window->gpuCreateInfo));
    window->gpuResource.physicalDevice.emplace(gpu::vulkan::PickBestPhysicalDevice(window->gpuResource));
    window->windowResource = windowing::CreateWindow(*window->gpuResource.instance, title, {width, height});
    window->gpuResource.queueFamilyIndices.emplace(gpu::vulkan::FindGraphicsAndPresentQueueFamilyIndex(window->gpuResource, *window->windowResource));
    window->gpuResource.device.emplace(gpu::vulkan::MakeDevice(window->gpuResource, window->gpuCreateInfo));
    window->gpuResource.commandPool.emplace(gpu::vulkan::MakeCommandPool(window->gpuResource));
    window->gpuResource.graphicsQueue.emplace(gpu::vulkan::MakeGraphicsQueue(window->gpuResource));
    window->gpuResource.presentQueue.emplace(gpu::vulkan::MakePresentQueue(window->gpuResource));
    window->windowResource->swapchainResource.emplace(gpu::vulkan::MakeSwapchainResource(window->gpuResource,*window->windowResource,gpu::vulkan::COLOR_IMAGE_USAGE_FLAG));
    window->frameResource.emplace(gpu::vulkan::MakeFrameResource(window->gpuResource, gpu::vulkan::SIGNALED_FENCE_CREATE_FLAG));


#ifdef USE_SDL
    SDL_AddEventWatch(FramebufferResizeCallback, window.get());
#else
    glfwSetWindowUserPointer(window->windowResource->getHandle(), window.get());
    glfwSetFramebufferSizeCallback(window->windowResource->getHandle(), FramebufferResizeCallback);
#endif

    return window;
  }
}