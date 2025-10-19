
module;

#include <utility>
#include <vector>
#include <optional>
#include <iostream>
#include <string>
#include <format>
#include <vulkan/vulkan_raii.hpp>

#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif
export module ufox_lib;

export namespace ufox {
    namespace debug {
        enum class LogLevel {
            eInfo,
            eWarning,
            eError
        };

        template<typename... Args>
        void log(LogLevel level, const std::string& format_str, Args&&... args) {
            std::string level_str;
            switch (level) {
                case LogLevel::eInfo:    level_str = "INFO"; break;
                case LogLevel::eWarning: level_str = "WARNING"; break;
                case LogLevel::eError:   level_str = "ERROR"; break;
            }
            std::string message = std::vformat(format_str, std::make_format_args(args...));
            std::cout << std::format("[{}] {}\n", level_str, message);
        }
    }

    struct Panel {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        std::vector<Panel*> rows;
        std::vector<Panel*> columns;

        void addRowChild(Panel* child) { rows.push_back(child); }
        void addColumnChild(Panel* child) { columns.push_back(child); }
        void print() const {
            debug::log(debug::LogLevel::eInfo, "Panel: x={}, y={}, width={}, height={}", x, y, width, height);
        }

        // Getters/Setters
        int getX() const { return x; }
        void setX(int val) { x = val; }
        int getY() const { return y; }
        void setY(int val) { y = val; }
        int getWidth() const { return width; }
        void setWidth(int val) { width = val; }
        int getHeight() const { return height; }
        void setHeight(int val) { height = val; }
    };

    namespace windowing {

        struct WindowResource {
#ifdef USE_SDL
            using WindowHandle = SDL_Window;
            using DestroyFunc = decltype(&SDL_DestroyWindow);
#else
            using WindowHandle = GLFWwindow;
            using DestroyFunc = decltype(&glfwDestroyWindow);
#endif

            explicit WindowResource(WindowHandle* wnd)
                : handle{wnd, DestroyFunc{}} {}

            WindowResource(WindowResource&& other) noexcept
                : handle{nullptr, DestroyFunc{}} {
                std::swap(handle, other.handle);
            }
            WindowResource(const WindowResource&) = delete;
            ~WindowResource() noexcept = default;

            std::unique_ptr<WindowHandle, DestroyFunc> handle;


            // Getters
            [[nodiscard]] WindowHandle* getHandle() const { return handle.get(); }
        };





    }


namespace gpu::vulkan {
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
            vk::CommandPoolCreateFlags commandPoolCreateFlags{};

            // Getters/Setters
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
        };

        struct SurfaceResource
        {
            SurfaceResource( vk::raii::Instance const & instance, const windowing::WindowResource& window )
            {
                VkSurfaceKHR _surface;
#ifdef USE_SDL
                if ( !SDL_Vulkan_CreateSurface( window.handle.get(), *instance, nullptr, &_surface )) {
                    throw std::runtime_error( "Failed to create window surface!" );
                }
#else
                if (const VkResult err = glfwCreateWindowSurface( *instance, window.getHandle(), nullptr, &_surface ); err != VK_SUCCESS )
                    throw std::runtime_error( "Failed to create window!" );

                int width = 0, height = 0;
                glfwGetFramebufferSize(window.getHandle(), &width, &height);
                extent = vk::Extent2D{static_cast<uint32_t> (width), static_cast<uint32_t> (height)};
#endif


                surface.emplace(instance, _surface);
            }

            vk::Extent2D                                extent;
            std::optional<vk::raii::SurfaceKHR>         surface{};
        };

        struct SwapchainResource {
            vk::Format                                  colorFormat;
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

            void ContinueNextFrame() {
                currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
            }
        };

        struct QueueFamilyIndices {
            uint32_t                                    graphicsFamily{0};
            uint32_t                                    presentFamily{0};
        };

    }

    struct GPUResources {
        std::optional<vk::raii::Context>                context{};
        std::optional<vk::raii::Instance>               instance{};

        std::optional<vk::raii::PhysicalDevice>         physicalDevice{};
        std::optional<vk::raii::Device>                 device{};
        std::optional<gpu::vulkan::QueueFamilyIndices>  queueFamilyIndices{};

        std::optional<vk::raii::CommandPool>            commandPool{};


        std::optional<vk::raii::Queue>                  graphicsQueue{};
        std::optional<vk::raii::Queue>                  presentQueue{};

    };
}