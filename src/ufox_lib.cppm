module;



#include <utility>

#include <vector>

#include <optional>

#include <iostream>

#include <string>

#include <format>

#include <functional>

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
        size_t GenerateUniqueID(const std::vector<glm::vec4>& fields) {
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

        size_t GenerateUniqueID(const std::vector<float>& fields) {

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

//     template <typename BitType>
// struct Flags {
//         using Enum = BitType;
//         using underlying_type = std::underlying_type_t<Enum>;
//
//         // --- Data ---
//         constexpr Flags() noexcept = default;
//         constexpr Flags(Enum bit) noexcept : value(static_cast<underlying_type>(bit)) {}
//         explicit constexpr Flags(underlying_type v) noexcept : value(v) {}
//
//         // --- Conversion ---
//         explicit constexpr operator underlying_type() const noexcept { return value; }
//         explicit constexpr operator bool() const noexcept { return value != 0; }
//
//         // --- Bitwise Operations (all constexpr) ---
//         constexpr Flags operator|(Flags other) const noexcept {
//             return Flags(value | other.value);
//         }
//         constexpr Flags operator&(Flags other) const noexcept {
//             return Flags(value & other.value);
//         }
//         constexpr Flags operator^(Flags other) const noexcept {
//             return Flags(value ^ other.value);
//         }
//         constexpr Flags operator~() const noexcept {
//             return Flags(~value);
//         }
//
//         constexpr Flags& operator|=(Flags other) noexcept {
//             value |= other.value;
//             return *this;
//         }
//         constexpr Flags& operator&=(Flags other) noexcept {
//             value &= other.value;
//             return *this;
//         }
//
//         // --- Query ---
//         constexpr bool has(Enum bit) const noexcept {
//             return (value & static_cast<underlying_type>(bit)) != 0;
//         }
//         [[nodiscard]] constexpr bool any() const noexcept { return value != 0; }
//         [[nodiscard]] constexpr bool none() const noexcept { return value == 0; }
//
//         // --- Equality ---
//         constexpr bool operator==(const Flags& other) const noexcept = default;
//         constexpr bool operator!=(const Flags& other) const noexcept = default;
//
//         underlying_type value = 0;
//     };
//
//     // --- Free-function operators (for Enum | Enum) ---
//     template <typename BitType>
//     constexpr Flags<BitType> operator|(BitType lhs, BitType rhs) noexcept {
//         return Flags<BitType>(lhs) | Flags<BitType>(rhs);
//     }
//
//     template <typename BitType>
//     constexpr Flags<BitType> operator&(BitType lhs, BitType rhs) noexcept {
//         return Flags<BitType>(lhs) & Flags<BitType>(rhs);
//     }





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
                                                           vk::MemoryRequirements const &             memoryRequirements,
                                                           vk::MemoryPropertyFlags                    memoryPropertyFlags )

        {

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

            Buffer( vk::raii::PhysicalDevice const & physicalDevice,

                                vk::raii::Device const &         device,

                                vk::DeviceSize                   size,

                                vk::BufferUsageFlags             usage,

                                vk::MemoryPropertyFlags          propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent ) {

                vk::BufferCreateInfo bufferInfo{};

                bufferInfo

                .setSize( size )

                .setUsage( usage );

                data.emplace(device, bufferInfo);



                memory = AllocateDeviceMemory( device, physicalDevice.getMemoryProperties(), data->getMemoryRequirements(), propertyFlags );

                data->bindMemory( *memory,0 );

            }



            Buffer(const GPUResources& gpu, vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags propertyFlags = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent ) :

                Buffer(gpu.physicalDevice.value(), gpu.device.value(), size, usage, propertyFlags){}





            Buffer() = default;





            std::optional<vk::raii::DeviceMemory>       memory{nullptr};

            std::optional<vk::raii::Buffer>             data{nullptr};

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
            vk::ImageViewType                               viewType{ vk::ImageViewType::e2D };
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

            explicit WindowResource(const vk::raii::Instance& instance, WindowHandle* wnd): handle{wnd, DestroyFunc{}}{
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
            eNWResize,
            eNEResize,
            eSWResize,
            eSEResize
        };

        enum class MouseButtonState {
            eNone,
            eDown,
            eUp,
            eHold,
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

            EventCallbackPool onMouseMoveCallbackPool{};
            EventCallbackPool onMouseStopCallbackPool{};
            EventCallbackPool onMouseDownCallbackPool{};
            EventCallbackPool onMouseUpCallbackPool{};
            EventCallbackPool onMouseWheelCallbackPool{};

            bool mouseLeftButton{false}, mouseRightButton{false}, mouseMiddleButton{false};

            CursorType currentCursor{CursorType::eDefault};

            void refresh() noexcept { mouseWheel = {0,0}; }
            void updateMouseDelta(const glm::ivec2& pos) noexcept { mouseDelta = pos - mousePosition; }
            [[nodiscard]] float getMouseDeltaMagnitude() const noexcept { return glm::length(glm::vec2(mouseDelta)); }

            void onMouseMove() {
                onMouseMoveCallbackPool.invoke(*this);
            }

            void onMouseStop() {
                onMouseStopCallbackPool.invoke(*this);
            }

            void onMouseDown() {
                onMouseDownCallbackPool.invoke(*this);
            }

            void onMouseUp() {
                onMouseUpCallbackPool.invoke(*this);
            }

            void onMouseWheel() {
                onMouseWheelCallbackPool.invoke(*this);
            }

            void setCursor(const CursorType type) {
                if (currentCursor != type) {
                    switch (type) {
                        case CursorType::eDefault: {
                            SDL_SetCursor(_defaultCursor.get());
                            break;
                        }
                        case CursorType::eEWResize: {
                            SDL_SetCursor(_ewResizeCursor.get());
                            break;
                        }
                        case CursorType::eNSResize: {
                            SDL_SetCursor(_nsResizeCursor.get());
                            break;
                        }
                        case CursorType::eNWResize: {
                            SDL_SetCursor(_nwResizeCursor.get());
                            break;
                        }
                        case CursorType::eNEResize: {
                            SDL_SetCursor(_neResizeCursor.get());
                            break;
                        }
                        case CursorType::eSWResize: {
                            SDL_SetCursor(_swResizeCursor.get());
                            break;
                        }
                        case CursorType::eSEResize: {
                            SDL_SetCursor(_seResizeCursor.get());
                            break;
                        }
                        default: {
                            currentCursor = CursorType::eDefault;
                            SDL_SetCursor(_defaultCursor.get());
                            break;
                        }
                    }

                    currentCursor = type;
                }
            }

        private:
#ifdef USE_SDL
            std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> _defaultCursor ={SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT), SDL_DestroyCursor};
            std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> _ewResizeCursor ={SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE), SDL_DestroyCursor};
            std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> _nsResizeCursor ={SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE), SDL_DestroyCursor};
            std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> _nwResizeCursor ={SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NW_RESIZE), SDL_DestroyCursor};
            std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> _neResizeCursor ={SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NE_RESIZE), SDL_DestroyCursor};
            std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> _swResizeCursor ={SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SW_RESIZE), SDL_DestroyCursor};
            std::unique_ptr<SDL_Cursor, decltype(&SDL_DestroyCursor)> _seResizeCursor ={SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SE_RESIZE), SDL_DestroyCursor};
