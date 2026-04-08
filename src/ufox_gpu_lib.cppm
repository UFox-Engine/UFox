module;

#include <optional>
#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <glm/glm.hpp>

#ifdef USE_SDL

#include <SDL3/SDL.h>

#include <SDL3/SDL_vulkan.h>

#include <SDL3_image/SDL_image.h>

#else

#include <GLFW/glfw3.h>

#endif

export module ufox_gpu_lib;
import ufox_lib;

export namespace ufox::gpu {
  inline vk::Format STANDARD_COLOR_TEXTURE_FORMAT = vk::Format::eR8G8B8A8Unorm;
  inline vk::ImageSubresourceRange DEFAULT_COLOR_SUBRESOURCE_RANGE{
    vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

  struct PhysicalDeviceRequirements {
    static constexpr uint32_t MINIMUM_API_VERSION = vk::ApiVersion14;
    static constexpr uint32_t DISCRETE_GPU_SCORE_BONUS = 1000;
  };

  struct GraphicDeviceCreateInfo {
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

  struct QueueFamilyIndices {
    uint32_t                                    graphicsFamily{0};
    uint32_t                                    presentFamily{0};
  };

  struct GPUResources {
    std::optional<vk::raii::Context>            context{};
    std::optional<vk::raii::Instance>           instance{};
    std::optional<vk::raii::PhysicalDevice>     physicalDevice{};
    std::optional<vk::raii::Device>             device{};
    std::optional<QueueFamilyIndices>           queueFamilyIndices{};
    std::optional<vk::raii::CommandPool>        commandPool{};
    std::optional<vk::raii::Queue>              graphicsQueue{};
    std::optional<vk::raii::Queue>              presentQueue{};
  };

  struct SwapchainResource {
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

  struct FrameResource {
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

  struct Buffer {
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

  struct Image {
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

  struct DepthImageResource {
    Image           image{};
    vk::Format      format{};
    vk::Extent3D    extent{1,1,1};
  };

  struct WindowResource {
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
    std::optional<SwapchainResource>                    swapchainResource{};

    // Getters
    [[nodiscard]] WindowHandle* getHandle() const { return handle.get(); }

    template<mathf::Arithmetic T>
    bool getExtent(T& width, T& height) const {
      int _width, _height;
  #ifdef USE_SDL
      SDL_GetWindowSize(handle.get(), &_width, &_height);
  #else
      glfwGetFramebufferSize(handle.get(), &_width, &_height);
  #endif
      width = static_cast<T>(_width);
      height = static_cast<T>(_height);

      return height < 1 || width < 1;
    }
  };

WindowResource CreateWindow(const vk::raii::Instance& instance, std::string const & windowName, vk::Extent2D const & extent ) {
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


        Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
        SDL_Window* window = SDL_CreateWindow(windowName.c_str(), static_cast<int>(extent.width), static_cast<int>(extent.height), flags);

        if (!window) {
            throw std::runtime_error("Failed to create SDL window");
        }


        debug::log(debug::LogLevel::eInfo, "Created SDL window: {} ({}x{})", windowName, static_cast<int>(extent.width), static_cast<int>(extent.height));

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

