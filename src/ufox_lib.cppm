module;

#include "glm/ext/matrix_transform.hpp"

#include <chrono>
#include <utility>

#include <vector>

#include <optional>

#include <iostream>

#include <string>

#include <format>

#include <functional>
#include <ranges>

#include <vulkan/vulkan_raii.hpp>

#include <glm/glm.hpp>



#ifdef USE_SDL

#include <SDL3/SDL.h>

#include <SDL3/SDL_vulkan.h>

#else

#include <GLFW/glfw3.h>

#endif

export module ufox_lib;



export namespace ufox {
    namespace utilities {
        template <typename TargetType, typename SourceType>
        constexpr  TargetType CheckedCast( SourceType value ) {
            static_assert( sizeof( TargetType ) <= sizeof( SourceType ), "No need to cast from smaller to larger type!" );
            static_assert( std::numeric_limits<SourceType>::is_integer, "Only integer types supported!" );
            static_assert( !std::numeric_limits<SourceType>::is_signed, "Only unsigned types supported!" );
            static_assert( std::numeric_limits<TargetType>::is_integer, "Only integer types supported!" );
            static_assert( !std::numeric_limits<TargetType>::is_signed, "Only unsigned types supported!" );
            assert( value <= ( std::numeric_limits<TargetType>::max )() );
            return static_cast<TargetType>( value );
        }

        constexpr size_t GenerateUniqueID(const std::vector<glm::vec4>& fields) {
            std::hash<float> floatHasher;
            size_t hash = 0;

            auto combineHash = [&hash](size_t value) {
                hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            };

            auto combineFloat = [&combineHash, &floatHasher](float value) {
                float normalized = std::round(value * 1000000.0f) / 1000000.0f;
                combineHash(floatHasher(normalized));
            };

            for (const auto& vec : fields) {
                combineFloat(vec.r);
                combineFloat(vec.g);
                combineFloat(vec.b);
                combineFloat(vec.a);
            }

            return hash;
        }

        // Hash a vector of floats

        constexpr size_t GenerateUniqueID(const std::vector<float>& fields) {
            std::hash<float> floatHasher;
            size_t hash = 0;

            auto combineHash = [&hash](size_t value) {
                hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            };

            auto combineFloat = [&combineHash, &floatHasher](float value) {
                float normalized = std::round(value * 1000000.0f) / 1000000.0f;
                combineHash(floatHasher(normalized));
            };

            for (float value : fields) {
                combineFloat(value);
            }
            return hash;
        }

        constexpr  size_t GenerateUniqueID(std::string_view sv) {
            std::vector<float> chars;
            chars.reserve(sv.size());
            for (char c : sv) {
                chars.push_back(static_cast<unsigned char>(c));
            }
            return GenerateUniqueID(chars);
        }

        constexpr  size_t GenerateUniqueID(const char* str) {
            return GenerateUniqueID(std::string_view(str));
        }

        constexpr size_t GenerateUniqueID(const std::string& str) {
            return GenerateUniqueID(std::string_view(str));
        }

        template <typename T>
        [[nodiscard]] constexpr std::optional<std::size_t> Index_Of(std::span<T> container, const T& value) noexcept {
            auto it = std::ranges::find(container, value);
            if (it == container.end())
                return std::nullopt;

            return static_cast<std::size_t>(std::ranges::distance(container.begin(), it));
        }
    }

    namespace mathf {
        /**
     * @brief High-precision value of π (float).
     *        32-bit float representation of 3.1415926535897932384626433832795.
     */
    constexpr float PI = 3.1415926535897932384626433832795f;

    template<typename T>
    concept Arithmetic = std::is_arithmetic_v<T>;

    /**
     * @brief Rounds a floating-point value to the nearest integer (half-away-from-zero).
     * @param v Input value.
     * @return Nearest integer as long.
     */
    constexpr long  RoundToNearest(const float  v) noexcept { return std::lroundf(v); }
    constexpr long  RoundToNearest(const double v) noexcept { return std::lround(v);  }

    /**
     * @brief Converts float/double to int using proper rounding (half-away-from-zero).
     * @param v Input value.
     * @return Rounded integer.
     */
    constexpr int RoundToInt(const float  v) noexcept { return static_cast<int>(std::lroundf(v)); }
    constexpr int RoundToInt(const double v) noexcept { return static_cast<int>(std::lround(v));  }

    // ──────────────────────────────────────────────────────────────────────
    // Hot-path integer results (pixel-perfect layout)
    // ──────────────────────────────────────────────────────────────────────
    template<typename  T, typename U>
    constexpr float Divide(const T a, const U b) noexcept {
        const auto f_a = static_cast<float>(a);
        const auto f_b = static_cast<float>(b);
        return f_a == 0.0f || f_b == 0.0f ? 0.0f : f_a / f_b;
    }

