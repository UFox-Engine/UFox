module;

#include <optional>
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <ufox_engine_export.h>

export module ufox_gpu_lib;
import ufox_mathf;


namespace ufox::gpu {
  export struct UFOX_ENGINE_EXPORT PhysicalDeviceRequirements {
    static constexpr uint32_t MINIMUM_API_VERSION = vk::ApiVersion14;
    static constexpr uint32_t DISCRETE_GPU_SCORE_BONUS = 1000;
  };

  export struct UFOX_ENGINE_EXPORT GraphicDeviceCreateInfo {
    vk::ApplicationInfo appInfo{};
    std::vector<std::string> instanceExtensions{};
    std::vector<std::string> deviceExtensions{};

    bool enableDynamicRendering{true};
    bool enableExtendedDynamicState{true};
    bool enableSynchronization2{true};
    bool enableShaderDrawParameters{true};

    vk::CommandPoolCreateFlags commandPoolCreateFlags{};

    [[nodiscard]] vk::ApplicationInfo getAppInfo() const { return appInfo; }

    void setAppInfo(const vk::ApplicationInfo& info) { appInfo = info; }

    [[nodiscard]] std::vector<std::string> getInstanceExtensions() const { return instanceExtensions; }

    void setInstanceExtensions(const std::vector<std::string>& exts) { instanceExtensions = exts; }

    [[nodiscard]] std::vector<std::string> getDeviceExtensions() const { return deviceExtensions; }

    void setDeviceExtensions(const std::vector<std::string>& exts) { deviceExtensions = exts; }

    [[nodiscard]] bool getEnableDynamicRendering() const { return enableDynamicRendering; }

    void setEnableDynamicRendering(bool enable) { enableDynamicRendering = enable; }

    [[nodiscard]] bool getEnableExtendedDynamicState() const { return enableExtendedDynamicState; }

    void setEnableExtendedDynamicState(bool enable) { enableExtendedDynamicState = enable; }

    [[nodiscard]] bool getEnableSynchronization2() const { return enableSynchronization2; }

    void setEnableSynchronization2(bool enable) { enableSynchronization2 = enable; }

    [[nodiscard]] vk::CommandPoolCreateFlags getCommandPoolCreateFlags() const { return commandPoolCreateFlags; }

    void setCommandPoolCreateFlags(vk::CommandPoolCreateFlags flags) { commandPoolCreateFlags = flags; }

    [[nodiscard]] bool getEnableShaderDrawParameters() const { return enableShaderDrawParameters; }
    void setEnableShaderDrawParameters(bool enable) { enableShaderDrawParameters = enable; }
  };

  export struct UFOX_ENGINE_EXPORT QueueFamilyIndices {
    uint32_t                                    graphicsFamily{0};
    uint32_t                                    presentFamily{0};
  };

  export struct UFOX_ENGINE_EXPORT GPUResources {
    std::optional<vk::raii::Context>            context{};
    std::optional<vk::raii::Instance>           instance{};
    std::optional<vk::raii::PhysicalDevice>     physicalDevice{};
    std::optional<vk::raii::Device>             device{};
    std::optional<QueueFamilyIndices>           queueFamilyIndices{};
    std::optional<vk::raii::CommandPool>        commandPool{};
    std::optional<vk::raii::Queue>              graphicsQueue{};
    std::optional<vk::raii::Queue>              presentQueue{};
    uint32_t                                    maxStorageBufferRange{};
  };

  export struct UFOX_ENGINE_EXPORT FrameResource {
    static constexpr int                        MAX_FRAMES_IN_FLIGHT = 2;
    std::vector<vk::raii::CommandBuffer>        commandBuffers{};
    std::vector<vk::raii::Semaphore>            presentCompleteSemaphores{};
    std::vector<vk::raii::Fence>                drawFences{};
    uint32_t                                    currentFrameIndex{0};

    const vk::raii::Fence& getCurrentDrawFence()  {
      return drawFences[currentFrameIndex];
    }

    const vk::raii::Semaphore& getCurrentPresentCompleteSemaphore() {
      return presentCompleteSemaphores[currentFrameIndex];
    }

    const vk::raii::CommandBuffer& getCurrentCommandBuffer()  {
      return commandBuffers[currentFrameIndex];
    }

    void continueNextFrame() {
      currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }
  };

  export struct UFOX_ENGINE_EXPORT Buffer {
    std::optional<vk::raii::DeviceMemory>       memory{};
    std::optional<vk::raii::Buffer>             data{};

    [[nodiscard]] bool isDataEmpty() const {
      return !data.has_value();
    }

    void clear() {
      memory.reset();
      data.reset();
    }
  };

  export struct UFOX_ENGINE_EXPORT Image {
    std::optional<vk::raii::Image>          data{};
    std::optional<vk::raii::DeviceMemory>   memory{};
    std::optional<vk::raii::ImageView>      view{};

    void clear() noexcept{
      data.reset();
      memory.reset();
      view.reset();
    }

    [[nodiscard]] bool hasGpuResources() const noexcept {
      return data.has_value() && view.has_value() && memory.has_value();
    }
  };

  export struct UFOX_ENGINE_EXPORT DepthImageResource {
    Image           image{};
    vk::Format      format{};
    vk::Extent3D    extent{1,1,1};
  };

  export struct UFOX_ENGINE_EXPORT SwapchainResource {
    vk::Format                                  colorFormat{};
    std::optional<vk::raii::SwapchainKHR>       swapChain{};
    std::vector<vk::Image>                      images;
    std::vector<vk::raii::ImageView>            imageViews;
    vk::Extent2D                                extent{0,0};
    std::vector<vk::raii::Semaphore>            renderFinishedSemaphores{};
    uint32_t                                    currentImageIndex{0};

    [[nodiscard]] const vk::Image& getCurrentImage() const {
      return images[currentImageIndex];
    }

    [[nodiscard]] vk::ImageView getCurrentImageView() const {
      return *imageViews[currentImageIndex]; // Dereference to get the underlying VkImageView
    }

    const vk::raii::Semaphore& getCurrentRenderFinishedSemaphore()  {
      return renderFinishedSemaphores[currentImageIndex];
    }

    [[nodiscard]] uint32_t getImageCount() const {
      return static_cast<uint32_t>(images.size());
    }

    void Clear() {
      imageViews.clear();
      renderFinishedSemaphores.clear();
      swapChain.reset();
    }
  };
}

