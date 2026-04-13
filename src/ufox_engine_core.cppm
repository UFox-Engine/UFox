//
// Created by b-boy on 25.02.2026.
//

module;

#include <chrono>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>
#include <unordered_set>

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
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "glm/gtc/quaternion.hpp"

export module ufox_engine_core;

import ufox_lib;
import ufox_gpu_lib;
import ufox_gpu_core;
import ufox_engine_lib;

import ufox_nanoid;

export namespace ufox::engine {

#ifdef USE_SDL
  constexpr bool FramebufferResizeCallback(void* userData,SDL_Event* event);
#else
  constexpr void FramebufferResizeCallback(GLFWwindow* window, int width, int height);
#endif

  template<Arithmetic T>
  constexpr void CalculateAspectRatio(const T& width, const T& height, float& data) noexcept{
    data = height == 0 ? 1.0f : static_cast<float>(width) / static_cast<float>(height);
  }

  constexpr void CalculateAspectRatio(const vk::Extent2D& extent, float& data) noexcept {
    CalculateAspectRatio(extent.width, extent.height, data);
  }

  [[nodiscard]] constexpr
  glm::vec3 GetForwardVector(const Transform& transform) noexcept {
    // -Z forward (standard in right-handed Vulkan/OpenGL convention)
    return glm::normalize(transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f));
  }

  [[nodiscard]] constexpr
  glm::vec3 GetBackwardVector(const Transform& transform) noexcept {
    return -GetForwardVector(transform);
  }

  [[nodiscard]] constexpr
  glm::vec3 GetUpVector(const Transform& transform) noexcept {
    return glm::normalize(transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f));
  }

  [[nodiscard]] constexpr
  glm::vec3 GetDownVector(const Transform& transform) noexcept {
    return -GetUpVector(transform);
  }

  [[nodiscard]] constexpr
  glm::vec3 GetRightVector(const Transform& transform) noexcept {
    return glm::normalize(transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f));
  }

  [[nodiscard]] constexpr
  glm::vec3 GetLeftVector(const Transform& transform) noexcept {
    return -GetRightVector(transform);
  }

  [[nodiscard]] constexpr glm::mat4 MakeTransformModel(const glm::vec3& position, const glm::quat& quaternion, const glm::vec3& scale = glm::vec3(1.0f)){
    return glm::translate(glm::mat4(1.0f), position) * glm::mat4_cast(quaternion) * glm::scale(glm::mat4(1.0f), scale) ;
  }

  [[nodiscard]] constexpr glm::mat4 MakeTransformModel(const glm::vec3& position, const glm::vec3& eulerDegrees, const glm::vec3& scale = glm::vec3(1.0f)){
    return MakeTransformModel(position, glm::quat(glm::radians(eulerDegrees)), scale);
  }

  [[nodiscard]] constexpr glm::mat4 MakeTransformModel(const Transform& transform) {
    return MakeTransformModel(transform.position, transform.rotation, transform.scale);
  }

  [[nodiscard]] constexpr glm::mat4 CalculateViewMatrix(const Transform& transform) noexcept {
    const glm::vec3 eye    = transform.position;
    const glm::vec3 center = eye + GetForwardVector(transform);
    const glm::vec3 up     = GetUpVector(transform);

    return glm::lookAt(eye, center, up);
  }

  [[nodiscard]] constexpr glm::mat4 CalculateViewMatrix(const Camera& camera) noexcept {
    return CalculateViewMatrix(camera.transform);
  }

  [[nodiscard]] constexpr glm::mat4 CalculateViewProjectionMatrix(const ViewProjectionType& type,
    const float& fov, const float& orthoSize, const float& aspectRatio, const float& nearPlane, const float& farPlane) noexcept {
    switch (type) {
    case ViewProjectionType::ePerspective: {
      return glm::perspective(glm::radians(fov),aspectRatio,nearPlane,farPlane);
    }
    case ViewProjectionType::eOrthographic: {
      const float halfW = orthoSize * aspectRatio * 0.5f;
      const float halfH = orthoSize * 0.5f;
      return glm::ortho(-halfW, halfW, -halfH, halfH,nearPlane, farPlane);
    }
    default: return {1.0f};
    }
  }

  [[nodiscard]] constexpr auto CalculateScreenProjectionMatrix(const float& width, const float& height) noexcept {
    return glm::ortho(0.0f, width, 0.0f, height, -1.0f, 0.0f);
  }


  class UFoxWindow {
  public:
    gpu::GraphicDeviceCreateInfo                          gpuCreateInfo{};
    gpu::GPUResources                                     gpuResource{};
    std::optional<gpu::WindowResource>         windowResource{};
    std::optional<gpu::FrameResource>                     frameResource{};
    std::optional<gpu::DepthImageResource>                depthResource{};


    [[nodiscard]] constexpr bool isRunning() const { return running; }

    void registerResizeEventHandlers(ResizeEventHandler cb, void* user) {
      resizeEventHandlers.emplace_back(cb, user);
    }

    void unregisterResizeEventHandlers(void* user) {  // simple by pointer
      std::erase_if(resizeEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void registerSystemInitEventHandlers(SystemInitEventHandler cb, void* user) {
      systemInitEventHandlers.emplace_back(cb, user);
    }

    void unregisterSystemInitEventHandlers(void* user) {
      std::erase_if(systemInitEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void registerResourceInitEventHandlers(ResourceInitEventHandler cb, void* user) {
      resourceInitEventHandlers.emplace_back(cb, user);
    }

    void unregisterResourceInitEventHandlers(void* user) {
      std::erase_if(resourceInitEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void registerStartEventHandlers(StartEventHandler cb, void* user) {
      startEventHandlers.emplace_back(cb, user);
    }

    void unregisterStartEventHandlers(void* user) {
      std::erase_if(startEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void registerUpdateBufferEventHandlers(UpdateBufferEventHandler cb, void* user) {
      updateBufferEventHandlers.emplace_back(cb, user);
    }

    void unregisterUpdateBufferEventHandlers(void* user) {
      std::erase_if(updateBufferEventHandlers, [user](auto& p){ return p.second == user; });
    }

    void registerDrawCanvasEventHandlers(DrawCanvasEventHandler cb, void* user) {
      renderPassEventHandlers.emplace_back(cb, user);
    }

    void unregisterDrawCanvasEventHandlers(void* user) {
      std::erase_if(renderPassEventHandlers, [user](auto& p){ return p.second == user; });
    }


    template<typename T>
    [[nodiscard]] static std::vector<T> ReadFileImpl(const std::string& filename) {
      std::ifstream file(filename, std::ios::ate | std::ios::binary);

      if (!file.is_open()) {
        debug::log(debug::LogLevel::eError, "Failed to open file: " + filename);
        return {};
      }

      const auto size = static_cast<std::size_t>(file.tellg());
      std::vector<T> buffer(size / sizeof(T));

      file.seekg(0, std::ios::beg);
      file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(size));

      if (!file) {
        debug::log(debug::LogLevel::eError, "Failed to read file: " + filename);
        return {};
      }

      return buffer;
    }

    [[nodiscard]] static std::vector<char> ReadFileChar(const std::string& filename) {
      return ReadFileImpl<char>(filename);
    }

    [[nodiscard]] static std::vector<std::byte> ReadFileBinary(const std::string& filename) {
      return ReadFileImpl<std::byte>(filename);
    }


    [[nodiscard]] uint32_t getImageCount() const {
      return windowResource->swapchainResource.value().getImageCount();
    }

    void pauseFrame() {
      pauseRendering = true;
    }

    void run() {
#ifdef USE_SDL
      running = true;
      SDL_Event event;

      float width, height = 0;
      windowResource->getExtent(width, height);

      executeInitEvents();
      executeResourceInitEvents(width,height);

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
#endif
    }

    template<Arithmetic T>
    void executeResizeEvents(const T& width, const T& height) {
      const auto fw = static_cast<float>(width);
      const auto fh = static_cast<float>(height);

      framebufferResized = true;
      recreateSwapchain();
      updateScreenProjectionBuffer(fw, fh);

      for (auto [callback, used] : resizeEventHandlers) {
        if (callback) callback(fw, fh, used);
      }

      drawFrame();
    }

    [[nodiscard]] vk::DescriptorBufferInfo MakeScreenViewProjectionBufferInfo() const {
      vk::DescriptorBufferInfo bufferInfo{};
      bufferInfo
        .setBuffer(*screenViewProjectionBuffer.value().data)
        .setOffset(0)
        .setRange(MAT4_BUFFER_SIZE);
      return bufferInfo;
    }

  private:
    std::vector<std::pair<SystemInitEventHandler, void*>>         systemInitEventHandlers{};
    std::vector<std::pair<ResourceInitEventHandler, void*>>       resourceInitEventHandlers{};
    std::vector<std::pair<ResizeEventHandler, void*>>             resizeEventHandlers{};
    std::vector<std::pair<StartEventHandler, void*>>              startEventHandlers{};
    std::vector<std::pair<UpdateBufferEventHandler, void*>>       updateBufferEventHandlers{};
    std::vector<std::pair<DrawCanvasEventHandler, void*>>         renderPassEventHandlers{};

    bool                                                          running {false};
    bool                                                          framebufferResized {false};
    bool                                                          pauseRendering {false};
    bool                                                          startFrameExecuted {false};

    std::optional<gpu::Buffer>                                    screenViewProjectionBuffer{};

    void initGPUBufferResource() {
      gpu::MakeBuffer(screenViewProjectionBuffer,gpuResource, MAT4_BUFFER_SIZE, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
      float width, height = 0;
      windowResource->getExtent(width, height);
      updateScreenProjectionBuffer(width, height);
    }

    void updateScreenProjectionBuffer(const float& width, const float& height) const {
      if (screenViewProjectionBuffer.has_value()) {
        const glm::mat4 viewProject = SCREEN_VIEW_METRIX * CalculateScreenProjectionMatrix(width, height);
        gpu::CopyToDevice(*screenViewProjectionBuffer->memory,&viewProject, MAT4_BUFFER_SIZE);
      }
    }

    void executeInitEvents() {
      initGPUBufferResource();
      for (auto [callback, used] : systemInitEventHandlers) {
        if (callback) callback(used);
      }
    }

    void executeResourceInitEvents(const float& width, const float& height) {
      for (auto [callback, used] : resourceInitEventHandlers) {
        if (callback) callback(width, height, used);
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

    void executeRenderPassEvents(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex, const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& depthAttachment) {
      for (auto [callback, used] : renderPassEventHandlers) {
        if (callback) callback(cmb, imageIndex, *windowResource, colorAttachment, depthAttachment, used);
      }
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
      gpu::ReMakeSwapchainResource(*windowResource->swapchainResource, gpuResource, *windowResource, vk::ImageUsageFlagBits::eColorAttachment);
      gpu::MakeDepthImage(gpuResource, depthResource, *windowResource->swapchainResource);

      return size;
    }

    void recordCommandBuffer(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex) {
      std::array<vk::ClearValue, 2> clearValues{};
      clearValues[0].color        = vk::ClearColorValue{0.0f, 0.0f, 0.0f,1.0f};
      clearValues[1].depthStencil = vk::ClearDepthStencilValue{0.0f, 0};

      cmb.begin({});
      vk::ImageSubresourceRange range{};
      range.aspectMask     = vk::ImageAspectFlagBits::eColor;
      range.baseMipLevel   = 0;
      range.levelCount     = 1;
      range.baseArrayLayer = 0;
      range.layerCount     = 1;

      vk::ImageSubresourceRange rangeDepth{};
      rangeDepth.aspectMask     = vk::ImageAspectFlagBits::eDepth;
      rangeDepth.baseMipLevel   = 0;
      rangeDepth.levelCount     = 1;
      rangeDepth.baseArrayLayer = 0;
      rangeDepth.layerCount     = 1;

      gpu::TransitionImageLayout(cmb, windowResource->swapchainResource->getCurrentImage(), windowResource->swapchainResource->colorFormat, range, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal );

      gpu::TransitionImageLayout(cmb, depthResource->image.data.value(), depthResource->format, rangeDepth, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthAttachmentOptimal );

      vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
      vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

      vk::RenderingAttachmentInfo colorAttachment{};
      colorAttachment.setImageView(windowResource->swapchainResource->getCurrentImageView())
                     .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                     .setLoadOp(vk::AttachmentLoadOp::eClear)
                     .setStoreOp(vk::AttachmentStoreOp::eStore)
                     .setClearValue(clearColor);

      vk::RenderingAttachmentInfo depthAttachment{};
      depthAttachment.setImageView(*depthResource->image.view)
                     .setImageLayout(vk::ImageLayout::eDepthAttachmentOptimal)
                     .setLoadOp(vk::AttachmentLoadOp::eClear)
                     .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                     .setClearValue(clearDepth);

      executeRenderPassEvents(cmb, imageIndex, colorAttachment, depthAttachment);

      gpu::TransitionImageLayout(cmb, windowResource->swapchainResource->getCurrentImage(), windowResource->swapchainResource->colorFormat, range, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR );

      cmb.end();
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
      frameResource->continueNextFrame();
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
    app->executeResizeEvents(event->window.data1, event->window.data2);
  }
  return true;
}
#else
constexpr void FramebufferResizeCallback(GLFWwindow* window, int width, int height) {
  auto app = static_cast<UFoxWindow*>(glfwGetWindowUserPointer(window));
  app->executeResizeEvents(width, height);
}
#endif


  std::unique_ptr<UFoxWindow> CreateUFoxWindow(const std::string& title, const uint32_t& width, const uint32_t& height) {
    auto window = std::make_unique<UFoxWindow>();

    window->gpuCreateInfo = gpu::CreateGraphicDeviceInfo();
    window->gpuResource.context.emplace();
    gpu::MakeInstance(window->gpuResource, window->gpuCreateInfo);
    gpu::PickBestPhysicalDevice(window->gpuResource);
    window->windowResource = gpu::CreateWindow(*window->gpuResource.instance, title, {width, height});
    gpu::FindGraphicsAndPresentQueueFamilyIndex(window->gpuResource, *window->windowResource);
    gpu::MakeDevice(window->gpuResource, window->gpuCreateInfo);
    gpu::MakeCommandPool(window->gpuResource);
    gpu::MakeGraphicsQueue(window->gpuResource);
    gpu::MakePresentQueue(window->gpuResource);
    gpu::MakeSwapchainResource(window->windowResource->swapchainResource, window->gpuResource, *window->windowResource, gpu::COLOR_IMAGE_USAGE_FLAG);
    gpu::MakeFrameResource(window->frameResource, window->gpuResource, gpu::SIGNALED_FENCE_CREATE_FLAG);
    gpu::MakeDepthImage(window->gpuResource,window->depthResource, *window->windowResource->swapchainResource);


#ifdef USE_SDL
    SDL_AddEventWatch(FramebufferResizeCallback, window.get());
#else
    glfwSetWindowUserPointer(window->windowResource->getHandle(), window.get());
    glfwSetFramebufferSizeCallback(window->windowResource->getHandle(), FramebufferResizeCallback);
#endif

    return window;
  }

  constexpr void EnsureDirectoryExists(const std::filesystem::path& dirPath) noexcept{
    if (dirPath.empty()) {
      debug::log(debug::LogLevel::eWarning,
                 "EnsureDirectoryExists ({}) : empty path",
                 dirPath.string());
      return;
    }

    try {
      if (std::filesystem::create_directories(dirPath)) {
        debug::log(debug::LogLevel::eInfo,
                   "EnsureDirectoryExists ({}) : created",
                   dirPath.string());
        return;
      }

      if (std::filesystem::is_directory(dirPath)) {
        debug::log(debug::LogLevel::eInfo,
                   "EnsureDirectoryExists ({}) : already exists",
                   dirPath.string());
        return;
      }

      debug::log(debug::LogLevel::eWarning,
                 "EnsureDirectoryExists ({}) : path exists but is not a directory",
                 dirPath.string());
    }
    catch (const std::filesystem::filesystem_error& e) {
      debug::log(debug::LogLevel::eError,
                 "EnsureDirectoryExists ({}) : {}",
                 dirPath.string(), e.what());
    }
    catch (const std::exception& e) {
      debug::log(debug::LogLevel::eError,
                 "EnsureDirectoryExists ({}) : {}",
                 dirPath.string(), e.what());
    }
  }

  std::string TimePointToIso8601(const std::chrono::file_clock::time_point& tp){
    const auto sysTime = std::chrono::clock_cast<std::chrono::system_clock>(tp);
    auto timeT   = std::chrono::system_clock::to_time_t(sysTime);

    std::tm utcTime{};
  #if defined(_WIN32)
    gmtime_s(&utcTime, &timeT);
  #else
    gmtime_r(&timeT, &utcTime);
  #endif

    std::ostringstream oss;
    oss << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
  }

  constexpr void MakeDirectoryContext(const std::filesystem::path& dirPath, DirectoryContext& outState) noexcept {
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
      debug::log(debug::LogLevel::eWarning,
                 "MakeDirectoryContext ({}) : not a valid directory", dirPath.string());
      return;
    }

    try {
      DirectoryContext state{};
      state.directoryPath = dirPath;
      uintmax_t totalSize = 0;

      for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath,std::filesystem::directory_options::skip_permission_denied)) {
        if (entry.is_regular_file()) {
          totalSize += entry.file_size();
        }
      }
      state.totalSizeBytes = totalSize;

      // Directory's last write time as ISO string
      auto lastWrite = std::filesystem::last_write_time(dirPath);
      state.lastWriteTimeIso = TimePointToIso8601(lastWrite);
      outState = state;
      debug::log(debug::LogLevel::eInfo, "MakeDirectoryContext ({}) : success", dirPath.string());
    } catch (const std::exception& e) {
      debug::log(debug::LogLevel::eError,"MakeDirectoryContext ({}) : {}", dirPath.string(), e.what());
    }
  }

  [[nodiscard]]constexpr bool DirectoryContextChanged(const DirectoryContext& oldState,const DirectoryContext& newState) noexcept {
    if (!oldState.isValid() || !newState.isValid()) return true;

    return oldState.lastWriteTimeIso != newState.lastWriteTimeIso ||
           oldState.totalSizeBytes   != newState.totalSizeBytes;
  }

  bool WriteDirectoryContextMetaData(const DirectoryContext& state) {
    if (!state.isValid()) {
      debug::log(debug::LogLevel::eWarning, "WriteDirectoryMeta: invalid DirectoryState");
      return false;
    }

    const std::string metaFilename = state.directoryPath.filename().string() + ".ufox.meta";
    const auto metaPath = state.directoryPath.parent_path() / metaFilename;

    nlohmann::json doc = {
      {"directory-context", {
                {"path",           state.directoryPath.string()},
                {"lastWriteTime",  state.lastWriteTimeIso},
                {"totalSizeBytes", state.totalSizeBytes}
      }}
    };

    std::ofstream file(metaPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
      debug::log(debug::LogLevel::eError, "WriteDirectoryMeta ({}) : Failed to open directory meta", metaPath.string());
      return false;
    }

    try {
      file << std::setw(2) << doc << std::endl;
      debug::log(debug::LogLevel::eInfo, "WriteDirectoryMeta ({}) : success" , metaPath.string());
      return true;
    }
    catch (const std::exception& e) {
      debug::log(debug::LogLevel::eError, "WriteDirectoryMeta ({}) : {}", metaPath.string(), e.what());
      return false;
    }
  }

  [[nodiscard]] DirectoryContext ReadDirectoryContextMetaData(const std::filesystem::path& dirPath) {
    DirectoryContext state{};
    state.directoryPath = dirPath;

    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath)) {
      return state;
    }

    const std::string metaFilename = dirPath.filename().string() + ".ufox.meta";
    const auto metaPath = dirPath.parent_path() / metaFilename;

    if (!std::filesystem::exists(metaPath)) {
      debug::log(debug::LogLevel::eInfo, "No previous directory meta found for {}", dirPath.string());
      return state;   // first run is normal
    }

    try {
      std::ifstream file(metaPath);
      nlohmann::json j = nlohmann::json::parse(file);

      const auto& m = j.at("directory-context");

      state.lastWriteTimeIso = m.value("lastWriteTime", "");
      state.totalSizeBytes   = m.value<uintmax_t>("totalSizeBytes", 0);

      debug::log(debug::LogLevel::eInfo, "Loaded directory state meta for {}", dirPath.string());
    }
    catch (const std::exception& e) {
      debug::log(debug::LogLevel::eWarning,
                 "Failed to read directory state meta {} : {}", metaPath.string(), e.what());
    }

    return state;
  }

  class ResourceManagerBase;
  void ReadResourceContextMetaData(const std::filesystem::path& rootDirectory,const std::span<const std::string_view>& sourceExtensions,ResourceManagerBase* manager);
  void WriteResourceContextMetaData(const ResourceContext& context);

  class ResourceManagerBase {
    public:
    virtual ~ResourceManagerBase() = default;
    explicit ResourceManagerBase(const gpu::GPUResources& gpu, const std::string_view& _directory,
                                 const std::span<const std::string_view> &extension) : directory(_directory), sourceExtensions(extension), gpuResources(&gpu) {


        EnsureDirectoryExists(directory);
        MakeDirectoryContext(directory, directoryContext);
        WriteDirectoryContextMetaData(directoryContext);

    }

    virtual void init() = 0;
    virtual void makeResource(const ResourceContextCreateInfo& info) = 0;

    [[nodiscard]] ResourceContext* getResourceContext(const ResourceID& id) noexcept {
      const auto it = container.find(id);
      return it != container.end() ? &it->second : nullptr;
    }

    [[nodiscard]] ResourceContext* getResourceContext(const std::string_view id) noexcept {
      const auto it = container.find(id);
      return it != container.end() ? &it->second : nullptr;
    }

    [[nodiscard]] ResourceContext* getResourceContext(const char* id) noexcept {
      return id ? getResourceContext(std::string_view{id}) : nullptr;
    }

    protected:
      const std::filesystem::path directory{};
      std::span<const std::string_view> sourceExtensions{};
      DirectoryContext directoryContext{};
      const gpu::GPUResources* gpuResources{nullptr};
      BuiltInResources builtInResources{};
      std::unordered_map<ResourceID, ResourceContext, ResourceIDHash, ResourceIDEq> container{};

      [[nodiscard]] const std::filesystem::path& getDirectory() const noexcept {
        return directory;
      }

      [[nodiscard]] bool isBuiltInResource(const ResourceID& id) const noexcept {
        const auto it = container.find(id);
        if (it == container.end()) {
          return false;
        }
        return it->second.sourceType == SourceType::eBuiltIn;
      }

      ResourceID makeResourceContext(const ResourceID& builtInId = {}) {
        const auto& id = builtInId.IsValid()? builtInId : ResourceID(nanoid::generate(12));
        auto [it, inserted] = container.try_emplace(id, ResourceContext{});
        auto& slot = it->second;
        slot.lastImportTime = std::chrono::file_clock::now();
        return id;
      }

     [[nodiscard]] const ResourceID* bindResourceToContext(std::unique_ptr<ResourceBase>& resource, const ResourceContextCreateInfo& info) {
        auto& ctx = *getResourceContext(resource->iD);
        ctx.name = info.name;
        ctx.sourceType = info.sourceType;
        ctx.category = info.category;
        ctx.sourcePath = info.sourcePath;
        ctx.dataPtr = std::move(resource);
        ctx.lastImportTime = std::chrono::file_clock::now();
        ctx.lastWriteTime = info.lastWriteTimeFromMeta;
        ctx.attachments.reserve(info.attachments.size());

        if (!info.attachments.empty()) {
          ctx.attachments.reserve(info.attachments.size());
          for (const auto& attachment : info.attachments) {
            ctx.attachments.emplace_back(attachment);
          }
        }

        WriteResourceContextMetaData(ctx);
        return &ctx.dataPtr->iD;
      }

      static void clearAllGpuResources(ResourceManagerBase& manager) {
          for (const auto &ctx : manager.container | std::views::values) {
            if (!ctx.dataPtr || !ctx.dataPtr->hasGpuResources()) continue;
            ctx.dataPtr->releaseGpuResources();
          }
        }
  };

  void ReadResourceContextMetaData(const std::filesystem::path& rootDirectory,const std::span<const std::string_view>& sourceExtensions, ResourceManagerBase* manager) {
    namespace fs = std::filesystem;
    if (!fs::is_directory(rootDirectory)) {
      debug::log(debug::LogLevel::eWarning,
                 "Root directory not found, skipping metadata load: {}",
                 rootDirectory.string());
      return;
    }

    if (!manager) {
      debug::log(debug::LogLevel::eWarning, "No handler provided for ReadResourceContextMetaData");
      return;
    }

    std::unordered_set<fs::path> sourceFilePaths;
    std::unordered_set<fs::path> sourceMetaFilePaths;
    const std::string meta = ".meta";
    for (const auto& entry : fs::recursive_directory_iterator(rootDirectory)) {
      const auto& path = entry.path();
      const std::string ext = path.extension().string();

      if (ext == meta ||(ext.size() == meta.size() &&
             std::ranges::equal(ext, meta, [](const char a, const char b) {
                 return std::tolower(a) == std::tolower(b);
             }))) {
        sourceMetaFilePaths.insert(path);

      }
      else {
        for (const auto& allowed : sourceExtensions) {
          if (ext == allowed ||
              (ext.size() == allowed.size() &&
               std::ranges::equal(ext, allowed, [](const char a, const char b) {
                   return std::tolower(a) == std::tolower(b);
               }))) {
            sourceFilePaths.insert(path);
            break;
               }
        }
      }
    }

    for (const auto& entry : sourceMetaFilePaths) {
      try {
        std::ifstream file(entry);
        if (!file.is_open()) {
          debug::log(debug::LogLevel::eWarning,
                     "Failed to open metadata file: {}", entry.string());
          continue;
        }

        nlohmann::json doc = nlohmann::json::parse(file);
        const auto& ctx = doc.value(META_TAG_RESOURCE_CONTEXT, nlohmann::json{});

        if (!ctx.is_object()) {
          debug::log(debug::LogLevel::eWarning,
                     "Invalid metadata format: {}", entry.string());
          continue;
        }

        const std::string name          = ctx.value("name", "");
        const std::string idStr         = ctx.value("id", "");
        const std::string sourcePathStr = ctx.value("sourcePath", "");
        const std::string category      = ctx.value("category", "Miscellaneous");
        const std::string lastWriteTime = ctx.value("lastWriteTime", "");   // ← Read from meta

        if (name.empty() || idStr.empty() || sourcePathStr.empty()) {
          debug::log(debug::LogLevel::eWarning,
                     "Metadata missing required fields: {}", entry.string());
          continue;
        }
        ResourceContextCreateInfo info{};
        info.setSourcePath(sourcePathStr)
            .setName(name)
            .setID(ResourceID{idStr})
            .setCategory(category)
            .setSourceType(SourceType::ePortIn)
            .setLastWriteTime(lastWriteTime);

        if (ctx.contains("attachments") && ctx["attachments"].is_object()) {
          const auto& attachmentsJson = ctx["attachments"];
          info.attachments.reserve(attachmentsJson.size());

          for (const auto& [key, value] : attachmentsJson.items()) {
            if (value.is_string()) {
              std::string attName = key;                    // key is the tag/name
              std::string attPathStr = value.get<std::string>();

              if (!attName.empty() && !attPathStr.empty()) {
                info.attachments.emplace_back(
                    std::move(attName),
                    std::filesystem::path(std::move(attPathStr))
                );
              }
            }
          }
        }

        ResourceID resourceID{idStr};
        fs::path sourcePath = sourcePathStr;

        manager->makeResource(info);

        if (sourceFilePaths.contains(sourcePath)) {
          sourceFilePaths.erase(sourcePath);
        }
      }
      catch (const std::exception& e) {
        debug::log(debug::LogLevel::eError,"Failed to load metadata {} : {}",entry.string(), e.what());
      }
    }

    if (sourceFilePaths.empty()) return;

    for (const auto& source : sourceFilePaths) {
      std::string name = source.stem().string();
      ResourceContextCreateInfo info{};
      info.setSourcePath(source)
          .setName(name)
          .setID(ResourceID{})
          .setCategory("Miscellaneous")
          .setSourceType(SourceType::ePortIn)
          .setLastWriteTime("");

      manager->makeResource(info);
    }
  }

  void WriteResourceContextMetaData(const ResourceContext& context) {
    if (context.sourceType == SourceType::eBuiltIn) {
      return;
    }

    if (context.name.empty() || context.sourcePath.empty()) {
      debug::log(debug::LogLevel::eWarning,"WriteResourceContextMeta : name or sourcePath is empty");
      return;
    }

    const std::string filename = context.name + ".ufox.meta";
    const auto metaPath = context.sourcePath.parent_path() / filename;

    // Get source file last write time as ISO 8601 string
    std::string lastWriteTimeStr;
    try {
      if (std::filesystem::exists(context.sourcePath)) {
        auto fileTime = std::filesystem::last_write_time(context.sourcePath);
        auto sysTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
        auto timeT = std::chrono::system_clock::to_time_t(sysTime);

        std::tm utcTime{};
#if defined(_WIN32)
        gmtime_s(&utcTime, &timeT);
#else
        gmtime_r(&timeT, &utcTime);
#endif
        std::ostringstream oss;
        oss << std::put_time(&utcTime, "%Y-%m-%dT%H:%M:%SZ");
        lastWriteTimeStr = oss.str();
      }
    }
    catch (...) {
      debug::log(debug::LogLevel::eWarning, "WriteResourceContextMeta : Failed to get source file last write time");
    }

    nlohmann::json doc = {
      {
        META_TAG_RESOURCE_CONTEXT, {
              { "name",          context.name },
              { "id",            context.dataPtr ? context.dataPtr->iD.view() : "" },
              { "sourcePath",    context.sourcePath.string() },
              { "category",      context.category },
              { "lastImportTime", context.lastImportTime == std::chrono::file_clock::time_point{}? "": TimePointToIso8601(context.lastImportTime) },
              { "lastWriteTime", lastWriteTimeStr }
        }
      }
    };

    if (!context.attachments.empty()) {
      nlohmann::json attObject = nlohmann::json::object();

      for (const auto& [name, path] : context.attachments) {
        attObject[name] = path.string();        // name becomes the key
      }

      doc[META_TAG_RESOURCE_CONTEXT]["attachments"] = attObject;
    }

    std::ofstream file(metaPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
      debug::log(debug::LogLevel::eError,"WriteResourceContextMeta : Failed to open metadata file for writing: {}", metaPath.string());
      return ;
    }

    try {
      file << std::setw(2) << doc << std::endl;
      file.close();
    }
    catch (const std::exception& e) {
      debug::log(debug::LogLevel::eError,"WriteResourceContextMeta : Exception while writing metadata {} : {}", metaPath.string(), e.what());
    }
  }
}