    /**
     * @brief int × float → int (rounded). zero short-circuits.
     */
    constexpr int MulToInt(const int a, const float b) noexcept
    {
        return a == 0 ? 0 : RoundToInt(static_cast<float>(a) * b);
    }

    /**
     * @brief float × int → int (rounded). zero short-circuits.
     */
    constexpr int MulToInt(const float a, const int b) noexcept
    {
        return b == 0 ? 0 : RoundToInt(a * static_cast<float>(b));
    }

    /**
     * @brief float × float → int (rounded). zero short-circuits.
     *        most common in layout: flex factor × parent size.
     */
    constexpr int MulToInt(const float a, const float b) noexcept
    {
        return a == 0.0f || b == 0.0f ? 0 : RoundToInt(a * b);
    }

    /**
     * @brief float × float → float. Zero short-circuit.
     */
    constexpr float MulToFloat(const float a, const float b) noexcept
    {
        return a == 0.0f || b == 0.0f ? 0.0f : a * b;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Floor / Ceil multiply
    // ──────────────────────────────────────────────────────────────────────

    /**
     * @brief Multiplies two values and floors the result to int.
     *        Returns 0 if either operand is zero.
     */
    constexpr int MulFloor(Arithmetic auto a, Arithmetic auto b) noexcept
    {
        if (a == 0 || b == 0) return 0;
        return static_cast<int>(std::floorf(static_cast<float>(a) * static_cast<float>(b)));
    }

    /**
     * @brief Multiplies two values and ceilings the result to int.
     *        Returns 0 if either operand is zero.
     */
    constexpr int MulCeil(Arithmetic auto a, Arithmetic auto b) noexcept
    {
        if (a == 0 || b == 0) return 0;
        return static_cast<int>(std::ceilf(static_cast<float>(a) * static_cast<float>(b)));
    }

    // ──────────────────────────────────────────────────────────────────────
    // Clamp
    // ──────────────────────────────────────────────────────────────────────

    /**
     * @brief Constexpr clamp: restricts value to [lo, hi].
     *        If lo > hi returns lo (graceful fallback).
     *        Branchless on signed integer types ≤ int size.
     * @tparam T Arithmetic type.
     * @param v  Value to clamp.
     * @param lo Lower bound.
     * @param hi Upper bound.
     * @return   Clamped value.
     */
    template<Arithmetic T>
    constexpr T Clamp(T v, T lo, T hi) noexcept
    {
        if constexpr (std::is_signed_v<T> && sizeof(T) <= sizeof(int))
        {
            const T t = v < lo ? lo : v;
            return t > hi ? hi : t;
        }
        else
        {
            if (v < lo) return lo;
            if (v > hi) return hi;
            return v;
        }
    }

    // ──────────────────────────────────────────────────────────────────────
    // Normalize
    // ──────────────────────────────────────────────────────────────────────

    /**
     * @brief Normalizes a value from [ min, max ] → [0.0, 1.0].
     *        Safe against division-by-zero.
     * @tparam T  Arithmetic input type.
     * @param value  Value to normalize.
     * @param min    Range minimum.
     * @param max    Range maximum.
     * @param clamp  Clamp result to [0.0, 1.0] (default: true).
     * @return       Normalized float.
     */
    template<Arithmetic U, Arithmetic V>
    constexpr float Normalize(float value, U min, V max,const bool clamp = true) noexcept
    {
        const auto f_min = static_cast<float>(min);
        const auto f_max = static_cast<float>(max);
        const float range = f_max - f_min;

        if (range == 0.0f) return value >= f_min ? 1.0f : 0.0f;

        const float result = (value - f_min) / range;
        return clamp ? Clamp(result, 0.0f, 1.0f) : result;
    }

    // ──────────────────────────────────────────────────────────────────────
    // Mix (Lerp)
    // ──────────────────────────────────────────────────────────────────────

    /**
     * @brief Linear interpolation with a float factor (clamped).
     */
    template<Arithmetic T>
    constexpr T Mix(T a, T b, const float t) noexcept
    {
        const float f = Clamp(t, 0.0f, 1.0f);
        if constexpr (std::is_floating_point_v<T>)
            return a + (b - a) * f;
        else
            return static_cast<T>(static_cast<float>(a) * (1.0f - f) + static_cast<float>(b) * f + 0.5f);
    }

    /**
     * @brief Linear interpolation with an arithmetic factor (auto-clamped).
     */
    template<Arithmetic T>
    constexpr T Mix(T a, T b, T t) noexcept
    {
        const float f = Clamp(static_cast<float>(t), 0.0f, 1.0f);
        const float result = static_cast<float>(a) * (1.0f - f) + static_cast<float>(b) * f;
        if constexpr (std::is_integral_v<T>)
            return static_cast<T>(result + 0.5f);
        else
            return static_cast<T>(result);
    }

    // ──────────────────────────────────────────────────────────────────────
    // Ratio utilities
    // ──────────────────────────────────────────────────────────────────────

    /**
     * @brief Returns the inverted ratio: 1.0f - ratio
     *        Used for "remaining space" in flex layout, splitter logic, etc.
     *        Input is safely clamped to [0.0, 1.0].
     * @param ratio Input ratio.
     * @return 1.0f - ratio ∈ [0.0, 1.0].
     */
    constexpr float InvertRatio(const float ratio) noexcept
    {
        const float r = Clamp(ratio, 0.0f, 1.0f);
        return 1.0f - r;
    }

    // ──────────────────────────────────────────────────────────────────────
    // SnapToGrid
    // ──────────────────────────────────────────────────────────────────────

    /**
     * @brief Snaps a float to the nearest grid multiple.
     *        If grid == 0.0f returns the value unchanged.
     */
    constexpr float SnapToGrid(const float value, const float grid) noexcept
    {
        return grid == 0.0f ? value : std::roundf(value / grid) * grid;
    }

    /**
     * @brief Snaps an integer to the nearest lower-or-equal grid multiple.
     *        If grid == 0 returns the value unchanged.
     */
    constexpr int SnapToGrid(const int value, const int grid) noexcept
    {
        return grid == 0 ? value : value / grid * grid;
    }
    }

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

