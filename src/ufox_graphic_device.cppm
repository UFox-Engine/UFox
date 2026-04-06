module;

#if defined( _WIN32 )
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined( __linux__ )
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined( __APPLE__ )
#define VK_USE_PLATFORM_MACOS_MVK
#endif
#include <GLFW/glfw3.h>
#include <SDL3/SDL_vulkan.h>
#include <iostream>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

export module ufox_graphic_device;

import ufox_lib;

export namespace ufox::gpu::vulkan {
    vk::raii::DeviceMemory AllocateDeviceMemory( vk::raii::Device const & device, vk::PhysicalDeviceMemoryProperties const & memoryProperties,
             vk::MemoryRequirements const &  memoryRequirements, vk::MemoryPropertyFlags memoryPropertyFlags ){
        uint32_t               memoryTypeIndex = FindMemoryType( memoryProperties, memoryRequirements.memoryTypeBits, memoryPropertyFlags );
        vk::MemoryAllocateInfo memoryAllocateInfo( memoryRequirements.size, memoryTypeIndex );
        return {device, memoryAllocateInfo};
    }

    std::vector<std::string> GetDeviceExtensions() {
        return {
            vk::KHRSwapchainExtensionName,
            vk::KHRSpirv14ExtensionName,
            vk::KHRSynchronization2ExtensionName,
            vk::KHRCreateRenderpass2ExtensionName
        };
    }

