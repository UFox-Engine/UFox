//
// Created by b-boy on 25.02.2026.
//

module;

#include "GLFW/glfw3.h"

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
    return glm::ortho(0.0f, width, 0.0f, height, -1.0f, 1.0f);
  }

  template<typename Callback>
  class EventList {
  public:
    using Entry = std::pair<Callback, void*>;

    void add(Callback callback, void* user) {
      handlers.emplace_back(callback, user);
    }

    void remove(void* user) {
      std::erase_if(handlers, [user](const Entry& entry) {
        return entry.second == user;
      });
    }

    template<typename... Args>
    void execute(Args&&... args) const {
      for (const auto& [callback, user] : handlers) {
        if (callback) {
          callback(std::forward<Args>(args)..., user);
        }
      }
    }

    [[nodiscard]] bool empty() const noexcept {
      return handlers.empty();
    }

  private:
    std::vector<Entry> handlers{};
  };


  class UFoxWindow {
  public:

    ~UFoxWindow() {
      if (gpuResource.device) {
        gpuResource.device->waitIdle();
      }
      pauseRendering = true;
      if (windowResource->swapchainResource) {
        windowResource->swapchainResource->Clear();
      }

      running = false;
#ifdef USE_SDL
      SDL_RemoveEventWatch(FramebufferResizeCallback, this);
#endif
    }

    gpu::GraphicDeviceCreateInfo                          gpuCreateInfo{};
    gpu::GPUResources                                     gpuResource{};
    std::optional<gpu::WindowResource>                    windowResource{};
    std::optional<gpu::FrameResource>                     frameResource{};
    std::optional<gpu::DepthImageResource>                depthResource{};


    [[nodiscard]] constexpr bool isRunning() const { return running; }

    template<EventType Type>
    void registerCallbackEvent(EventTraits<Type>::Handler callback, void* user) {
      getEventList<Type>().add(callback, user);
    }

    void unregisterCallbackEvent(const EventType type, void* user) {
      switch (type) {
      case EventType::eResize:
        resizeEventHandlers.remove(user);
        break;
      case EventType::eSystemInit:
        systemInitEvents.remove(user);
        break;
      case EventType::ePostSystemInit:
        postSystemInitEvents.remove(user);
        break;
      case EventType::eStart:
        startEvents.remove(user);
        break;
      case EventType::eGainsFocus:
        gainsFocusEvents.remove(user);
        break;
      case EventType::ePreDraw:
        preDrawEvents.remove(user);
        break;
      case EventType::eUpdateBuffer:
        updateBufferEvents.remove(user);
        break;
      case EventType::eRenderPass:
        renderPassEvents.remove(user);
        break;
      }
    }

    template<EventType Type>
    auto& getEventList() {
      if constexpr (Type == EventType::eResize) {
        return resizeEventHandlers;
      }
      else if constexpr (Type == EventType::eSystemInit) {
        return systemInitEvents;
      }
      else if constexpr (Type == EventType::ePostSystemInit) {
        return postSystemInitEvents;
      }
      else if constexpr (Type == EventType::eStart) {
        return startEvents;
      }
      else if constexpr (Type == EventType::eGainsFocus) {
        return gainsFocusEvents;
      }
      else if constexpr (Type == EventType::ePreDraw) {
        return preDrawEvents;
      }
      else if constexpr (Type == EventType::eUpdateBuffer) {
        return updateBufferEvents;
      }
      else if constexpr (Type == EventType::eRenderPass) {
        return renderPassEvents;
      }
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

    void requestPreDraw() noexcept {
      enabledPreDraw = true;
    }

    void run() {
#ifdef USE_SDL
      running = true;
      SDL_Event event;

      float width, height = 0;
      windowResource->getExtent(width, height);

      executeSystemInit();
      executePostSystemInit(width,height);

      while (running) {
        executeStart();
        sdlPollEvents(event, running);
        executePreDraw();
        drawFrame();
      }
#else
      executeSystemInit();
      executeResourceInitEvents();

      while (!glfwWindowShouldClose(windowResource->getHandle())) {
        executeStart();
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

      resizeEventHandlers.execute(fw, fh);

      drawFrame();
    }

    [[nodiscard]] vk::DescriptorBufferInfo makeScreenViewProjectionBufferInfo() const {
      if (!screenViewProjectionBuffer.has_value() ||!screenViewProjectionBuffer->data.has_value()) {
        throw std::runtime_error("MakeScreenViewProjectionBufferInfo : screen view projection buffer is not initialized");
      }

      vk::DescriptorBufferInfo bufferInfo{};
      bufferInfo
        .setBuffer(*screenViewProjectionBuffer.value().data)
        .setOffset(0)
        .setRange(MAT4_BUFFER_SIZE);

      return bufferInfo;

    }

  private:
    EventList<SystemInitEventHandler>                             systemInitEvents{};
    EventList<PostSystemInitEventHandler>                         postSystemInitEvents{};
    EventList<ResizeEventHandler>                                 resizeEventHandlers{};
    EventList<StartEventHandler>                                  startEvents{};
    EventList<GainsFocusEventHandler>                             gainsFocusEvents{};
    EventList<PreDrawEventHandler>                                preDrawEvents{};
    EventList<UpdateBufferEventHandler>                           updateBufferEvents{};
    EventList<RenderPassEventHandler>                             renderPassEvents{};

    bool                                                          running {false};
    bool                                                          framebufferResized {false};
    bool                                                          pauseRendering {false};
    bool                                                          startFrameExecuted {false};
    bool                                                          enabledPreDraw = false;

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

    void executeSystemInit() const {
      if (systemInitEvents.empty()) return;
      systemInitEvents.execute();
    }

    void executePostSystemInit(const float& width, const float& height) {
      initGPUBufferResource();
      if (postSystemInitEvents.empty()) return;
      postSystemInitEvents.execute(width, height);
    }

    void executeStart() {
      if (startFrameExecuted) return;
      if (!startEvents.empty())
        startEvents.execute();
      startFrameExecuted = true;
    }

    void executeGainsFocus() const {
      if (gainsFocusEvents.empty()) return;
      gainsFocusEvents.execute();
    }

    void executePreDraw() {
      if (preDrawEvents.empty() || !enabledPreDraw) return;
      gpuResource.device->waitIdle();
      preDrawEvents.execute();
      enabledPreDraw = false;
    }

    void executeUpdateBuffer(const uint32_t& imageIndex) const {
      if (updateBufferEvents.empty()) return;
      updateBufferEvents.execute(imageIndex);
    }

    void executeRenderPass(const vk::raii::CommandBuffer& cmb, const uint32_t& imageIndex, const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& depthAttachment) {
      if (renderPassEvents.empty()) return;
      renderPassEvents.execute(cmb, imageIndex, *windowResource, colorAttachment, depthAttachment);
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
      cmb.begin({});

      int height, width;
      if (windowResource->getExtent(width, height)) { cmb.end(); return; }
      const vk::Rect2D rect{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

      vk::ImageSubresourceRange colorRange{};
      colorRange.aspectMask     = vk::ImageAspectFlagBits::eColor;
      colorRange.baseMipLevel   = 0;
      colorRange.levelCount     = 1;
      colorRange.baseArrayLayer = 0;
      colorRange.layerCount     = 1;

      vk::ImageSubresourceRange depthStencilRange{};
      depthStencilRange.aspectMask     = vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil; // FIXED
      depthStencilRange.baseMipLevel   = 0;
      depthStencilRange.levelCount     = 1;
      depthStencilRange.baseArrayLayer = 0;
      depthStencilRange.layerCount     = 1;

      gpu::TransitionImageLayout(cmb, windowResource->swapchainResource->getCurrentImage(), windowResource->swapchainResource->colorFormat, colorRange, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
      gpu::TransitionImageLayout(cmb, depthResource->image.data.value(), depthResource->format, depthStencilRange, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);

      vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 0.0f);
      vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f,0);
      vk::RenderingAttachmentInfo colorAttachment{};
      colorAttachment.setImageView(windowResource->swapchainResource->getCurrentImageView())
                     .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                     .setLoadOp(vk::AttachmentLoadOp::eClear)
                     .setStoreOp(vk::AttachmentStoreOp::eStore)
                     .setClearValue(clearColor);

      vk::RenderingAttachmentInfo stencilAttachment{};
      stencilAttachment.setImageView(*depthResource->image.view)
                       .setImageLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal)
                       .setLoadOp(vk::AttachmentLoadOp::eClear)
                       .setStoreOp(vk::AttachmentStoreOp::eDontCare)
                       .setClearValue(clearDepth);


      executeRenderPass(cmb, imageIndex, colorAttachment, stencilAttachment);

      gpu::TransitionImageLayout(cmb, windowResource->swapchainResource->getCurrentImage(), windowResource->swapchainResource->colorFormat, colorRange, vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR);

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

      executeUpdateBuffer(imageIndex);

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
        case SDL_EVENT_WINDOW_FOCUS_GAINED:{

          gpuResource.device->waitIdle();
          executeGainsFocus();
          requestPreDraw();

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

  void GetFileWriteTime(const std::filesystem::path& path, std::string& outTime) {
    try {
      if (std::filesystem::exists(path)) {
        auto fileTime = std::filesystem::last_write_time(path);
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
        outTime = oss.str();
      }
    }
    catch (...) {
      debug::log(debug::LogLevel::eWarning, "WriteResourceContextMeta : Failed to get source file last write time");
    }
  }

  void MakeResourceContextCreateInfo(const ResourceContext& ctx, ResourceContextCreateInfo& outInfo, const std::string_view lastWriteTime = "") {
    outInfo.setSourcePath(ctx.sourcePath)
      .setName(ctx.name)
      .setID(ctx.dataPtr ? ctx.dataPtr->iD : ResourceID{})
      .setCategory(ctx.category)
      .setSourceType(ctx.sourceType)
      .setLastWriteTime(!lastWriteTime.empty() ? lastWriteTime : ctx.lastWriteTime)
      .setAttachments(ctx.attachments)
      .setOverwrite(!lastWriteTime.empty());
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
      state.updateHash();
      outState = state;
    } catch (const std::exception& e) {
      debug::log(debug::LogLevel::eError,"MakeDirectoryContext ({}) : {}", dirPath.string(), e.what());
    }
  }

  [[nodiscard]]constexpr bool DirectoryContextChanged(const DirectoryContext& oldState,const DirectoryContext& newState) noexcept {
    if (!oldState.isValid() || !newState.isValid()) return true;

    return oldState.stateHash != newState.stateHash;
  }

  bool WriteDirectoryContextMetaData(const DirectoryContext& state) {
    if (!state.isValid()) {
      debug::log(debug::LogLevel::eWarning, "WriteDirectoryMeta: invalid DirectoryState");
      return false;
    }

    const std::string metaFilename = state.directoryPath.filename().string() + ".meta";
    const auto metaPath = state.directoryPath.parent_path() / metaFilename;

    nlohmann::json doc = {
      {"directory-context", {
        {"path",           state.directoryPath.string()},
        {"lastWriteTime",  state.lastWriteTimeIso},
        {"totalSizeBytes", state.totalSizeBytes},
        {"stateHash",      state.stateHash}
      }}
    };

    std::ofstream file(metaPath, std::ios::out | std::ios::trunc);
    if (!file.is_open()) {
      debug::log(debug::LogLevel::eError, "WriteDirectoryMeta ({}) : Failed to open directory meta", metaPath.string());
      return false;
    }

    try {
      file << std::setw(2) << doc << std::endl;
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

    const std::string metaFilename = dirPath.filename().string() + ".meta";
    const auto metaPath = dirPath.parent_path() / metaFilename;

    if (!std::filesystem::exists(metaPath)) {
      return state;
    }

    try {
      std::ifstream file(metaPath);
      nlohmann::json j = nlohmann::json::parse(file);

      const auto& m = j.at("directory-context");

      state.lastWriteTimeIso = m.value("lastWriteTime", "");
      state.totalSizeBytes   = m.value<uintmax_t>("totalSizeBytes", 0);
      state.stateHash = m.value<size_t>("stateHash", 0);

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
  void RemoveMissingSourceMetaData(const std::filesystem::path& metaPath,const nlohmann::json& ctx,ResourceManagerBase* manager);

  class ResourceManagerBase {
    public:
    virtual ~ResourceManagerBase() = default;
    explicit ResourceManagerBase(UFoxWindow& _window, const std::string_view& _directory,
                                 const std::span<const std::string_view> &extension) : window(&_window), directory(_directory), sourceExtensions(extension), gpuResources(&gpu) {


        EnsureDirectoryExists(directory);
        DirectoryContext directoryContext{};
        MakeDirectoryContext(directory, directoryContext);
        WriteDirectoryContextMetaData(directoryContext);

    }

    virtual void onSystemInit() = 0;
    virtual void onPostSystemInit(const float& width, const float& height) = 0;
    virtual void onMakeResource(ResourceContextCreateInfo& info) = 0;
    virtual void onGainsFocus() = 0;
    virtual void onPreDraw() = 0;

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

    [[nodiscard]] const ResourceContext* getResourceContext(const ResourceID& id) const noexcept {
      const auto it = container.find(id);
      return it != container.end() ? &it->second : nullptr;
    }


    bool removeResourceContext(const ResourceID& id) {
      const auto it = container.find(id);
      if (it == container.end()) {
        return false;
      }

      if (it->second.dataPtr && it->second.dataPtr->hasGpuResources()) {
        it->second.dataPtr->releaseGpuResources();
      }

      container.erase(it);
      return true;
    }

    protected:
      UFoxWindow* window = nullptr;
      const std::filesystem::path directory{};
      std::span<const std::string_view> sourceExtensions{};
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

      bool makeResourceContext(ResourceID& id, const bool& overwrite) {
        if (!id.IsValid()) {
          do {
            id = ResourceID(nanoid::generate(12));
          } while (container.contains(id));
        }

        auto [it, inserted] = container.try_emplace(id, ResourceContext{});

        if (!inserted && !overwrite) {
          return false;
        }

        auto& slot = it->second;
        slot.lastImportTime = std::chrono::file_clock::now();

        return true;
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
        ctx.attachments = info.attachments;

        WriteResourceContextMetaData(ctx);
        return &ctx.dataPtr->iD;
      }

      void refreshResource() {
        const DirectoryContext oldContext = ReadDirectoryContextMetaData(directory);
        DirectoryContext newContext{};
        MakeDirectoryContext(directory, newContext);
        if (DirectoryContextChanged(oldContext, newContext)) {
          WriteDirectoryContextMetaData(newContext);
          ReadResourceContextMetaData(directory, sourceExtensions, this);
        }
      }

      static void clearAllGpuResources(ResourceManagerBase& manager) {
          for (const auto &ctx : manager.container | std::views::values) {
            if (!ctx.dataPtr || !ctx.dataPtr->hasGpuResources()) continue;
            ctx.dataPtr->releaseGpuResources();
          }
        }
  };

  void RemoveMissingSourceMetaData(const std::filesystem::path& metaPath, const nlohmann::json& ctx, ResourceManagerBase* manager) {
    const std::string idStr = ctx.value("id", "");
    const std::string sourcePathStr = ctx.value("sourcePath", "");

    if (!idStr.empty() && manager) {
      manager->removeResourceContext(ResourceID{idStr});
    }

    if (ctx.contains("attachments") && ctx["attachments"].is_array()) {
      for (const auto& attachmentJson : ctx["attachments"]) {
        if (!attachmentJson.is_object()) {
          continue;
        }

        const std::string attachmentPathStr = attachmentJson.value("path", "");
        if (attachmentPathStr.empty()) {
          continue;
        }

        std::error_code ec;
        std::filesystem::remove(std::filesystem::path{attachmentPathStr}, ec);

        if (ec) {
          debug::log(
            debug::LogLevel::eWarning,
            "Failed to remove stale attachment: {} error: {}",
            attachmentPathStr,
            ec.message()
          );
        }
      }
    }

    std::error_code ec;
    std::filesystem::remove(metaPath, ec);

    if (ec) {
      debug::log(
        debug::LogLevel::eWarning,
        "Failed to remove stale metadata: {} error: {}",
        metaPath.string(),
        ec.message()
      );
    }
  }

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
        nlohmann::json doc;

        {
          std::ifstream file(entry);
          if (!file.is_open()) {
            debug::log(debug::LogLevel::eWarning,
                       "Failed to open metadata file: {}", entry.string());
            continue;
          }

          doc = nlohmann::json::parse(file);
        }

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
        const std::string lastWriteTime = ctx.value("lastWriteTime", "");

        if (name.empty() || idStr.empty() || sourcePathStr.empty()) {
          debug::log(debug::LogLevel::eWarning,
                     "Metadata missing required fields: {}", entry.string());
          continue;
        }

        fs::path sourcePath = sourcePathStr;

        if (!fs::exists(sourcePath)) {
          RemoveMissingSourceMetaData(entry, ctx, manager);
          continue;
        }

        ResourceContextCreateInfo info{};
        info.setSourcePath(sourcePathStr)
            .setName(name)
            .setID(ResourceID{idStr})
            .setCategory(category)
            .setSourceType(SourceType::ePortIn)
            .setLastWriteTime(lastWriteTime);

        if (ctx.contains("attachments") && ctx["attachments"].is_array()) {
          const auto& attachmentsJson = ctx["attachments"];
          info.attachments.reserve(attachmentsJson.size());

          for (const auto& attachmentJson : attachmentsJson) {
            if (!attachmentJson.is_object()) {
              continue;
            }

            const std::string attName = attachmentJson.value("name", "");
            const std::string attPathStr = attachmentJson.value("path", "");

            if (!attName.empty() && !attPathStr.empty()) {
              info.attachments.emplace_back(
                attName,
                std::filesystem::path(attPathStr)
              );
            }
          }
        }

        ResourceID resourceID{idStr};

        manager->onMakeResource(info);

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

      manager->onMakeResource(info);
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

    const std::string filename = context.name + ".meta";
    const auto metaPath = context.sourcePath.parent_path() / filename;

    // Get source file last write time as ISO 8601 string
    std::string lastWriteTimeStr;

    GetFileWriteTime(context.sourcePath, lastWriteTimeStr);

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
      nlohmann::json attArray = nlohmann::json::array();

      for (const auto& [name, path] : context.attachments) {
        attArray.push_back({
          { "name", name },
          { "path", path.string() }
        });
      }

      doc[META_TAG_RESOURCE_CONTEXT]["attachments"] = attArray;
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

  [[nodiscard]] ResourceID ResolveResourceID(const ResourceContextCreateInfo& info) {
    return info.sourceType == SourceType::eBuiltIn ? info.id : ResourceID{info.sourcePath.filename().string()};
  }

}