    namespace gpu::vulkan {
        uint32_t FindMemoryType(const vk::PhysicalDeviceMemoryProperties &memoryProperties, uint32_t typeBits,
               vk::MemoryPropertyFlags requirementsMask){

            auto typeIndex = static_cast<uint32_t>(~0);

            for ( uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++ )
            {
                if ( typeBits & 1 && ( memoryProperties.memoryTypes[i].propertyFlags & requirementsMask ) == requirementsMask )
                {
                    typeIndex = i;
                    break;
                }

                typeBits >>= 1;
            }

            assert( typeIndex != static_cast<uint32_t>(~0));
            return typeIndex;
        }

        vk::raii::DeviceMemory AllocateDeviceMemory( vk::raii::Device const & device, vk::PhysicalDeviceMemoryProperties const & memoryProperties,
         vk::MemoryRequirements const &  memoryRequirements, vk::MemoryPropertyFlags memoryPropertyFlags ){
            uint32_t               memoryTypeIndex = FindMemoryType( memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags );
            vk::MemoryAllocateInfo memoryAllocateInfo( memoryRequirements.size, memoryTypeIndex );
            return {device, memoryAllocateInfo};
        }

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

            void ContinueNextFrame() {
                currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
            }
        };

        struct QueueFamilyIndices {
            uint32_t                                    graphicsFamily{0};
            uint32_t                                    presentFamily{0};
        };

        struct UniformBufferObject {
            glm::mat4                                   model;
            glm::mat4                                   view;
            glm::mat4                                   proj;
        };

        constexpr vk::DeviceSize UBO_BUFFER_SIZE = sizeof(UniformBufferObject);

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

        struct Buffer {
            std::optional<vk::raii::Buffer>             data{};
            std::optional<vk::raii::DeviceMemory>       memory{};
        };

        struct RemappableBuffer {
            std::optional<Buffer>                       buffer{};
            std::optional<void*>                        mapped{nullptr};
        };

        struct TextureImage {
            TextureImage(vk::raii::PhysicalDevice const &  physicalDevice,
                  vk::raii::Device const &          device,
                  uint32_t width, uint32_t          height,
                  vk::Format format, vk::ImageTiling tiling,
                  vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, vk::SharingMode shareMode = vk::SharingMode::eExclusive) {

                vk::ImageCreateInfo imageInfo{};
                imageInfo
                    .setImageType(vk::ImageType::e2D)
                    .setFormat(format)
                    .setExtent({ width, height, 1 })
                    .setMipLevels(1)
                    .setArrayLayers(1)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setTiling(tiling)
                    .setUsage(usage)
                    .setSharingMode(shareMode);

                data.emplace(device, imageInfo);


                vk::MemoryRequirements memoryRequirements = data->getMemoryRequirements();

                vk::MemoryAllocateInfo allocInfo{};

                allocInfo

                    .setAllocationSize(memoryRequirements.size)

                    .setMemoryTypeIndex(FindMemoryType(physicalDevice.getMemoryProperties(), memoryRequirements.memoryTypeBits, properties));

                memory.emplace(device, allocInfo);

                data->bindMemory(*memory, 0);
            }

