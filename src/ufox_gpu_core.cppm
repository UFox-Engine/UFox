module;

#if defined( _WIN32 )
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined( __linux__ )
#define VK_USE_PLATFORM_XLIB_KHR
#elif defined( __APPLE__ )
#define VK_USE_PLATFORM_MACOS_MVK
#endif

#include <map>
#include <numeric>
#include <vulkan/vulkan_raii.hpp>

#ifdef USE_SDL

#include <SDL3/SDL.h>

#include <SDL3/SDL_vulkan.h>

#include <SDL3_image/SDL_image.h>

#else

#include <GLFW/glfw3.h>

#endif

export module ufox_gpu_core;

import ufox_lib;
import ufox_gpu_lib;

export namespace ufox::gpu {
    uint32_t FindMemoryType(const GPUResources& gpu, const uint32_t typeFilter, const vk::MemoryPropertyFlags properties) {
        vk::PhysicalDeviceMemoryProperties memProperties = gpu.physicalDevice->getMemoryProperties();

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++){
            if (typeFilter & 1 << i && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)return i;
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    vk::raii::DeviceMemory AllocateDeviceMemory(const GPUResources& gpu, const vk::MemoryRequirements& memoryRequirements, const vk::MemoryPropertyFlags memoryPropertyFlags ) {
        const uint32_t memoryTypeIndex = FindMemoryType( gpu, memoryRequirements.memoryTypeBits, memoryPropertyFlags );
        vk::MemoryAllocateInfo memoryAllocateInfo( memoryRequirements.size, memoryTypeIndex );
        return {*gpu.device, memoryAllocateInfo};
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
        const auto preferredFormat = std::ranges::find_if(formats,
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

    uint32_t ClampSurfaceImageCount( const uint32_t desiredImageCount, const uint32_t minImageCount, const uint32_t maxImageCount ){
        uint32_t imageCount = ( std::max )( desiredImageCount, minImageCount );

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

    void MakeInstance(GPUResources& gpu, const GraphicDeviceCreateInfo& info) {
        std::vector<vk::ExtensionProperties> availableExtensions = gpu.context->enumerateInstanceExtensionProperties();

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

        gpu.instance.emplace(*gpu.context, createInfo);
    }

    void PickBestPhysicalDevice(GPUResources& gpu) {
        auto devices = vk::raii::PhysicalDevices(*gpu.instance);
        if (devices.empty())
            throw std::runtime_error( "failed to find GPUs with Vulkan support!" );

        std::multimap<uint32_t, vk::raii::PhysicalDevice> candidates;

        for (const auto& device : devices) {
            auto deviceProperties = device.getProperties();
            auto deviceFeatures = device.getFeatures();

            if (deviceProperties.apiVersion < PhysicalDeviceRequirements::MINIMUM_API_VERSION ||
                !deviceFeatures.geometryShader){
                continue;
                }

            uint32_t score = 0;

            if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu){
                score += PhysicalDeviceRequirements::DISCRETE_GPU_SCORE_BONUS;
            }

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

        gpu.physicalDevice.emplace(std::move(candidates.rbegin()->second));
    }


    void FindGraphicsAndPresentQueueFamilyIndex(GPUResources& gpu, const WindowResource& surfaceData) {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = gpu.physicalDevice->getQueueFamilyProperties();
        assert(queueFamilyProperties.size() < (std::numeric_limits<uint32_t>::max)());

        auto graphicsQueueFamilyProperty = std::find_if( queueFamilyProperties.begin(),queueFamilyProperties.end(),
                        []( vk::QueueFamilyProperties const & qfp ) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics;});
        assert( graphicsQueueFamilyProperty != queueFamilyProperties.end());

        auto graphicsQueueFamilyIndex = static_cast<uint32_t>( std::distance( queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

        if (gpu.physicalDevice->getSurfaceSupportKHR(graphicsQueueFamilyIndex, *surfaceData.surface)){
            gpu.queueFamilyIndices.emplace(graphicsQueueFamilyIndex,graphicsQueueFamilyIndex);  // the first graphicsQueueFamilyIndex does also support presents
            return;
        }

        for (size_t i = 0; i < queueFamilyProperties.size(); i++ ){
            if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eGraphics &&gpu.physicalDevice->getSurfaceSupportKHR( static_cast<uint32_t>(i), *surfaceData.surface) ) {
                gpu.queueFamilyIndices.emplace(static_cast<uint32_t>(i), static_cast<uint32_t>(i));
                return;
            }
        }

        for (size_t i = 0; i < queueFamilyProperties.size(); i++ ){
            if (gpu.physicalDevice->getSurfaceSupportKHR(static_cast<uint32_t>(i), *surfaceData.surface)){
                gpu.queueFamilyIndices.emplace (graphicsQueueFamilyIndex, static_cast<uint32_t>( i ));
                return;
            }
        }

        throw std::runtime_error( "Could not find queues for both graphics or present -> terminating" );
    }

    void MakeDevice(GPUResources& gpu, const GraphicDeviceCreateInfo& info) {
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

        std::vector<char const *> enabledExtensions;
        enabledExtensions.reserve( info.deviceExtensions.size() );
        for ( auto const & ext : info.deviceExtensions ){
            enabledExtensions.push_back( ext.data() );
        }

        float                     queuePriority = 0.0f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo( vk::DeviceQueueCreateFlags(), gpu.queueFamilyIndices->graphicsFamily, 1, &queuePriority );
        vk::DeviceCreateInfo      deviceCreateInfo( vk::DeviceCreateFlags(), deviceQueueCreateInfo, {}, enabledExtensions, nullptr, features2 );
        gpu.device.emplace(*gpu.physicalDevice, deviceCreateInfo);
    }

    void MakeCommandPool(GPUResources& gpu, const vk::CommandPoolCreateFlags& flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient) {
        vk::CommandPoolCreateInfo createInfo{};
        createInfo.setQueueFamilyIndex(gpu.queueFamilyIndices->graphicsFamily)
                  .setFlags(flags);

        gpu.commandPool.emplace(*gpu.device, createInfo);
    }

    void MakeGraphicsQueue(GPUResources& gpu) {
        gpu.graphicsQueue.emplace(*gpu.device, gpu.queueFamilyIndices->graphicsFamily, 0);
    }

    void MakePresentQueue(GPUResources& gpu) {
        gpu.presentQueue.emplace(*gpu.device, gpu.queueFamilyIndices->presentFamily, 0);
    }

    void ReMakeSwapchainResource(SwapchainResource& resource, const GPUResources& gpu,  const WindowResource& surfaceData, const vk::ImageUsageFlags usage) {

        vk::SurfaceCapabilitiesKHR surfaceCapabilities  = gpu.physicalDevice->getSurfaceCapabilitiesKHR( *surfaceData.surface );
        vk::SurfaceFormatKHR surfaceFormat              = ChooseSwapSurfaceFormat(gpu.physicalDevice->getSurfaceFormatsKHR(surfaceData.surface.value()));
        resource.colorFormat                            = surfaceFormat.format;
        resource.extent                                 = ChooseSwapExtent(surfaceData.extent, surfaceCapabilities);
        vk::PresentModeKHR presentMode                  = ChooseSwapPresentMode(gpu.physicalDevice->getSurfacePresentModesKHR(surfaceData.surface.value()));
        vk::SurfaceTransformFlagBitsKHR preTransform    = surfaceCapabilities.supportedTransforms & vk::SurfaceTransformFlagBitsKHR::eIdentity ? vk::SurfaceTransformFlagBitsKHR::eIdentity : surfaceCapabilities.currentTransform;
        vk::CompositeAlphaFlagBitsKHR compositeAlpha    = surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePreMultiplied  ? vk::CompositeAlphaFlagBitsKHR::ePreMultiplied
            :  surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::ePostMultiplied ? vk::CompositeAlphaFlagBitsKHR::ePostMultiplied
            : surfaceCapabilities.supportedCompositeAlpha & vk::CompositeAlphaFlagBitsKHR::eInherit ? vk::CompositeAlphaFlagBitsKHR::eInherit: vk::CompositeAlphaFlagBitsKHR::eOpaque;

        vk::SwapchainCreateInfoKHR createInfo{};
        createInfo.setSurface(surfaceData.surface.value())
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

        if (gpu.queueFamilyIndices->graphicsFamily != gpu.queueFamilyIndices->presentFamily) {
            uint32_t queueFamilyIndices[2] = { gpu.queueFamilyIndices->graphicsFamily, gpu.queueFamilyIndices->presentFamily };
            createInfo.setImageSharingMode(vk::SharingMode::eConcurrent)
                .setQueueFamilyIndexCount(2)
                .setPQueueFamilyIndices(queueFamilyIndices);
        }
        else {
            createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
        }

        resource.swapChain.emplace(*gpu.device, createInfo);

        resource.images = resource.swapChain->getImages();

        resource.imageViews.reserve( resource.images.size() );
        resource.renderFinishedSemaphores.reserve( resource.images.size() );

        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::ImageViewCreateInfo imageViewCreateInfo( {}, {}, vk::ImageViewType::e2D, resource.colorFormat, {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 } );

        for ( auto image : resource.images )
        {
            imageViewCreateInfo.image = image;
            resource.imageViews.emplace_back( *gpu.device, imageViewCreateInfo );
            resource.renderFinishedSemaphores.emplace_back( *gpu.device, semaphoreInfo );
        }
    }

    void MakeSwapchainResource(std::optional<SwapchainResource>& swapChain,const GPUResources& gpu, const WindowResource& surfaceData, const vk::ImageUsageFlags usage) {
        SwapchainResource resource{};
        ReMakeSwapchainResource(resource, gpu, surfaceData, usage);
        swapChain.emplace(std::move(resource));
    }

    void MakeFrameResource(std::optional<FrameResource>& frameRes, const GPUResources& gpu, const vk::FenceCreateFlags& fenceFlags) {
        FrameResource resource{};
        vk::CommandBufferAllocateInfo commandBufferAllocateInfo(*gpu.commandPool, vk::CommandBufferLevel::ePrimary, FrameResource::MAX_FRAMES_IN_FLIGHT);
        resource.commandBuffers = gpu.device->allocateCommandBuffers(commandBufferAllocateInfo);

        vk::SemaphoreCreateInfo semaphoreInfo{};
        vk::FenceCreateInfo fenceInfo{ fenceFlags };

        resource.presentCompleteSemaphores.reserve(FrameResource::MAX_FRAMES_IN_FLIGHT);
        resource.drawFences.reserve(FrameResource::MAX_FRAMES_IN_FLIGHT);

        for (uint32_t i = 0; i < FrameResource::MAX_FRAMES_IN_FLIGHT; ++i) {
            resource.presentCompleteSemaphores.emplace_back(*gpu.device, semaphoreInfo);
            resource.drawFences.emplace_back(*gpu.device, fenceInfo);
        }

        resource.currentFrameIndex = 0;

        frameRes.emplace(std::move(resource));
    }

    void MakeImage(const GPUResources& gpu, Image& image, const vk::Extent3D extent, const vk::Format format,
        const vk::ImageTiling tiling, const vk::ImageUsageFlags usage, const vk::MemoryPropertyFlags properties,
        const vk::ImageType type = vk::ImageType::e2D, const vk::SharingMode shareMode = vk::SharingMode::eExclusive){
        vk::ImageCreateInfo imageInfo{};
        imageInfo
            .setImageType(type)
            .setFormat(format)
            .setExtent(extent)
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(tiling)
            .setUsage(usage)
            .setSharingMode(shareMode)
            .setInitialLayout(vk::ImageLayout::eUndefined);

        image.data.emplace(*gpu.device, imageInfo);

        const vk::MemoryRequirements memRequirements = image.data->getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{};
        allocInfo.setAllocationSize(memRequirements.size).setMemoryTypeIndex(FindMemoryType(gpu,memRequirements.memoryTypeBits, properties));
        image.memory.emplace(*gpu.device, allocInfo);
        image.data->bindMemory(*image.memory, 0);
    }

    void MakeImageView(GPUResources const & gpu, Image& image, const vk::Format format, const vk::ImageViewType type = vk::ImageViewType::e2D, const vk::ImageSubresourceRange & subresourceRange = DEFAULT_COLOR_SUBRESOURCE_RANGE) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo
            .setImage(*image.data)
            .setViewType(type)
            .setFormat(format)
            .setSubresourceRange(subresourceRange);
        image.view.emplace(gpu.device.value(), viewInfo);
    }

    void MakeSampler(const GPUResources & gpu, std::optional<vk::raii::Sampler>& sampler){
        const auto deviceProperties = gpu.physicalDevice->getProperties();

        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.setMagFilter(vk::Filter::eLinear)
                    .setMinFilter(vk::Filter::eLinear)
                    .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                    .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                    .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                    .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                    .setAnisotropyEnable(false)
                    .setMaxAnisotropy(deviceProperties.limits.maxSamplerAnisotropy)
                    .setUnnormalizedCoordinates(false)
                    .setCompareEnable(false)
                    .setCompareOp(vk::CompareOp::eAlways)
                    .setMinLod(0.0f)
                    .setMaxLod(0.0f)
                    .setMipLodBias(0.0f);

        sampler.emplace(gpu.device.value(), samplerInfo);
    }

    vk::Format FindDepthFormat(const GPUResources& gpu) {
        return FindSupportedFormat(gpu,{vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},vk::ImageTiling::eOptimal,vk::FormatFeatureFlagBits::eDepthStencilAttachment);
    }

    bool HasStencilComponent(vk::Format format) {
        return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
    }

    void MakeDepthImage(const GPUResources& gpu, std::optional<DepthImageResource>& depthRes, SwapchainResource& swapchainResource) {
        DepthImageResource depthResource{};
        depthResource.format = FindDepthFormat(gpu);
        MakeImage(gpu, depthResource.image, {swapchainResource.extent.width, swapchainResource.extent.height,1}, depthResource.format, vk::ImageTiling::eOptimal, vk::ImageUsageFlagBits::eDepthStencilAttachment, vk::MemoryPropertyFlagBits::eDeviceLocal);
        MakeImageView(gpu, depthResource.image, depthResource.format, vk::ImageViewType::e2D, vk::ImageSubresourceRange{ vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1 });
        depthRes.emplace(std::move(depthResource));
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

    vk::raii::DescriptorSetLayout MakeDescriptorSetLayout(const GPUResources& gpu,const std::vector<DescriptorSetLayoutBinding>& bindingData,const vk::DescriptorSetLayoutCreateFlags flags = {} ){
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

    vk::PipelineVertexInputStateCreateInfo MakePipeVertexInputState(std::span<const vk::VertexInputBindingDescription> bindings,std::span<const vk::VertexInputAttributeDescription> attributes) {
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo
            .setVertexBindingDescriptionCount(bindings.size())
            .setVertexAttributeDescriptionCount(attributes.size())
            .setVertexBindingDescriptions(bindings)
            .setVertexAttributeDescriptions(attributes);
        return vertexInputInfo;
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

    void EndSingleTimeCommands(const GPUResources& gpu, const vk::raii::CommandBuffer &cmd) {
        cmd.end();
        vk::SubmitInfo submitInfo{};
        submitInfo.setCommandBufferCount(1)
                 .setPCommandBuffers(&*cmd);
        gpu.graphicsQueue->submit(submitInfo, nullptr);
        gpu.graphicsQueue->waitIdle();
    }


    void MakeBuffer(Buffer& buffer,const GPUResources& gpu, const vk::DeviceSize& size, const vk::BufferUsageFlags& usage, const vk::MemoryPropertyFlags& properties, const vk::SharingMode& shareMode = vk::SharingMode::eExclusive) noexcept {
        if (!gpu.device || !gpu.physicalDevice || size == 0 || !usage) return ;

        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.setSize(size)
                  .setUsage(usage)
                  .setSharingMode(shareMode);

        buffer.data.emplace(*gpu.device, bufferInfo);

        const vk::MemoryRequirements reqs = buffer.data->getMemoryRequirements();
        buffer.memory.emplace(AllocateDeviceMemory(gpu,reqs,properties));

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

    void CopyBufferToImage(const GPUResources & gpu, const Buffer& buffer, const Image& image, const vk::Format format, const vk::Extent3D& extent, const vk::ImageSubresourceRange& range = DEFAULT_COLOR_SUBRESOURCE_RANGE, const vk::ImageSubresourceLayers& layers = {vk::ImageAspectFlagBits::eColor,0,0,1}, const vk::Offset3D& offset = {0,0,0},const vk::DeviceSize& bufferOffset = 0, const uint32_t& bufferRowLength = 0, const uint32_t& bufferImageHeight = 0) {
        const auto cmb = BeginSingleTimeCommands(gpu);
        TransitionImageLayout(cmb, *image.data, format, range, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vk::BufferImageCopy region{};
        region.setImageSubresource( layers)
              .setImageOffset(offset)
              .setImageExtent(extent)
              .setBufferOffset(bufferOffset)
              .setBufferRowLength(bufferRowLength)
              .setBufferImageHeight(bufferImageHeight);

        cmb.copyBufferToImage(*buffer.data, *image.data, vk::ImageLayout::eTransferDstOptimal, { region });

        TransitionImageLayout(cmb, *image.data, format, range, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        EndSingleTimeCommands(gpu, cmb);
    }

}