    std::vector<std::string> GetInstanceExtensions()
    {
        std::vector<std::string> extensions;
        extensions.emplace_back(VK_KHR_SURFACE_EXTENSION_NAME );
    #if defined( VK_USE_PLATFORM_ANDROID_KHR )
        extensions.push_back( VK_KHR_ANDROID_SURFACE_EXTENSION_NAME );
    #elif defined( VK_USE_PLATFORM_METAL_EXT )
        extensions.push_back( VK_EXT_METAL_SURFACE_EXTENSION_NAME );
    #elif defined( VK_USE_PLATFORM_VI_NN )
        extensions.push_back( VK_NN_VI_SURFACE_EXTENSION_NAME );
    #elif defined( VK_USE_PLATFORM_WAYLAND_KHR )
        extensions.push_back( VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME );
    #elif defined( VK_USE_PLATFORM_WIN32_KHR )
        extensions.emplace_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME );
    #elif defined( VK_USE_PLATFORM_XCB_KHR )
        extensions.push_back( VK_KHR_XCB_SURFACE_EXTENSION_NAME );
    #elif defined( VK_USE_PLATFORM_XLIB_KHR )
        extensions.push_back( VK_KHR_XLIB_SURFACE_EXTENSION_NAME );
    #elif defined( VK_USE_PLATFORM_XLIB_XRANDR_EXT )
        extensions.push_back( VK_EXT_ACQUIRE_XLIB_DISPLAY_EXTENSION_NAME );
    #endif
        return extensions;
    }

    vk::AccessFlags2 GetAccessFlags2(const vk::ImageLayout& layout)
    {
        switch (layout)
        {
        case vk::ImageLayout::eUndefined:
            return {};
        case vk::ImageLayout::ePresentSrcKHR:
            return {};
        case vk::ImageLayout::ePreinitialized:
            return vk::AccessFlagBits2::eHostWrite;
        case vk::ImageLayout::eColorAttachmentOptimal:
            return vk::AccessFlagBits2::eColorAttachmentRead | vk::AccessFlagBits2::eColorAttachmentWrite;
        case vk::ImageLayout::eDepthAttachmentOptimal:
            return vk::AccessFlagBits2::eDepthStencilAttachmentRead | vk::AccessFlagBits2::eDepthStencilAttachmentWrite;
        case vk::ImageLayout::eFragmentShadingRateAttachmentOptimalKHR:
            return vk::AccessFlagBits2::eFragmentShadingRateAttachmentReadKHR;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eInputAttachmentRead;
        case vk::ImageLayout::eTransferSrcOptimal:
            return vk::AccessFlagBits2::eTransferRead;
        case vk::ImageLayout::eTransferDstOptimal:
            return vk::AccessFlagBits2::eTransferWrite;
        case vk::ImageLayout::eGeneral:
            assert(false && "Don't know how to get a meaningful VkAccessFlags for VK_IMAGE_LAYOUT_GENERAL! Don't use it!");
            return {};
        default:
            assert(false);
            return {};
        }
    }

    vk::PipelineStageFlags2 GetPipelineStageFlags2(const vk::ImageLayout& layout) {
        switch (layout)
        {
        case vk::ImageLayout::eUndefined:
            return vk::PipelineStageFlagBits2::eTopOfPipe;
        case vk::ImageLayout::ePreinitialized:
            return vk::PipelineStageFlagBits2::eHost;
        case vk::ImageLayout::eTransferDstOptimal:
        case vk::ImageLayout::eTransferSrcOptimal:
            return vk::PipelineStageFlagBits2::eTransfer;
        case vk::ImageLayout::eColorAttachmentOptimal:
            return vk::PipelineStageFlagBits2::eColorAttachmentOutput;
        case vk::ImageLayout::eDepthAttachmentOptimal:
            return vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests;
        case vk::ImageLayout::eFragmentShadingRateAttachmentOptimalKHR:
            return vk::PipelineStageFlagBits2::eFragmentShadingRateAttachmentKHR;
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader;
        case vk::ImageLayout::ePresentSrcKHR:
            return vk::PipelineStageFlagBits2::eBottomOfPipe;
        case vk::ImageLayout::eGeneral:
            assert(false && "Don't know how to get a meaningful VkPipelineStageFlags for VK_IMAGE_LAYOUT_GENERAL! Don't use it!");
            return {};
        default:
            assert(false);
            return {};
        }
    }

    [[nodiscard]] vk::PresentModeKHR ChooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    [[nodiscard]] vk::Extent2D ChooseSwapExtent(const vk::Extent2D extent, const vk::SurfaceCapabilitiesKHR& surfaceCapabilities) {
        if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            return surfaceCapabilities.currentExtent;
        }

        return {
            std::clamp<uint32_t>(extent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width),
            std::clamp<uint32_t>(extent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height)
        };
    }

    auto ChooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
        const auto preferredFormat = std::find_if(formats.begin(), formats.end(),
            [](const auto& format) {
                return format.format == vk::Format::eB8G8R8A8Unorm &&
                       format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear;
            });

        if (preferredFormat == formats.end()) {
            throw std::runtime_error("Failed to find suitable surface format");
        }
        return *preferredFormat;
    }

    vk::Format FindSupportedFormat(const GPUResources& gpu,const std::vector<vk::Format> &candidates, const vk::ImageTiling tiling, const vk::FormatFeatureFlags features) {
        for(const vk::Format format : candidates) {
            vk::FormatProperties props = gpu.physicalDevice->getFormatProperties(format);
            if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features) {
                return format;
            }
            if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format!");
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
                debug::log(debug::LogLevel::eWarning, "Missing extension: {}", req);
                return false;
            }
        }
        return true;
    }

    uint32_t ClampSurfaceImageCount( const uint32_t desiredImageCount, const uint32_t minImageCount, const uint32_t maxImageCount )
    {
        uint32_t imageCount = ( std::max )( desiredImageCount, minImageCount );
        // Some drivers report maxImageCount as 0, so only clamp to max if it is valid.
        if ( maxImageCount > 0 )
        {
            imageCount = ( std::min )( imageCount, maxImageCount );
        }
        return imageCount;
    }

    auto ConfigureQueueSharingMode(vk::SwapchainCreateInfoKHR& createInfo, const QueueFamilyIndices& indices) {
        uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};
        if (indices.graphicsFamily != indices.presentFamily) {
            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                     .setQueueFamilyIndexCount(2)
                     .setPQueueFamilyIndices(queueFamilyIndices);
        } else {
            createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
        }
    }

    vk::raii::Instance MakeInstance(const vk::raii::Context&        context, const GraphicDeviceCreateInfo& info) {
        std::vector<vk::ExtensionProperties> availableExtensions = context.enumerateInstanceExtensionProperties();

        std::vector<char const *> enabledExtensions;
        enabledExtensions.reserve( info.instanceExtensions.size() );
        for ( auto const & ext : info.instanceExtensions )
        {
            enabledExtensions.push_back( ext.data() );
        }

        if (!AreExtensionsSupported(enabledExtensions, availableExtensions))
            throw std::runtime_error("Required instance extensions are missing");

        vk::InstanceCreateInfo createInfo{};
        createInfo.setPApplicationInfo(&info.appInfo)
                .setEnabledExtensionCount(static_cast<uint32_t>(enabledExtensions.size()))
                .setPpEnabledExtensionNames(enabledExtensions.data());

        return {context, createInfo};
    }

    vk::raii::Instance MakeInstance(const GPUResources& gpu, const GraphicDeviceCreateInfo& info) {
        return MakeInstance(*gpu.context, info);
    }

    vk::raii::PhysicalDevice PickBestPhysicalDevice(const vk::raii::Instance& instance) {
        auto devices = vk::raii::PhysicalDevices(instance);
        if (devices.empty())
            throw std::runtime_error( "failed to find GPUs with Vulkan support!" );

        std::multimap<uint32_t, vk::raii::PhysicalDevice> candidates;

        for (const auto& device : devices) {
            auto deviceProperties = device.getProperties();
            auto deviceFeatures = device.getFeatures();

            // Skip if doesn't meet basic requirements
            if (deviceProperties.apiVersion < PhysicalDeviceRequirements::MINIMUM_API_VERSION ||
                !deviceFeatures.geometryShader){
                continue;
                }

            uint32_t score = 0;

            // Discrete GPUs have a significant performance advantage
            if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu){
                score += PhysicalDeviceRequirements::DISCRETE_GPU_SCORE_BONUS;
            }

            // Safely add scores with overflow check
            if (score <= UINT32_MAX - deviceProperties.limits.maxDescriptorSetUniformBuffersDynamic){
                score += deviceProperties.limits.maxDescriptorSetUniformBuffersDynamic;
            }

            if (score <= UINT32_MAX - deviceProperties.limits.maxImageDimension2D){
                score += deviceProperties.limits.maxImageDimension2D;
            }

            candidates.insert(std::make_pair(score, device));
        }

        if (candidates.empty()) {
            throw std::runtime_error("failed to find a suitable GPU!");
        }

        return std::move(candidates.rbegin()->second);
    }

    vk::raii::PhysicalDevice PickBestPhysicalDevice(const GPUResources& gpu) {
        return PickBestPhysicalDevice(*gpu.instance);
    }

    QueueFamilyIndices FindGraphicsAndPresentQueueFamilyIndex(const vk::raii::PhysicalDevice& physicalDevice, const windowing::WindowResource& surfaceData) {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();
        assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

        // get the first index into queueFamiliyProperties which supports graphics
        auto graphicsQueueFamilyProperty =
          std::find_if( queueFamilyProperties.begin(),
                        queueFamilyProperties.end(),
                        []( vk::QueueFamilyProperties const & qfp ) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics;});
        assert( graphicsQueueFamilyProperty != queueFamilyProperties.end());

        auto graphicsQueueFamilyIndex = static_cast<uint32_t>( std::distance( queueFamilyProperties.begin(), graphicsQueueFamilyProperty));
        if (physicalDevice.getSurfaceSupportKHR(graphicsQueueFamilyIndex, *surfaceData.surface))
        {
            return {graphicsQueueFamilyIndex,graphicsQueueFamilyIndex};  // the first graphicsQueueFamilyIndex does also support presents
        }

        // the graphicsQueueFamilyIndex doesn't support present -> look for an other family index that supports both
        // graphics and present
        for (size_t i = 0; i < queueFamilyProperties.size(); i++ )
        {
            if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics &&
                physicalDevice.getSurfaceSupportKHR( static_cast<uint32_t>(i), *surfaceData.surface) ) {
                return {static_cast<uint32_t>(i), static_cast<uint32_t>(i)};
                }
        }

        // there's nothing like a single family index that supports both graphics and present -> look for an other
        // family index that supports present
        for (size_t i = 0; i < queueFamilyProperties.size(); i++ )
        {
            if (physicalDevice.getSurfaceSupportKHR(static_cast<uint32_t>(i), *surfaceData.surface)){
                return { graphicsQueueFamilyIndex, static_cast<uint32_t>( i ) };
            }
        }

        throw std::runtime_error( "Could not find queues for both graphics or present -> terminating" );
    }

    QueueFamilyIndices FindGraphicsAndPresentQueueFamilyIndex(const GPUResources& gpu, const windowing::WindowResource& surfaceData) {
        return FindGraphicsAndPresentQueueFamilyIndex(*gpu.physicalDevice, surfaceData);
    }

    vk::raii::Device MakeDevice(const vk::raii::PhysicalDevice&       physicalDevice,
                                   const uint32_t                     queueFamilyIndex,
                                   const std::vector<std::string>&    extensions             = {},
                                   const vk::PhysicalDeviceFeatures*  physicalDeviceFeatures = nullptr,
                                   const void*                        pNext                  = nullptr )
    {
        std::vector<char const *> enabledExtensions;
        enabledExtensions.reserve( extensions.size() );
        for ( auto const & ext : extensions )
        {
            enabledExtensions.push_back( ext.data() );
        }

        float                     queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo( vk::DeviceQueueCreateFlags(), queueFamilyIndex, 1, &queuePriority );
        vk::DeviceCreateInfo      deviceCreateInfo( vk::DeviceCreateFlags(), deviceQueueCreateInfo, {}, enabledExtensions, physicalDeviceFeatures, pNext );
        return {physicalDevice, deviceCreateInfo};
    }

    vk::raii::Device MakeDevice(const GPUResources& gpu, const GraphicDeviceCreateInfo& info) {
        //root
        auto features2 = gpu.physicalDevice->getFeatures2();
        //1
        vk::PhysicalDeviceVulkan11Features vulkan11Features;
        vulkan11Features.setShaderDrawParameters(info.getEnableShaderDrawParameters());
        //2
        vk::PhysicalDeviceVulkan12Features vulkan12Features;
        vulkan12Features.setRuntimeDescriptorArray(true);
        //3
        vk::PhysicalDeviceVulkan13Features vulkan13Features;
        vulkan13Features.setDynamicRendering(info.getEnableDynamicRendering());
        vulkan13Features.setSynchronization2(info.getEnableSynchronization2());
        //4
        vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures;
        extendedDynamicStateFeatures.setExtendedDynamicState(info.getEnableExtendedDynamicState());

        //3 <-- 4
        vulkan13Features.pNext = &extendedDynamicStateFeatures;
        //2 <-- 3
        vulkan12Features.pNext = &vulkan13Features;
        //1 <-- 2
        vulkan11Features.pNext = &vulkan12Features;
        //root <-- 1
        features2.pNext = &vulkan11Features;

        return MakeDevice(*gpu.physicalDevice, gpu.queueFamilyIndices->graphicsFamily, info.deviceExtensions, nullptr, features2);
    }

    vk::raii::CommandPool MakeCommandPool(const vk::raii::Device& device, const QueueFamilyIndices& queueFamilyIndices,
        const vk::CommandPoolCreateFlags& flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient) {
        vk::CommandPoolCreateInfo createInfo{};
        createInfo.setQueueFamilyIndex(queueFamilyIndices.graphicsFamily)
                  .setFlags(flags);

        return {device, createInfo};
    }

    vk::raii::CommandPool MakeCommandPool(const GPUResources& gpu,
        const vk::CommandPoolCreateFlags& flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient) {
        return MakeCommandPool(*gpu.device, *gpu.queueFamilyIndices, flags);
    }

    vk::raii::CommandPool MakeCommandPool(const GPUResources& gpu, const GraphicDeviceCreateInfo& info) {
        return MakeCommandPool(*gpu.device, *gpu.queueFamilyIndices, info.commandPoolCreateFlags);
    }

    vk::raii::Queue MakeGraphicsQueue(const vk::raii::Device& device, const QueueFamilyIndices& queueFamilyIndices) {
        ufox::debug::log(debug::LogLevel::eInfo, "Retrieving graphics queue for queue family index: {}", queueFamilyIndices.graphicsFamily);

        return {device, queueFamilyIndices.graphicsFamily, 0};
    }

    vk::raii::Queue MakeGraphicsQueue(const GPUResources& gpu) {
        return MakeGraphicsQueue(*gpu.device, *gpu.queueFamilyIndices);
    }

    vk::raii::Queue MakePresentQueue(const vk::raii::Device& device, const QueueFamilyIndices& queueFamilyIndices) {

        return {device, queueFamilyIndices.presentFamily, 0};
    }

    vk::raii::Queue MakePresentQueue(const GPUResources& gpu) {
        return MakePresentQueue(*gpu.device, *gpu.queueFamilyIndices);
    }

    void ReMakeSwapchainResource(SwapchainResource& resource, const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Device& device, const vk::raii::SurfaceKHR& surface,
        const vk::Extent2D& extent, const vk::ImageUsageFlags usage, uint32_t graphicsQueueFamilyIndex, uint32_t presentQueueFamilyIndex) {

        vk::SurfaceCapabilitiesKHR surfaceCapabilities  = physicalDevice.getSurfaceCapabilitiesKHR( surface );
        vk::SurfaceFormatKHR surfaceFormat              = ChooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(*surface));
        resource.colorFormat                            = surfaceFormat.format;
        resource.extent                                 = ChooseSwapExtent(extent, surfaceCapabilities);
        vk::PresentModeKHR presentMode                  = ChooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(*surface));
        vk::SurfaceTransformFlagBitsKHR preTransform    = surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity ? vk::SurfaceTransformFlagBitsKHR::eIdentity : surfaceCapabilities.currentTransform;
        vk::CompositeAlphaFlagBitsKHR compositeAlpha    = surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied  ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
            :  surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
            : surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit ? vk::CompositeAlphaFlagBitsKHR::eInherit: vk::CompositeAlphaFlagBitsKHR::eOpaque;

        vk::SwapchainCreateInfoKHR createInfo{};
        createInfo.setSurface(*surface)
            .setMinImageCount(ClampSurfaceImageCount(3u, surfaceCapabilities.minImageCount, surfaceCapabilities.maxImageCount))
            .setImageFormat(resource.colorFormat )
            .setImageColorSpace(surfaceFormat.colorSpace) // Use colorSpace from surfaceFormat
            .setImageExtent(resource.extent)
            .setImageArrayLayers(1)
            .setImageUsage(usage)
            .setPreTransform(preTransform)
            .setCompositeAlpha(compositeAlpha)
            .setPresentMode(presentMode)
            .setClipped(true);

        if (graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
            uint32_t queueFamilyIndices[2] = { graphicsQueueFamilyIndex, presentQueueFamilyIndex };
            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(2)
                .setPQueueFamilyIndices(queueFamilyIndices);
        }
        else {
            createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
        }

        resource.swapChain.emplace(device, createInfo);

        resource.images = resource.swapChain->getImages();

        resource.imageViews.reserve( resource.images.size() );
        resource.renderFinishedSemaphores.reserve( resource.images.size() );

        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::ImageViewCreateInfo imageViewCreateInfo( {}, {}, vk::ImageViewType::e2D, resource.colorFormat, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } );

        for ( auto image : resource.images )
        {
            imageViewCreateInfo.image = image;
            resource.imageViews.emplace_back( device, imageViewCreateInfo );
            resource.renderFinishedSemaphores.emplace_back( device, semaphoreInfo );
        }
    }

    void ReMakeSwapchainResource(SwapchainResource& resource,const GPUResources& gpu, const windowing::WindowResource& surfaceData, vk::ImageUsageFlags usage) {
        ReMakeSwapchainResource(resource, *gpu.physicalDevice, *gpu.device, *surfaceData.surface, surfaceData.extent,
            usage, gpu.queueFamilyIndices->graphicsFamily, gpu.queueFamilyIndices->presentFamily);
    }

    SwapchainResource MakeSwapchainResource(const vk::raii::PhysicalDevice& physicalDevice, const vk::raii::Device& device, const vk::raii::SurfaceKHR& surface,
        const vk::Extent2D& extent, const vk::ImageUsageFlags usage, uint32_t graphicsQueueFamilyIndex, uint32_t presentQueueFamilyIndex) {
        SwapchainResource resource{};
        ReMakeSwapchainResource(resource, physicalDevice, device, surface, extent, usage, graphicsQueueFamilyIndex, presentQueueFamilyIndex);
        return resource;
    }

    SwapchainResource MakeSwapchainResource(const GPUResources& gpu, const windowing::WindowResource& surfaceData, const vk::ImageUsageFlags usage) {
        return MakeSwapchainResource(*gpu.physicalDevice, *gpu.device, *surfaceData.surface, surfaceData.extent,
            usage, gpu.queueFamilyIndices->graphicsFamily, gpu.queueFamilyIndices->presentFamily);
    }

    FrameResource MakeFrameResource(const vk::raii::Device& device, const vk::raii::CommandPool& commandPool, const vk::FenceCreateFlags& fenceFlags) {
        FrameResource resource{};
        vk::CommandBufferAllocateInfo commandBufferAllocateInfo(commandPool, vk::CommandBufferLevel::ePrimary, FrameResource::MAX_FRAMES_IN_FLIGHT);
        resource.commandBuffers = device.allocateCommandBuffers(commandBufferAllocateInfo);

        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{ fenceFlags };

        resource.presentCompleteSemaphores.reserve(FrameResource::MAX_FRAMES_IN_FLIGHT);
        resource.drawFences.reserve(FrameResource::MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < FrameResource::MAX_FRAMES_IN_FLIGHT; ++i) {
            resource.presentCompleteSemaphores.emplace_back(device, semaphoreInfo);
            resource.drawFences.emplace_back(device, fenceInfo);
        }

        resource.currentFrameIndex = 0;

        return resource;
    }

    void CreateImage(const GPUResources& gpu,vk::Extent2D extent, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties, std::optional<vk::raii::Image> &image, std::optional<vk::raii::DeviceMemory> &imageMemory){
        vk::ImageCreateInfo imageInfo{};
        imageInfo
            .setImageType(vk::ImageType::e2D)
            .setFormat(format)
            .setExtent({ extent.width, extent.height, 1 })
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(tiling)
            .setUsage(usage)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setInitialLayout(vk::ImageLayout::eUndefined);

        image.emplace(*gpu.device, imageInfo);

        vk::MemoryRequirements memRequirements = image->getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo
            .setAllocationSize(memRequirements.size)
            .setMemoryTypeIndex(FindMemoryType(gpu.physicalDevice.value().getMemoryProperties(), memRequirements.memoryTypeBits, properties));

        imageMemory.emplace(*gpu.device, allocInfo);
        image->bindMemory(imageMemory.value(), 0);
    }

    void CreateImageView(const GPUResources& gpu, std::optional<vk::raii::ImageView>& view,vk::raii::Image &image, vk::Format format, vk::ImageAspectFlags aspectFlags){
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo
            .setImage(image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(format)
            .setSubresourceRange({aspectFlags, 0, 1, 0, 1});
        view.emplace(*gpu.device, viewInfo);
    }

    FrameResource MakeFrameResource(const GPUResources& gpu, const vk::FenceCreateFlags& fenceFlags) {
        return MakeFrameResource(*gpu.device, *gpu.commandPool, fenceFlags);
    }

    vk::Format FindDepthFormat(const GPUResources& gpu) {
        return FindSupportedFormat(gpu,{vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},vk::ImageTiling::eOptimal,vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }

    bool HasStencilComponent(vk::Format format) {
        return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
    }

    void MakeDepthImage(const GPUResources& gpu, SwapchainResource& swapchainResource) {
        swapchainResource.depthFormat = FindDepthFormat(gpu);
        CreateImage(gpu, swapchainResource.extent, swapchainResource.depthFormat, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal, swapchainResource.depthImage, swapchainResource.depthImageMemory);
        CreateImageView(gpu, swapchainResource.depthImageView, *swapchainResource.depthImage, swapchainResource.depthFormat, vk::ImageAspectFlagBits::eDepth);
    }

    using DescriptorSetLayoutBinding = std::tuple<vk::DescriptorType,uint32_t,vk::ShaderStageFlags, vk::Sampler*>;

    [[nodiscard]] DescriptorSetLayoutBinding MakeUniformBufferLayoutBinding(const uint32_t& maxSize = 1, const vk::ShaderStageFlags& flags = vk::ShaderStageFlagBits::eVertex) noexcept {
        return {vk::DescriptorType::eUniformBuffer,maxSize,flags,nullptr};
    }

    [[nodiscard]] DescriptorSetLayoutBinding MakeStorageBufferLayoutBinding(const uint32_t& maxSize = 1, const vk::ShaderStageFlags& flags = vk::ShaderStageFlagBits::eVertex) noexcept {
        return {vk::DescriptorType::eStorageBuffer,maxSize,flags,nullptr};
    }

    [[nodiscard]] DescriptorSetLayoutBinding MakeCombinedImageSamplerLayoutBinding(const uint32_t& maxSize = 1, const vk::ShaderStageFlags& flags = vk::ShaderStageFlagBits::eFragment) noexcept {
        return {vk::DescriptorType::eCombinedImageSampler,maxSize,flags,nullptr};
    }

    constexpr vk::raii::DescriptorSetLayout MakeDescriptorSetLayout(const GPUResources& gpu,const std::vector<DescriptorSetLayoutBinding>& bindingData,
        const vk::DescriptorSetLayoutCreateFlags flags = {} )
    {
        std::vector<vk::DescriptorSetLayoutBinding> bindings( bindingData.size() );
        for ( size_t i = 0; i < bindingData.size(); i++ )
        {
            bindings[i] = vk::DescriptorSetLayoutBinding{}
            .setBinding( utilities::CheckedCast<uint32_t>( i ) )
            .setDescriptorType( std::get<0>( bindingData[i] ) )
            .setDescriptorCount( std::get<1>( bindingData[i] ) )
            .setStageFlags( std::get<2>( bindingData[i] ) )
            .setPImmutableSamplers( std::get<3>(bindingData[i]) );
        }
        vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo( flags, bindings );
        return {*gpu.device, descriptorSetLayoutCreateInfo };
    }

    vk::raii::DescriptorPool MakeDescriptorPool( const GPUResources& gpu, std::vector<vk::DescriptorPoolSize> const & poolSizes ) {
        assert( !poolSizes.empty() );
        const uint32_t maxSets = std::accumulate(
          poolSizes.begin(), poolSizes.end(), 0, [](const uint32_t sum, vk::DescriptorPoolSize const & dps ) { return sum + dps.descriptorCount; } );
        assert( 0 < maxSets );

        vk::DescriptorPoolCreateInfo descriptorPoolCreateInfo( vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, maxSets, poolSizes );
        return { *gpu.device, descriptorPoolCreateInfo };
    }

    vk::raii::ShaderModule CreateShaderModule(const GPUResources& gpu, const std::vector<char>& code) {
        vk::ShaderModuleCreateInfo createInfo{};
        createInfo
            .setCodeSize(code.size() * sizeof(char))
            .setPCode(reinterpret_cast<const uint32_t*>(code.data()));

        vk::raii::ShaderModule shaderModule{ *gpu.device, createInfo };

        return shaderModule;
    }

    vk::PipelineVertexInputStateCreateInfo MakePipeVertexInputState(
            std::span<const vk::VertexInputBindingDescription> bindings,
            std::span<const vk::VertexInputAttributeDescription> attributes) {
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo
            .setVertexBindingDescriptionCount(bindings.size())
            .setVertexAttributeDescriptionCount(attributes.size())
            .setVertexBindingDescriptions(bindings)
            .setVertexAttributeDescriptions(attributes);
        return vertexInputInfo;
    }


    vk::raii::Pipeline makeGraphicsPipeline( vk::raii::Device const &                             device,
                                               vk::raii::PipelineCache const &                      pipelineCache,
                                               vk::raii::ShaderModule const &                       vertexShaderModule,
                                               vk::SpecializationInfo const *                       vertexShaderSpecializationInfo,
                                               vk::raii::ShaderModule const &                       fragmentShaderModule,
                                               vk::SpecializationInfo const *                       fragmentShaderSpecializationInfo,
                                               uint32_t                                             vertexStride,
                                               std::vector<std::pair<vk::Format, uint32_t>> const & vertexInputAttributeFormatOffset,
                                               vk::FrontFace                                        frontFace,
                                               bool                                                 depthBuffered,
                                               vk::raii::PipelineLayout const &                     pipelineLayout,
                                               vk::raii::RenderPass const &                         renderPass )
    {
        std::array pipelineShaderStageCreateInfos = {
            vk::PipelineShaderStageCreateInfo( {}, vk::ShaderStageFlagBits::eVertex, vertexShaderModule, "main", vertexShaderSpecializationInfo ),
            vk::PipelineShaderStageCreateInfo( {}, vk::ShaderStageFlagBits::eFragment, fragmentShaderModule, "main", fragmentShaderSpecializationInfo )
          };

        std::vector<vk::VertexInputAttributeDescription> vertexInputAttributeDescriptions;
        vk::PipelineVertexInputStateCreateInfo           pipelineVertexInputStateCreateInfo;
        vk::VertexInputBindingDescription                vertexInputBindingDescription( 0, vertexStride );

        if ( 0 < vertexStride )
        {
            vertexInputAttributeDescriptions.reserve( vertexInputAttributeFormatOffset.size() );
            for ( uint32_t i = 0; i < vertexInputAttributeFormatOffset.size(); i++ )
            {
                vertexInputAttributeDescriptions.emplace_back( i, 0, vertexInputAttributeFormatOffset[i].first, vertexInputAttributeFormatOffset[i].second );
            }
            pipelineVertexInputStateCreateInfo.setVertexBindingDescriptions( vertexInputBindingDescription );
            pipelineVertexInputStateCreateInfo.setVertexAttributeDescriptions( vertexInputAttributeDescriptions );
        }

        vk::PipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCreateInfo( vk::PipelineInputAssemblyStateCreateFlags(),
                                                                                       vk::PrimitiveTopology::eTriangleList );

        vk::PipelineViewportStateCreateInfo pipelineViewportStateCreateInfo( vk::PipelineViewportStateCreateFlags(), 1, nullptr, 1, nullptr );

        vk::PipelineRasterizationStateCreateInfo pipelineRasterizationStateCreateInfo( vk::PipelineRasterizationStateCreateFlags(),
                                                                                       false,
                                                                                       false,
                                                                                       vk::PolygonMode::eFill,
                                                                                       vk::CullModeFlagBits::eBack,
                                                                                       frontFace,
                                                                                       false,
                                                                                       0.0f,
                                                                                       0.0f,
                                                                                       0.0f,
                                                                                       1.0f );

        vk::PipelineMultisampleStateCreateInfo pipelineMultisampleStateCreateInfo( {}, vk::SampleCountFlagBits::e1 );

        vk::StencilOpState                      stencilOpState( vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::StencilOp::eKeep, vk::CompareOp::eAlways );
        vk::PipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCreateInfo(
          vk::PipelineDepthStencilStateCreateFlags(), depthBuffered, depthBuffered, vk::CompareOp::eLessOrEqual, false, false, stencilOpState, stencilOpState );

        vk::ColorComponentFlags colorComponentFlags( vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                                                     vk::ColorComponentFlagBits::eA );
        vk::PipelineColorBlendAttachmentState pipelineColorBlendAttachmentState( false,
                                                                                 vk::BlendFactor::eZero,
                                                                                 vk::BlendFactor::eZero,
                                                                                 vk::BlendOp::eAdd,
                                                                                 vk::BlendFactor::eZero,
                                                                                 vk::BlendFactor::eZero,
                                                                                 vk::BlendOp::eAdd,
                                                                                 colorComponentFlags );
        vk::PipelineColorBlendStateCreateInfo pipelineColorBlendStateCreateInfo(
          vk::PipelineColorBlendStateCreateFlags(), false, vk::LogicOp::eNoOp, pipelineColorBlendAttachmentState, { { 1.0f, 1.0f, 1.0f, 1.0f } } );

        std::array    dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };
        vk::PipelineDynamicStateCreateInfo pipelineDynamicStateCreateInfo( vk::PipelineDynamicStateCreateFlags(), dynamicStates );

        vk::GraphicsPipelineCreateInfo graphicsPipelineCreateInfo( vk::PipelineCreateFlags(),
                                                                   pipelineShaderStageCreateInfos,
                                                                   &pipelineVertexInputStateCreateInfo,
                                                                   &pipelineInputAssemblyStateCreateInfo,
                                                                   nullptr,
                                                                   &pipelineViewportStateCreateInfo,
                                                                   &pipelineRasterizationStateCreateInfo,
                                                                   &pipelineMultisampleStateCreateInfo,
                                                                   &pipelineDepthStencilStateCreateInfo,
                                                                   &pipelineColorBlendStateCreateInfo,
                                                                   &pipelineDynamicStateCreateInfo,
                                                                   pipelineLayout,
                                                                   renderPass );

        return { device, pipelineCache, graphicsPipelineCreateInfo };
    }

    vk::raii::CommandBuffer BeginSingleTimeCommands(const GPUResources& gpu) {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.setLevel(vk::CommandBufferLevel::ePrimary)
                 .setCommandPool(gpu.commandPool.value())
                 .setCommandBufferCount(1);

        vk::raii::CommandBuffer cmd = std::move(gpu.device.value().allocateCommandBuffers(allocInfo).front());
        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

        cmd.begin(beginInfo);

        return cmd;
    }

    void EndSingleTimeCommands(const vk::raii::CommandBuffer &cmd, const vk::raii::Queue& graphicsQueue) {
        cmd.end();
        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBufferCount(1)
                 .setPCommandBuffers(&*cmd);
        graphicsQueue.submit(submitInfo, nullptr);
        graphicsQueue.waitIdle();
    }

    void EndSingleTimeCommands(const GPUResources& gpu, const vk::raii::CommandBuffer &cmd) {
        EndSingleTimeCommands(cmd, *gpu.graphicsQueue);
    }

    void MakeBuffer(Buffer& buffer,const GPUResources& gpu, const vk::DeviceSize& size, const vk::BufferUsageFlags& usage, const vk::MemoryPropertyFlags& properties, const vk::SharingMode& shareMode = vk::SharingMode::eExclusive) noexcept {
        if (!gpu.device || !gpu.physicalDevice || size == 0 || !usage) return ;

        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.setSize(size)
                  .setUsage(usage)
                  .setSharingMode(shareMode);

        buffer.data.emplace(*gpu.device, bufferInfo);

        const vk::MemoryRequirements reqs = buffer.data->getMemoryRequirements();
        buffer.memory.emplace(AllocateDeviceMemory(
            *gpu.device,
            *gpu.physicalDevice->getMemoryProperties(),
            reqs,
            properties));

        buffer.data->bindMemory(*buffer.memory, 0);
    }

    void MakeBuffer(std::optional<Buffer>& buffer,const GPUResources& gpu, const vk::DeviceSize& size, const vk::BufferUsageFlags& usage, const vk::MemoryPropertyFlags& properties, const vk::SharingMode& shareMode = vk::SharingMode::eExclusive) {
        Buffer rawBuffer{};
        MakeBuffer(rawBuffer, gpu, size, usage, properties, shareMode);
        buffer.emplace(std::move(rawBuffer));
    }


    template <typename T>
    void CopyToDevice(const vk::raii::DeviceMemory& deviceMemory,const T* pData, const vk::DeviceSize& size ) {
        void* data = deviceMemory.mapMemory(0, size);
        memcpy(data, pData, size);
        deviceMemory.unmapMemory();
    }


    void CopyBuffer(const GPUResources& gpu,  const Buffer& srcBuffer, const Buffer& dstBuffer, const vk::DeviceSize& size) {
        vk::raii::CommandBuffer cmd = BeginSingleTimeCommands(gpu);

        vk::BufferCopy copyRegion{};
        copyRegion.setSize(size);
        cmd.copyBuffer(*srcBuffer.data, *dstBuffer.data, { copyRegion });

        EndSingleTimeCommands(gpu, cmd);
    }

    template <typename T>
    void MakeAndCopyBuffer(const GPUResources& gpu, std::span<const T> sources, const vk::BufferUsageFlags& finalUsage, std::optional<Buffer>& destinationBuffer) {
        if (sources.empty()) return;

        if (destinationBuffer.has_value()) {
            destinationBuffer.reset();
        }

        const vk::DeviceSize bufferSize = sizeof(T) * sources.size();

        std::optional<Buffer> stagingBuffer{};
        MakeBuffer(
            stagingBuffer,
            gpu,
            bufferSize,
            vk::BufferUsageFlagBits::eTransferSrc,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        CopyToDevice(stagingBuffer->memory.value(), sources.data(), bufferSize);

        MakeBuffer(
            destinationBuffer,
            gpu,
            bufferSize,
            finalUsage | vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        CopyBuffer(gpu, stagingBuffer.value(), destinationBuffer.value(), bufferSize);
    }

    template <typename T, size_t N>
    void MakeAndCopyBuffer(const GPUResources& gpu,const T (&array)[N], const vk::BufferUsageFlags& finalUsage, std::optional<Buffer>& destinationBuffer){
        MakeAndCopyBuffer(gpu, std::span<const T>{array}, finalUsage, destinationBuffer);
    }

    template <typename T, size_t N>
    void MakeAndCopyBuffer(const GPUResources& gpu, const std::array<T, N>& arr, const vk::BufferUsageFlags& finalUsage, std::optional<Buffer>& destinationBuffer){
        MakeAndCopyBuffer(gpu, std::span<const T>{arr}, finalUsage, destinationBuffer);
    }

    template <typename T>
    void MakeAndCopyBuffer(const GPUResources& gpu,const std::vector<T>& vec, const vk::BufferUsageFlags& finalUsage, std::optional<Buffer>& destinationBuffer){
        MakeAndCopyBuffer(gpu, std::span<const T>{vec}, finalUsage, destinationBuffer);
    }

    template <typename T>
    void MakeAndCopyBuffer(const GPUResources& gpu,const T& source, const vk::BufferUsageFlags& finalUsage, std::optional<Buffer>& destinationBuffer) {
        MakeAndCopyBuffer(gpu, std::span<const T>{&source, 1}, finalUsage, destinationBuffer);
    }

    GraphicDeviceCreateInfo CreateGraphicDeviceInfo() {
        GraphicDeviceCreateInfo createInfo{};
        createInfo.appInfo.pApplicationName = "UFox";
        createInfo.appInfo.applicationVersion = vk::makeVersion(1, 0, 0);
        createInfo.appInfo.pEngineName = "UFox Engine";
        createInfo.appInfo.engineVersion = vk::makeVersion(1, 0, 0);
        createInfo.appInfo.apiVersion = vk::ApiVersion14;
        createInfo.instanceExtensions = GetInstanceExtensions();
        createInfo.deviceExtensions = GetDeviceExtensions();
        createInfo.enableDynamicRendering = true;
        createInfo.enableExtendedDynamicState = true;
        createInfo.enableSynchronization2 = true;
        createInfo.commandPoolCreateFlags = vk::CommandPoolCreateFlags{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient
        };

        return createInfo;
    }

    [[nodiscard]] vk::WriteDescriptorSet MakeBufferWriteDescriptorSet(const vk::raii::DescriptorSet& set, const vk::DescriptorBufferInfo& info,
    const uint32_t binding, const vk::DescriptorType type, const uint32_t descriptorCount = 1, const uint32_t arrayElement = 0) {
        vk::WriteDescriptorSet writeDescriptorSet{};
        writeDescriptorSet
          .setDstSet(*set)
          .setDstBinding(binding)
          .setDstArrayElement(arrayElement)
          .setDescriptorType(type)
          .setDescriptorCount(descriptorCount)
          .setPBufferInfo(&info);
        return writeDescriptorSet;
    }

    inline auto COLOR_IMAGE_USAGE_FLAG = vk::ImageUsageFlags{ vk::ImageUsageFlagBits::eColorAttachment };
    inline auto SIGNALED_FENCE_CREATE_FLAG = vk::FenceCreateFlags{vk::FenceCreateFlagBits::eSignaled};

    void TransitionImageLayout(vk::raii::CommandBuffer const & commandBuffer, const vk::Image& image, const vk::Format& format, const vk::ImageSubresourceRange& range, const vk::ImageLayout oldImageLayout, const vk::ImageLayout newImageLayout ) {
        const vk::AccessFlags2 sourceAccessMask = GetAccessFlags2(oldImageLayout);
        const vk::PipelineStageFlags2 sourceStage = GetPipelineStageFlags2(oldImageLayout);
        const vk::AccessFlags2 destinationAccessMask = GetAccessFlags2(newImageLayout);
        const vk::PipelineStageFlags2 destinationStage = GetPipelineStageFlags2(newImageLayout);

        vk::ImageAspectFlags aspectMask;
        if ( newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal ){
            aspectMask = vk::ImageAspectFlagBits::eDepth;
            if ( format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint ){
                aspectMask |= vk::ImageAspectFlagBits::eStencil;
            }
        }
        else{
            aspectMask = vk::ImageAspectFlagBits::eColor;
        }

        vk::ImageMemoryBarrier2  imageMemoryBarrier{};
        imageMemoryBarrier
            .setSrcStageMask( sourceStage )
            .setDstStageMask( destinationStage )
            .setSrcAccessMask( sourceAccessMask )
            .setDstAccessMask( destinationAccessMask )
            .setOldLayout( oldImageLayout )
            .setNewLayout( newImageLayout )
            .setSrcQueueFamilyIndex( VK_QUEUE_FAMILY_IGNORED )
            .setDstQueueFamilyIndex( VK_QUEUE_FAMILY_IGNORED )
            .setImage( image )
            .setSubresourceRange( range );

        vk::DependencyInfo dependencyInfo{};
        dependencyInfo.setImageMemoryBarrierCount(1)
        .setPImageMemoryBarriers(&imageMemoryBarrier);

        return commandBuffer.pipelineBarrier2(dependencyInfo);
    }
}
