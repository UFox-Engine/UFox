//
// Created by b-boy on 11.11.2025.
//
module;

#include <memory>
#include <vulkan/vulkan_raii.hpp>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>

export module ufox_render_core;

import ufox_lib;
import ufox_graphic_device;
import ufox_render_lib;

export namespace ufox::render {
    void RenderArea(const vk::raii::CommandBuffer& cmb, const windowing::WindowResource& window, const vk::Rect2D& rect, const vk::ClearColorValue& clearColor) {
        if (rect.extent.width < 1 || rect.extent.height < 1) return;

        vk::RenderingAttachmentInfo colorAttachment{};
        colorAttachment.setImageView(window.swapchainResource->getCurrentImageView())
                       .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                       .setLoadOp(vk::AttachmentLoadOp::eClear)
                       .setStoreOp(vk::AttachmentStoreOp::eStore)
                       .setClearValue(clearColor);

        vk::RenderingInfo renderingInfo{};
        renderingInfo.setRenderArea(rect)
                     .setLayerCount(1)
                     .setColorAttachmentCount(1)
                     .setPColorAttachments(&colorAttachment);
        cmb.beginRendering(renderingInfo);





        cmb.endRendering();
    }

    void SetImageLayout(vk::raii::CommandBuffer const & commandBuffer, const Texture2D& texture, vk::ImageLayout oldImageLayout, vk::ImageLayout newImageLayout ) {
        vk::AccessFlags sourceAccessMask;
        switch ( oldImageLayout )
        {
        case vk::ImageLayout::eTransferDstOptimal: sourceAccessMask = vk::AccessFlagBits::eTransferWrite; break;
        case vk::ImageLayout::ePreinitialized    : sourceAccessMask = vk::AccessFlagBits::eHostWrite; break;
        case vk::ImageLayout::eGeneral           :  // sourceAccessMask is empty
        case vk::ImageLayout::eUndefined         : break;
        default                                  : assert( false ); break;
        }

        vk::PipelineStageFlags sourceStage;
        switch ( oldImageLayout )
        {
        case vk::ImageLayout::eGeneral:
        case vk::ImageLayout::ePreinitialized    : sourceStage = vk::PipelineStageFlagBits::eHost; break;
        case vk::ImageLayout::eTransferDstOptimal: sourceStage = vk::PipelineStageFlagBits::eTransfer; break;
        case vk::ImageLayout::eUndefined         : sourceStage = vk::PipelineStageFlagBits::eTopOfPipe; break;
        default                                  : assert( false ); break;
        }

        vk::AccessFlags destinationAccessMask;
        switch ( newImageLayout )
        {
        case vk::ImageLayout::eColorAttachmentOptimal: destinationAccessMask = vk::AccessFlagBits::eColorAttachmentWrite; break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal:
            destinationAccessMask = vk::AccessFlagBits::eDepthStencilAttachmentRead | vk::AccessFlagBits::eDepthStencilAttachmentWrite;
            break;
        case vk::ImageLayout::eGeneral:  // empty destinationAccessMask
        case vk::ImageLayout::ePresentSrcKHR        : break;
        case vk::ImageLayout::eShaderReadOnlyOptimal: destinationAccessMask = vk::AccessFlagBits::eShaderRead; break;
        case vk::ImageLayout::eTransferSrcOptimal   : destinationAccessMask = vk::AccessFlagBits::eTransferRead; break;
        case vk::ImageLayout::eTransferDstOptimal   : destinationAccessMask = vk::AccessFlagBits::eTransferWrite; break;
        default                                     : assert( false ); break;
        }

        vk::PipelineStageFlags destinationStage;
        switch ( newImageLayout )
        {
        case vk::ImageLayout::eColorAttachmentOptimal       : destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput; break;
        case vk::ImageLayout::eDepthStencilAttachmentOptimal: destinationStage = vk::PipelineStageFlagBits::eEarlyFragmentTests; break;
        case vk::ImageLayout::eGeneral                      : destinationStage = vk::PipelineStageFlagBits::eHost; break;
        case vk::ImageLayout::ePresentSrcKHR                : destinationStage = vk::PipelineStageFlagBits::eBottomOfPipe; break;
        case vk::ImageLayout::eShaderReadOnlyOptimal        : destinationStage = vk::PipelineStageFlagBits::eFragmentShader; break;
        case vk::ImageLayout::eTransferDstOptimal           :
        case vk::ImageLayout::eTransferSrcOptimal           : destinationStage = vk::PipelineStageFlagBits::eTransfer; break;
        default                                             : assert( false ); break;
        }

        vk::ImageAspectFlags aspectMask;
        if ( newImageLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal )
        {
            aspectMask = vk::ImageAspectFlagBits::eDepth;
            if ( texture.format == vk::Format::eD32SfloatS8Uint || texture.format == vk::Format::eD24UnormS8Uint )
            {
                aspectMask |= vk::ImageAspectFlagBits::eStencil;
            }
        }
        else
        {
            aspectMask = vk::ImageAspectFlagBits::eColor;
        }

        vk::ImageSubresourceRange   imageSubresourceRange{};
        imageSubresourceRange
            .setAspectMask(aspectMask)
            .setBaseMipLevel(0)
            .setLevelCount(1)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

        vk::ImageMemoryBarrier      imageMemoryBarrier{};
        imageMemoryBarrier
            .setSrcAccessMask( sourceAccessMask )
            .setDstAccessMask( destinationAccessMask )
            .setOldLayout( oldImageLayout )
            .setNewLayout( newImageLayout )
            .setSrcQueueFamilyIndex( VK_QUEUE_FAMILY_IGNORED )
            .setDstQueueFamilyIndex( VK_QUEUE_FAMILY_IGNORED )
            .setImage( *texture.data )
            .setSubresourceRange( imageSubresourceRange );

        return commandBuffer.pipelineBarrier( sourceStage, destinationStage, {}, nullptr, nullptr, imageMemoryBarrier );
    }