#endif

        };
    }






    namespace gui {

        struct Vertex {

            glm::vec2 position;

            glm::vec2 uv;

            glm::vec4 color;



            static constexpr  vk::VertexInputBindingDescription getBindingDescription(uint32_t binding) {

                vk::VertexInputBindingDescription bindingDescription{};

                bindingDescription.setBinding(binding).setStride(sizeof(Vertex)).setInputRate(vk::VertexInputRate::eVertex);

                return bindingDescription;

            }



            static constexpr  std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions(uint32_t binding) {

                std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

                attributeDescriptions[0].setBinding(binding).setLocation(0).setFormat(vk::Format::eR32G32Sfloat).setOffset(0);

                attributeDescriptions[1].setBinding(binding).setLocation(1).setFormat(vk::Format::eR32G32Sfloat).setOffset(offsetof(Vertex, uv));

                attributeDescriptions[2].setBinding(binding).setLocation(2).setFormat(vk::Format::eR32G32B32A32Sfloat).setOffset(offsetof(Vertex, color));

                return attributeDescriptions;

            }

        };







        struct UniformBufferObject {

            glm::mat4 model;

            glm::mat4 view;

            glm::mat4 proj;

        };



        constexpr Vertex Geometries[] {

            {{0.0f, 0.0f,}, {0.0f, 0.0f}, {1.0f, 1.0f, 1.0f,1.0f}}, // Top-left

             {{1.0f, 0.0f},  {1.0f, 0.0f}, {1.0f, 1.0f, 1.0f,1.0f}}, // Top-right

             {{1.0f, 1.0f},  {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f,1.0f}}, // Bottom-right

             {{0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 1.0f, 1.0f,1.0f}}, // Bottom-left

    };

        constexpr uint16_t Indices[] {
            0, 1, 2, 2, 3, 0,
    };


        // GUIStyle: Visual properties for GUI elements

        struct Style {

            glm::vec4 backgroundColor = {0.5f, 0.5f, 0.5f, 1.0f};

            glm::vec4 borderTopColor = {0.0f, 0.0f, 0.0f, 1.0f};

            glm::vec4 borderRightColor = {0.0f, 0.0f, 0.0f, 1.0f};

            glm::vec4 borderBottomColor = {0.0f, 0.0f, 0.0f, 1.0f};

            glm::vec4 borderLeftColor = {0.0f, 0.0f, 0.0f, 1.0f};

            glm::vec4 borderThickness = {1.0f, 1.0f, 1.0f, 1.0f}; // Top, right, bottom, left

            glm::vec4 cornerRadius = {10.0f, 10.0f, 10.0f, 10.0f}; // Top-left, top-right, bottom-left, bottom-right



            [[nodiscard]] size_t generateUniqueID() const {

                return ufox::utilities::GenerateUniqueID({

                    backgroundColor,

                    borderTopColor,

                    borderRightColor,

                    borderBottomColor,

                    borderLeftColor,

                    borderThickness,

                    cornerRadius

                });
            }
        };

        struct StyleBuffer {
            size_t                  hashUID;
            Style                   content;
            gpu::vulkan::Buffer     buffer{};
        };

        constexpr vk::DeviceSize GUI_VERTEX_BUFFER_SIZE = sizeof(Vertex);
        //constexpr vk::DeviceSize GUI_TRANSFORM_MATRIX_SIZE = sizeof(GUITransformMatrix);
        constexpr vk::DeviceSize GUI_RECT_MESH_BUFFER_SIZE = sizeof(Geometries);
        constexpr vk::DeviceSize GUI_INDEX_BUFFER_SIZE = sizeof(Indices);
        constexpr vk::DeviceSize GUI_STYLE_BUFFER_SIZE = sizeof(Style);

    }
}
