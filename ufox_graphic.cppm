
module;

#include <vulkan/vulkan_raii.hpp>
#include <vector>
#include <string>
#include <optional>
#include <SDL3/SDL_vulkan.h>
#include <map>
#include <set>

export module ufox_graphic;

import ufox_lib;
import ufox_windowing;

export namespace ufox::gpu::vulkan {

    struct PhysicalDeviceRequirements {
        static constexpr uint32_t MINIMUM_API_VERSION = vk::ApiVersion14;
        static constexpr uint32_t DISCRETE_GPU_SCORE_BONUS = 1000;
    };

    struct PhysicalDeviceCandidate {
        uint32_t score{0};
        std::unique_ptr<vk::raii::PhysicalDevice> device;
    };

    struct GraphicDeviceCreateInfo {
        PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr{reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_Vulkan_GetVkGetInstanceProcAddr())};
        vk::ApplicationInfo appInfo{};

        std::vector<const char*> requiredDeviceExtensions{
            vk::KHRSwapchainExtensionName,
            vk::KHRSpirv14ExtensionName,
            vk::KHRSynchronization2ExtensionName,
            vk::KHRCreateRenderpass2ExtensionName
        };

        bool enableDynamicRendering{true};
        bool enableExtendedDynamicState{true};
    };



    struct GraphicDeviceResource {
        std::optional<vk::raii::Context> context{};
        std::optional<vk::raii::Instance> instance{};
        std::vector<PhysicalDeviceCandidate> physicalDevices;
        size_t selectedPhysicalDevice{0};
        std::optional<vk::raii::Device> device{};
        std::optional<vk::raii::SurfaceKHR> surface{};
        std::optional<vk::raii::Queue> graphicsQueue{};
        std::optional<vk::raii::Queue> presentQueue{};
        vk::SurfaceCapabilitiesKHR surfaceCapabilities;
        std::vector<vk::PresentModeKHR> availablePresentModes;
        std::optional<vk::raii::SwapchainKHR> swapChain{};
        std::vector<vk::Image> swapChainImages;
        std::vector<vk::raii::ImageView> swapChainImageViews;
        vk::Format swapChainFormat{ vk::Format::eUndefined };
        vk::PresentModeKHR presentMode{ vk::PresentModeKHR::eFifo };
        vk::Extent2D swapChainExtent{ 0, 0 };

        [[nodiscard]] const vk::raii::PhysicalDevice& getPhysicalDevice() const {
            if (physicalDevices.empty() || selectedPhysicalDevice >= physicalDevices.size()) {
                throw std::runtime_error("No physical device available");
            }
            return *physicalDevices[selectedPhysicalDevice].device;
        }
    };


    [[nodiscard]] vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    [[nodiscard]] vk::Extent2D ChooseSwapExtent(SDL_Window& window, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities) {
        if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return surfaceCapabilities.currentExtent;
        }
        int width, height;
        SDL_GetWindowSizeInPixels(&window, &width, &height);
        return {
            std::clamp<uint32_t>(width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
            std::clamp<uint32_t>(height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height)
        };
    }

    std::optional<uint32_t> EvaluateDeviceSuitability(const vk::raii::PhysicalDevice& device) {
        auto deviceProperties = device.getProperties();
        auto deviceFeatures = device.getFeatures();

        if (deviceProperties.apiVersion < PhysicalDeviceRequirements::MINIMUM_API_VERSION ||
            !deviceFeatures.geometryShader) {
            return std::nullopt;
        }

        uint32_t score = deviceProperties.limits.maxImageDimension2D;
        if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            score += PhysicalDeviceRequirements::DISCRETE_GPU_SCORE_BONUS;
        }

        return score;
    }

    std::vector<PhysicalDeviceCandidate> GetAllPhysicalDevices(const std::vector<vk::raii::PhysicalDevice>& devices) {
        std::vector<PhysicalDeviceCandidate> candidates;
        for (const auto& device : devices) {
            if (auto deviceScore = EvaluateDeviceSuitability(device)) {
                candidates.push_back(PhysicalDeviceCandidate{
                    *deviceScore,
                    std::make_unique<vk::raii::PhysicalDevice>(device)
                });
            }
        }

        if (candidates.empty()) {
            throw std::runtime_error("Failed to find suitable GPU!");
        }

        std::sort(candidates.begin(), candidates.end(),
            [](const auto& a, const auto& b) { return a.score > b.score; });

        return candidates;
    }

    bool AreExtensionsSupported(const std::vector<const char*>& required, const std::vector<vk::ExtensionProperties>& available) {
        for (const auto* req : required) {
            bool found = false;
            for (const auto& avail : available) {
                if (strcmp(req, avail.extensionName) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                debug::log(debug::LogLevel::WARNING, "Missing extension: {}", req);
                return false;
            }
        }
        return true;
    }

    GraphicDeviceResource CreateGraphicDeviceResource(const GraphicDeviceCreateInfo& info, const windowing::sdl::UFoxWindowResource& mainWindow) {
        GraphicDeviceResource resource{};

#pragma region Create Context
        resource.context.emplace(info.vkGetInstanceProcAddr);
        auto const vulkanVersion {resource.context->enumerateInstanceVersion()};
        debug::log(debug::LogLevel::INFO, "Vulkan version: {}", vulkanVersion);
#pragma endregion

#pragma region Create Instance
        std::vector<vk::ExtensionProperties> availableExtensions = resource.context->enumerateInstanceExtensionProperties();
        uint32_t extensionCount;
        const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
        if (extensions == nullptr) throw std::runtime_error("Failed to get SDL Vulkan extensions");

        std::vector<const char*> requiredInstanceExtensions{};
        requiredInstanceExtensions.insert(requiredInstanceExtensions.end(), extensions, extensions + extensionCount);

        if (!AreExtensionsSupported(requiredInstanceExtensions, availableExtensions))
            throw std::runtime_error("Required instance extensions are missing");

        vk::InstanceCreateInfo createInfo{};
        createInfo.setPApplicationInfo(&info.appInfo)
                  .setEnabledExtensionCount(static_cast<uint32_t>(requiredInstanceExtensions.size()))
                  .setPpEnabledExtensionNames(requiredInstanceExtensions.data());

        resource.instance.emplace(*resource.context, createInfo);
#pragma endregion

#pragma region Create Surface
        VkSurfaceKHR raw_surface;
        if (!SDL_Vulkan_CreateSurface(mainWindow.window.get(), **resource.instance, nullptr, &raw_surface))
            throw windowing::sdl::SDLException("Failed to create surface");

        resource.surface.emplace(*resource.instance, raw_surface);
#pragma endregion

#pragma region Create Physical Device
        auto devices = resource.instance->enumeratePhysicalDevices();
        if (devices.empty()) {
            throw std::runtime_error("Failed to find GPUs with Vulkan support!");
        }

        resource.physicalDevices = GetAllPhysicalDevices(devices);
        resource.selectedPhysicalDevice = 0;
#pragma endregion

#pragma region Find Graphics Queue Families and Present Queue Family
        auto& physicalDevice = resource.getPhysicalDevice();
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        auto graphicsQueueFamilyProperty = std::find_if(queueFamilyProperties.begin(), queueFamilyProperties.end(),
            [](vk::QueueFamilyProperties const& qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });

        if (graphicsQueueFamilyProperty == queueFamilyProperties.end()) {
            throw std::runtime_error("Failed to find a queue family that supports graphics!");
        }

        auto graphicsIndex = static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        debug::log(debug::LogLevel::INFO, "Graphics queue family index: {}", graphicsIndex);

        auto presentIndex = physicalDevice.getSurfaceSupportKHR(graphicsIndex, *resource.surface)
                            ? graphicsIndex
                            : static_cast<uint32_t>(queueFamilyProperties.size());

        if (graphicsIndex == queueFamilyProperties.size() || presentIndex == queueFamilyProperties.size()) {
            throw std::runtime_error("Could not find a queue for graphics or present -> terminating");
        }

        std::set uniqueFamilies = { graphicsIndex, presentIndex };
        std::vector<vk::DeviceQueueCreateInfo> queueInfos(uniqueFamilies.size());

        int i = 0;

        for (auto family : uniqueFamilies) {
            float queuePriority = 0.0f;
            queueInfos[i].setQueueFamilyIndex(family)
                         .setQueueCount(1)
                         .setPQueuePriorities(&queuePriority);
            i++;
        }
#pragma endregion

#pragma region Create Logical Device
        std::vector<vk::ExtensionProperties> availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
        if (!AreExtensionsSupported(info.requiredDeviceExtensions, availableDeviceExtensions))
            throw std::runtime_error("Required device extensions are missing");

        auto features = physicalDevice.getFeatures2();
        vk::PhysicalDeviceVulkan13Features vulkan13Features;
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures;
        vulkan13Features.dynamicRendering = info.enableDynamicRendering ? vk::True : vk::False;
        extendedDynamicStateFeatures.extendedDynamicState = info.enableExtendedDynamicState ? vk::True : vk::False;
        vulkan13Features.pNext = &extendedDynamicStateFeatures;
        features.pNext = &vulkan13Features;

        vk::DeviceCreateInfo createDeviceInfo{};
        createDeviceInfo.setQueueCreateInfoCount(static_cast<uint32_t>(queueInfos.size()))
                       .setPQueueCreateInfos(queueInfos.data())
                       .setEnabledExtensionCount(static_cast<uint32_t>(info.requiredDeviceExtensions.size()))
                       .setPpEnabledExtensionNames(info.requiredDeviceExtensions.data())
                       .setPNext(&features);

        resource.device.emplace(physicalDevice, createDeviceInfo);
        resource.graphicsQueue.emplace(*resource.device, graphicsIndex, 0);
        resource.presentQueue.emplace(*resource.device, presentIndex, 0);
#pragma endregion

#pragma region Create Swapchain
        resource.surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*resource.surface);
        resource.availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*resource.surface);
        auto formats = physicalDevice.getSurfaceFormatsKHR(*resource.surface);
        vk::SurfaceFormatKHR surfaceFormat;
        for (const auto& format : formats) {
            if (format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                surfaceFormat = format;
        }
        resource.swapChainFormat = surfaceFormat.format;

        resource.presentMode = ChooseSwapPresentMode(resource.availablePresentModes);
        resource.swapChainExtent = ChooseSwapExtent(*mainWindow.window, resource.surfaceCapabilities);

        uint32_t imageCount = resource.surfaceCapabilities.minImageCount < 2 ? 2 : resource.surfaceCapabilities.minImageCount;
        if (resource.surfaceCapabilities.maxImageCount > 0 && imageCount > resource.surfaceCapabilities.maxImageCount) {
            imageCount = resource.surfaceCapabilities.maxImageCount;
        }
#pragma endregion

        return resource;
    }
}