    void CopyBufferToImage(vk::raii::CommandBuffer const & cmb, const gpu::vulkan::Buffer& buffer, const Texture2D& texture, const vk::Extent3D extent, const vk::ImageSubresourceLayers& subresourceLayers = {vk::ImageAspectFlagBits::eColor,0,0,1}, const vk::Offset3D& offset = {0,0,0},
        vk::DeviceSize bufferOffset = 0, const uint32_t& bufferRowLength = 0, const uint32_t& bufferImageHeight = 0) {


        vk::BufferImageCopy region{};
        region.setImageSubresource( subresourceLayers)
              .setImageOffset(offset)
              .setImageExtent(extent)
              .setBufferOffset(bufferOffset)
              .setBufferRowLength(bufferRowLength)
              .setBufferImageHeight(bufferImageHeight);

        cmb.copyBufferToImage(*buffer.data, *texture.data, vk::ImageLayout::eTransferDstOptimal, { region });
    }


    void CreateImage(const gpu::vulkan::GPUResources& gpu, Texture2D& texture,const vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,const vk::SharingMode shareMode = vk::SharingMode::eExclusive) {
        vk::ImageCreateInfo imageInfo{};
        imageInfo
            .setImageType(vk::ImageType::e2D)
            .setFormat(texture.format)
            .setExtent({ texture.extent.width, texture.extent.height, 1 })
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(texture.tiling)
            .setUsage(usage)
            .setSharingMode(shareMode);

        texture.data.emplace(gpu.device.value(), imageInfo);


        vk::MemoryRequirements memoryRequirements = texture.data->getMemoryRequirements();

        vk::MemoryAllocateInfo allocInfo{};

        allocInfo

            .setAllocationSize(memoryRequirements.size)

            .setMemoryTypeIndex(gpu::vulkan::FindMemoryType(gpu.physicalDevice.value().getMemoryProperties(), memoryRequirements.memoryTypeBits, properties));

        texture.memory.emplace(gpu.device.value(), allocInfo);

        texture.data->bindMemory(*texture.memory, 0);
    }

    void CreateTextureImageView(gpu::vulkan::GPUResources const & gpu, Texture2D & texture) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo
            .setImage(*texture.data)
            .setViewType(texture.viewType)
            .setFormat(texture.format)
            .setSubresourceRange(texture.subresourceRange);
        texture.view.emplace(gpu.device.value(), viewInfo);
    }

    void createTextureSampler(const gpu::vulkan::GPUResources & gpu, Texture2D& texture){
      const auto deviceProperties = gpu.physicalDevice->getProperties();

        vk::SamplerCreateInfo samplerInfo{};
        samplerInfo.setMagFilter(vk::Filter::eLinear)
                    .setMinFilter(vk::Filter::eLinear)
                    .setAddressModeU(vk::SamplerAddressMode::eRepeat)
                    .setAddressModeV(vk::SamplerAddressMode::eRepeat)
                    .setAddressModeW(vk::SamplerAddressMode::eRepeat)
                    .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                    .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                    .setAnisotropyEnable(true)
                    .setMaxAnisotropy(deviceProperties.limits.maxSamplerAnisotropy)
                    .setBorderColor(vk::BorderColor::eFloatOpaqueWhite)
                    .setUnnormalizedCoordinates(false)
                    .setCompareEnable(false)
                    .setCompareOp(vk::CompareOp::eAlways)
                    .setMinLod(0.0f)
                    .setMaxLod(0.0f)
                    .setMipLodBias(0.0f);

       texture.sampler.emplace(gpu.device.value(), samplerInfo);
    }

    std::unique_ptr<Texture2D> CreateTexture(const gpu::vulkan::GPUResources& gpu, std::string_view path) {
        const std::string filename = path.data();
        std::string fullPath = SDL_GetBasePath() + filename;

        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> rawSurface
        {IMG_Load(fullPath.c_str()), SDL_DestroySurface};
        if (!rawSurface) {
            debug::log(debug::LogLevel::eError, "Failed to load texture: {}", SDL_GetError());
            return nullptr;
        }

        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> convSurface
        {SDL_ConvertSurface(rawSurface.get(), SDL_PIXELFORMAT_ABGR8888), SDL_DestroySurface};
        if (!convSurface) {
            debug::log(debug::LogLevel::eError, "Failed to convert texture: {}", SDL_GetError());
            return nullptr;
        }

        Texture2D texture{};

        vk::Extent2D extent{static_cast<uint32_t>(convSurface->w), static_cast<uint32_t>(convSurface->h)};
        texture.format = vk::Format::eR8G8B8A8Unorm;
        texture.extent = extent;
        vk::DeviceSize imageSize = extent.width * extent.height * 4;

        //Create a staging buffer
        gpu::vulkan::Buffer stagingBuffer{};
        gpu::vulkan::MakeBuffer(stagingBuffer, gpu, imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        // Copy texture data to staging buffer
        gpu::vulkan::CopyToDevice(*stagingBuffer.memory, convSurface->pixels, imageSize);

        //Create the texture image
        CreateImage(gpu, texture, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);

        // Copy data from staging buffer to texture image
        auto cmb = gpu::vulkan::BeginSingleTimeCommands(gpu);
        SetImageLayout(cmb, texture, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        CopyBufferToImage(cmb,stagingBuffer, texture, {extent.width, extent.height, 1});
        SetImageLayout(cmb, texture, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        gpu::vulkan::EndSingleTimeCommands(gpu, cmb);

        return std::make_unique<Texture2D>(std::move(texture));
    }
}