            TextureImage(const GPUResources& gpu, uint32_t width, uint32_t height, vk::Format format,
                vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,
                vk::SharingMode shareMode = vk::SharingMode::eExclusive) :
                TextureImage(gpu.physicalDevice.value(), gpu.device.value(), width, height, format, tiling, usage, properties, shareMode){}

            ~TextureImage() = default;

            std::optional<vk::raii::Image>              data{};
            std::optional<vk::raii::DeviceMemory>       memory{};
            std::optional<vk::raii::ImageView>          view{};
            vk::Format                                  format{ vk::Format::eUndefined};
            vk::Extent2D                                extent{ 0, 0 };
            vk::ImageViewType                           viewType{ vk::ImageViewType::e2D };
            vk::ImageSubresourceRange                   subresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};

            void clear() {
                view.reset();
                data.reset();
                memory.reset();
            }
        };
    }

    namespace windowing {
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
            std::optional<gpu::vulkan::SwapchainResource>       swapchainResource{};

            // Getters
            [[nodiscard]] WindowHandle* getHandle() const { return handle.get(); }
            bool getExtent(int& width, int& height) const {
#ifdef USE_SDL
                SDL_GetWindowSize(handle.get(), &width, &height);
#else
                glfwGetFramebufferSize(handle.get(), &width, &height);
#endif

                return width == 0 || height == 0;
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

    namespace input {
        struct InputResource;
        struct EventCallbackPool;

        enum class CursorType {
            eDefault,
            eEWResize,
            eNSResize,
            eNWSEResize,
            eNESWResize
        };

        enum class ActionPhase {
            eWait,
            eStart,
            ePerform,
            eEnd,
            eReset,
            eRepeat,
            eSleep
        };

        enum class MouseButtonState {
            eNone,
            eDown,
            eUp,
            eHold,
        };

        enum class MouseButton {
            eNone,eLeft, eRight, eMiddle
        };


        struct StandardCursorResource {
#ifdef USE_SDL
            using CursorHandle = SDL_Cursor;
            using DestroyFunc = decltype(&SDL_DestroyCursor);

#else
            using CursorHandle = GLFWcursor;
            using DestroyFunc = decltype(&glfwDestroyCursor);
#endif



            CursorType                                  currentCursor{CursorType::eDefault};
            std::unique_ptr<CursorHandle, DestroyFunc>  defaultCursor{nullptr,nullptr};
            std::unique_ptr<CursorHandle, DestroyFunc>  ewResizeCursor{nullptr,nullptr};
            std::unique_ptr<CursorHandle, DestroyFunc>  nsResizeCursor{nullptr,nullptr};
            std::unique_ptr<CursorHandle, DestroyFunc>  nwseResizeCursor{nullptr,nullptr};
            std::unique_ptr<CursorHandle, DestroyFunc>  neswResizeCursor{nullptr,nullptr};

            void ResetAll() {
                defaultCursor.reset();
                ewResizeCursor.reset();
                nsResizeCursor.reset();
                nwseResizeCursor.reset();
                neswResizeCursor.reset();
            }
        };

        struct EventCallbackPool {
            struct Handler {
                Handler(const Handler&) = delete;
                Handler(EventCallbackPool* p, std::size_t i) : pool(p), index(i) {}
                Handler() = default;
                Handler(Handler&& o) noexcept : pool(o.pool), index(o.index) { o.pool = nullptr; o.index = SIZE_MAX; }
                ~Handler() { disconnect(); }
                Handler& operator=(const Handler&) = delete;
                Handler& operator=(Handler&& o) noexcept { disconnect(); pool = o.pool; index = o.index; o.pool = nullptr; o.index = SIZE_MAX; return *this; }

                EventCallbackPool* pool = nullptr;
                std::size_t index = SIZE_MAX;

                void disconnect() { if (pool && index != SIZE_MAX) pool->unbind(index); pool = nullptr; index = SIZE_MAX; }
                [[nodiscard]] bool is_connected() const { return pool != nullptr; }
            };

            using Callback = std::function<void(InputResource&)>;

            [[nodiscard]] Handler bind(Callback cb) {
                std::size_t idx = free_list.empty() ? callbacks.size() : free_list.back();
                if (free_list.empty()) {
                    callbacks.emplace_back();
                    active.push_back(false);
                } else {
                    free_list.pop_back();
                }
                callbacks[idx] = std::move(cb);
                active[idx] = true;
                return {this, idx};
            }

            void unbind(std::size_t idx) {
                if (idx < active.size() && active[idx]) {
                    active[idx] = false;
                    free_list.push_back(idx);
                    callbacks[idx] = nullptr;
                }
            }

            void clear() {
                for (std::size_t i = 0; i < active.size(); ++i) {
                    unbind(i);
                }
            }

            void invoke(InputResource& input) const {
                for (std::size_t i = 0; i < active.size(); ++i) {
                    if (active[i] && callbacks[i]) callbacks[i](input);
                }
            }

        private:
            std::vector<Callback> callbacks;
            std::vector<bool> active;
            std::vector<std::size_t> free_list;
        };

        struct Action {
            explicit Action(const std::string &name_, const std::chrono::milliseconds& timeout_) : id(utilities::GenerateUniqueID(name_)) , name(name_), timeout(timeout_) {
                debug::log(debug::LogLevel::eInfo, "InputAction CREATED: {} [id: {:#x}]", name, id);
            }
            ~Action() = default;

            const uint32_t                          id;
            const std::string                       name;
            ActionPhase                             phase = ActionPhase::eSleep;
            float                                   value1 = 0.0f;
            float                                   value2 = 0.0f;
            bool                                    isStarted{false};
            bool                                    isPerformed{false};
            bool                                    isEnded{false};
            uint32_t                                triggerCount{0};

            std::chrono::steady_clock::time_point   startTime = std::chrono::steady_clock::now();
            std::chrono::milliseconds               timeout{1000};

            void refresh() noexcept {
                if (phase == ActionPhase::eStart && isStarted) {
                    phase = ActionPhase::ePerform;
                }else if (phase == ActionPhase::eEnd && isEnded) {
                    phase = std::chrono::steady_clock::now() - startTime > timeout? ActionPhase::eReset : ActionPhase::eRepeat;
                }else if (phase == ActionPhase::eRepeat) {
                    isStarted = false;
                    isPerformed = false;
                    isEnded = false;
                    phase = ActionPhase::eWait;
                }
                else if (phase == ActionPhase::eReset) {
                    isStarted = false;
                    isPerformed = false;
                    isEnded = false;
                    triggerCount = 0;
                    phase = ActionPhase::eSleep;
                }
            }

            void perform() noexcept {
                if (phase == ActionPhase::eStart) {
                    triggerCount++;
                    isStarted = true;
                }else if (phase == ActionPhase::ePerform && !isPerformed) {
                    isPerformed = true;
                }else if (phase == ActionPhase::eEnd) {
                    isEnded = true;
                }else if (phase == ActionPhase::eWait) {
                    phase = std::chrono::steady_clock::now() - startTime > timeout? ActionPhase::eReset : ActionPhase::eWait;
                }
            }
        };

        enum class MouseMotionState : uint8_t {
            Idle,     // No movement
            Moving,   // Delta > 0
            Stopping  // Delta == 0, was Moving
        };

        constexpr  std::string MouseMotionStateToString(const MouseMotionState& state) {
            switch (state) {
                case MouseMotionState::Moving: return "Moving";
                case MouseMotionState::Stopping: return "Stopping";
                case MouseMotionState::Idle: return "Idle";
                default: return "Unknown";
            }
        }

        struct InputResource {

            glm::ivec2 mousePosition{0,0};
            glm::ivec2 mouseDelta{0, 0};
            glm::ivec2 mouseWheel{0,0};

            MouseMotionState mouseMotionState{MouseMotionState::Idle};
            Action leftMouseButtonAction{"left-mouse-button", std::chrono::milliseconds{500}};
            Action rightMouseButtonAction{"right-mouse-button", std::chrono::milliseconds{500}};
            Action middleMouseButtonAction{"middle-mouse-button", std::chrono::milliseconds{500}};

            EventCallbackPool onMouseMoveCallbackPool{};
            EventCallbackPool onMouseStopCallbackPool{};
            EventCallbackPool onMouseWheelCallbackPool{};

            EventCallbackPool onLeftMouseButtonCallbackPool{};
            EventCallbackPool onRightMouseButtonCallbackPool{};
            EventCallbackPool onMiddleMouseButtonCallbackPool{};

            bool mouseLeftButton{false}, mouseRightButton{false}, mouseMiddleButton{false};

            void refresh() noexcept {
                mouseWheel = {0,0};
                leftMouseButtonAction.refresh();
            }
            void updateMouseDelta(const glm::ivec2& pos) noexcept { mouseDelta = pos - mousePosition; }
            [[nodiscard]] float getMouseDeltaMagnitude() const noexcept { return glm::length(glm::vec2(mouseDelta)); }

            void onMouseMove() {
                onMouseMoveCallbackPool.invoke(*this);
            }

            void onMouseStop() {
                onMouseStopCallbackPool.invoke(*this);
            }

            void onLeftMouseButton() {

                leftMouseButtonAction.perform();

                onLeftMouseButtonCallbackPool.invoke(*this);
            }

            void onRightMouseButton() {
                onRightMouseButtonCallbackPool.invoke(*this);
            }

            void onMiddleMouseButton() {
                onMiddleMouseButtonCallbackPool.invoke(*this);
            }

            void onMouseWheel() {
                onMouseWheelCallbackPool.invoke(*this);
            }
        };
    }

    namespace geometry {
        enum class PanelAlignment { eRow, eColumn };
        enum class PickingMode    { ePosition, eIgnore };
        enum class ScalingMode    { eFlex, eFixed };

        constexpr uint32_t RESIZER_THICKNESS = 12;
        constexpr int32_t RESIZER_OFFSET = 6;
        constexpr int RESIZER_SNAP_GRID = 4;

        struct UniformBufferObject {
            glm::mat4           model{};
            glm::mat4           view{};
            glm::mat4           proj{};
        };

    struct Length {
        enum class Unit { Pixel, Percent };

        std::optional<Unit> unit = std::nullopt;  // nullopt = auto (default stretch)
        float value = 0.0f;                       // 0.0f + nullopt = auto (fill available/parent)

        constexpr Length() noexcept = default;    // auto: resolves to full remain/parent (universal default)
        constexpr Length(float v, Unit u) noexcept : unit(u), value(v) {}

        // Factories
        static constexpr Length Pixels(float v) noexcept { return {v, Unit::Pixel}; }
        static constexpr Length Percent(float v) noexcept { return {v, Unit::Percent}; }

        // Auto sentinel (default ctor alias)
        static constexpr Length Auto() noexcept { return {}; }  // Explicit "stretch to fit"

        // Queries
        [[nodiscard]] constexpr bool isPixel() const noexcept { return unit == Unit::Pixel; }
        [[nodiscard]] constexpr bool isPercent() const noexcept { return unit == Unit::Percent; }
        [[nodiscard]] constexpr bool isAuto() const noexcept { return !unit.has_value() && value == 0.0f; }  // auto default
        [[nodiscard]] constexpr bool hasUnit() const noexcept { return unit.has_value(); }
    };

    // Literals (renamed: _fill → _auto)
    constexpr Length operator""_px(unsigned long long v) noexcept { return Length::Pixels(static_cast<float>(v)); }
    constexpr Length operator""_px(long double v) noexcept { return Length::Pixels(static_cast<float>(v)); }
    constexpr Length operator""_pct(unsigned long long v) noexcept { return Length::Percent(static_cast<float>(v)); }
    constexpr Length operator""_pct(long double v) noexcept { return Length::Percent(static_cast<float>(v)); }
    constexpr Length operator""_auto(unsigned long long) noexcept { return Length::Auto(); }

    struct RectLayout {
        int minWidth{0};
        int minHeight{0};
        int maxWidth{0};
        int maxHeight{0};
        int baseWidth{0};
        int baseHeight{0};
        int greaterMinWidth{0};
        int greaterMinHeight{0};
    };

    struct Viewpanel;

    struct ViewpanelResizerContext {
        ViewpanelResizerContext() = default;
        ~ViewpanelResizerContext() = default;

        Viewpanel*              targetPanel= nullptr;

        bool                    isActive= false;
        bool                    isRow= true;

        size_t                  index= 0;
        size_t                  pushIndex= 0;
        size_t                  panelsCount= 0;

        int                     clickOffset= 0;
        int                     valueOffset= 0;
        int                     currentValue= 0;
        int                     min= 0;
        int                     max= 0;
        int                     parentExtent = 0;
    };

    struct Viewpanel {
        explicit Viewpanel(PanelAlignment align = PanelAlignment::eColumn,PickingMode picking = PickingMode::ePosition,ScalingMode scaling = ScalingMode::eFlex):alignment(align), pickingMode(picking), scalingMode(scaling) {}
        ~Viewpanel() = default;

        std::string             name{"Viewpanel"};

        vk::Rect2D              rect{{0,0},{100,100}};
        Length                  width{};
        Length                  height{};
        Length                  minWidth{};
        Length                  minHeight{};
        Length                  maxWidth{};
        Length                  maxHeight{};
        RectLayout              layout{};

        vk::Rect2D              resizerZone{{0,0},{0,0}};

        Viewpanel*              parent{nullptr};
        std::vector<Viewpanel*> children;

        PanelAlignment          alignment{PanelAlignment::eColumn};
        PickingMode             pickingMode{PickingMode::ePosition};
        ScalingMode             scalingMode{ScalingMode::eFlex};

        vk::ClearColorValue     clearColor{0.5f, 0.5f, 0.5f, 1.0f};
        vk::ClearColorValue     clearColor2{0.8f, 0.8f, 0.8f, 1.0f};

        float                   resizerValue{0.0f};
        float                   flexGlow{1.0f};
        float                   flexShrink{1.0f};

        [[nodiscard]] bool hasParent() const noexcept { return parent != nullptr; }
        [[nodiscard]] bool isChildrenEmpty() const noexcept { return children.empty(); }
        [[nodiscard]] size_t childCount() const noexcept { return children.size(); }
        [[nodiscard]] Viewpanel* getChild(const size_t i) const noexcept { return children[i]; }
        [[nodiscard]] size_t getLastChildIndex() const noexcept { return children.size() - 1; }
        [[nodiscard]] bool isRow() const noexcept { return alignment == PanelAlignment::eRow; }
        [[nodiscard]] bool isColumn() const noexcept { return alignment == PanelAlignment::eColumn; }


        void add(Viewpanel* child)
        {
            if (child && child->parent != this)
            {
                if (child->parent) child->parent->remove(child);
                child->parent = this;
                children.push_back(child);
            }
        }

        void remove(Viewpanel* child)
        {
            if (!child) return;
            auto it = std::find(children.begin(), children.end(), child);
            if (it != children.end())
            {
                child->parent = nullptr;
                children.erase(it);
            }
        }

        [[nodiscard]] std::vector<Viewpanel*> getAllPanels() const &
        {
            std::vector<Viewpanel*> result;
            result.push_back(const_cast<Viewpanel*>(this));

            std::function<void(const Viewpanel&)> collect = [&](const Viewpanel& panel)
            {
                for (size_t i = 0; i < panel.childCount(); ++i)
                {
                    Viewpanel* child = panel.getChild(i);
                    result.push_back(child);
                    collect(*child);
                }
            };

            collect(*this);
            return result;
        }

        void setBackgroundColor(const vk::ClearColorValue& color) {
            clearColor = color;
        }
    };

    struct Viewport {
        explicit Viewport(const windowing::WindowResource& window) : window(window) {}
        ~Viewport() = default;

        const windowing::WindowResource&                    window;
        vk::Extent2D                                        extent{};
        Viewpanel*                                          panel = nullptr;
        Viewpanel*                                          hoveredPanel = nullptr;
        Viewpanel*                                          focusedPanel = nullptr;
        ViewpanelResizerContext                             resizerContext{};
        std::optional<input::EventCallbackPool::Handler>    mouseMoveEventHandle{};
        std::optional<input::EventCallbackPool::Handler>    leftClickEventHandle{};
    };



    struct DiscadeltaBaseFiller {

        int                     remainLength{0};
        int                     accumulateOffset{0};
        float                   accumulateStepRatio{0.0f};
        std::vector<int*>       lengths{};
        std::vector<int>        mins{};
        std::vector<int>        reduceDistances{};
        std::vector<float>      stepRatios{};

        DiscadeltaBaseFiller() = default;
        ~DiscadeltaBaseFiller() = default;

        explicit DiscadeltaBaseFiller(const size_t& steps) {
            if (steps > 0) {
                lengths.reserve(steps);
                mins.reserve(steps);
                reduceDistances.reserve(steps);
                stepRatios.reserve(steps);
            }
        }
    };

    struct DiscadeltaGlowFiller {
        int                     remainLength{0};
        float                   accumulateStepRatio{0.0f};
        std::vector<int*>       lengths{};
        std::vector<int>        maxs{};
        std::vector<float>      stepRatios{};

        DiscadeltaGlowFiller() = default;
        ~DiscadeltaGlowFiller() = default;

        explicit DiscadeltaGlowFiller(const size_t& steps) {
            if (steps > 0) {
                lengths.reserve(steps);
                maxs.reserve(steps);
                stepRatios.reserve(steps);
            }
        }
    };

    struct DiscadeltaContext {
        int                     accumulateOffset{0};
        std::vector<int>        baseLengths{};
        std::vector<int>        glowLengths{};

        DiscadeltaContext() = default;
        ~DiscadeltaContext() = default;

        explicit DiscadeltaContext(const int& accumulateOffset_, const size_t& steps):accumulateOffset(accumulateOffset_) {
            if (steps > 0) {
                baseLengths.reserve(steps);
                glowLengths.reserve(steps);
            }
        }
    };



        struct Vertex {
            glm::vec2 position;
            glm::vec2 uv;
            glm::vec4 color;

            static constexpr vk::VertexInputBindingDescription
            getBindingDescription(uint32_t binding) {
                vk::VertexInputBindingDescription d{};
                d.setBinding(binding)
                 .setStride(sizeof(Vertex))
                 .setInputRate(vk::VertexInputRate::eVertex);
                return d;
            }

            static constexpr std::array<vk::VertexInputAttributeDescription, 3>
            getAttributeDescriptions(uint32_t binding) {
                std::array<vk::VertexInputAttributeDescription, 3> a{};
                a[0].setBinding(binding).setLocation(0)
                     .setFormat(vk::Format::eR32G32Sfloat).setOffset(0);
                a[1].setBinding(binding).setLocation(1)
                     .setFormat(vk::Format::eR32G32Sfloat)
                     .setOffset(offsetof(Vertex, uv));
                a[2].setBinding(binding).setLocation(2)
                     .setFormat(vk::Format::eR32G32B32A32Sfloat)
                     .setOffset(offsetof(Vertex, color));
                return a;
            }
        };

        constexpr Vertex QuadVertices[]{
            {{0.f, 0.f}, {0.f, 0.f}, {1.f,1.f,1.f,1.f}}, // TL
            {{1.f, 0.f}, {1.f, 0.f}, {1.f,1.f,1.f,1.f}}, // TR
            {{1.f, 1.f}, {1.f, 1.f}, {1.f,1.f,1.f,1.f}}, // BR
            {{0.f, 1.f}, {0.f, 1.f}, {1.f,1.f,1.f,1.f}}, // BL
        };

        constexpr uint16_t QuadIndices[]{
            0, 1, 2, 2, 3, 0
        };

        constexpr vk::DeviceSize VERTEX_BUFFER_SIZE         = sizeof(Vertex);
        constexpr vk::DeviceSize QUAD_VERTICES_BUFFER_SIZE      = sizeof(QuadVertices);
        constexpr vk::DeviceSize QUAD_INDICES_BUFFER_SIZE          = sizeof(QuadIndices);
        constexpr auto DEFAULT_QUAD_MESH_NAME = "default_quad_mesh";
        constexpr auto QUAD_VERTEX_COUNT = std::size(QuadVertices);
        constexpr auto QUAD_INDEX_COUNT = std::size(QuadIndices);

        struct MeshResource {
            std::string                             name;
            size_t                                  id{0};

            std::vector<Vertex>                     vertices;
            std::vector<uint16_t>                   indices;

            std::optional<gpu::vulkan::Buffer>      vertexBuffer;
            std::optional<gpu::vulkan::Buffer>      indexBuffer;

            explicit MeshResource(const std::string_view name_ = {}): name(name_) , id(utilities::GenerateUniqueID(name_)) {}
        };
    }

    namespace gui {
        struct Style {
            glm::vec4                                       backgroundColor = {0.5f, 0.5f, 0.5f, 1.0f};
            glm::vec4                                       borderTopColor = {0.0f, 0.0f, 0.0f, 1.0f};
            glm::vec4                                       borderRightColor = {0.0f, 0.0f, 0.0f, 1.0f};
            glm::vec4                                       borderBottomColor = {0.0f, 0.0f, 0.0f, 1.0f};
            glm::vec4                                       borderLeftColor = {0.0f, 0.0f, 0.0f, 1.0f};
            glm::vec4                                       borderThickness = {1.0f, 1.0f, 1.0f, 1.0f}; // Top, right, bottom, left
            glm::vec4                                       cornerRadius = {10.0f, 10.0f, 10.0f, 10.0f}; // Top-left, top-right, bottom-left, bottom-right
        };

        struct StyleResource {
            std::string                                     name;
            size_t                                          id{0};
            Style                                           content;
            gpu::vulkan::Buffer                             buffer{};
        };

        constexpr vk::DeviceSize GUI_STYLE_BUFFER_SIZE = sizeof(Style);

        struct GUIElement {
            glm::vec3                                       position = {0.0f, 0.0f, 0.0f};
            glm::vec3                                       rotation = {0.0f, 0.0f, 0.0f};
            glm::vec3                                       scale = {1.0f, 1.0f, 1.0f};

            std::vector<gpu::vulkan::Buffer>                uniformBuffers;
            std::vector<void*>                              uniformBuffersMapped;

            std::vector<vk::raii::DescriptorSet>            descriptorSets;

            [[nodiscard]] glm::mat4 getModelMatrix() const {
                auto model = glm::mat4(1.0f);
                model = glm::translate(model, position);
                model = glm::rotate(model, rotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
                model = glm::rotate(model, rotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
                model = glm::rotate(model, rotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
                model = glm::scale(model, scale);
                return model;
            }
        };

        struct GUIResource {
            std::vector<StyleResource>                      styles{};
            std::optional<vk::raii::PipelineCache>          pipelineCache{};
            std::optional<vk::raii::Pipeline>               pipeline{};
            std::optional<vk::raii::DescriptorSetLayout>    descriptorSetLayout{};
            std::optional<vk::raii::PipelineLayout>         pipelineLayout{};
            std::optional<geometry::MeshResource>           meshResource{};
            std::optional<vk::raii::DescriptorPool>         descriptorPool{};

            std::optional<GUIElement>                       elements{};
        };
    }
}
