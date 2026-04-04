//
// Created by b-boy on 11.11.2025.
//
module;

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <vulkan/vulkan_raii.hpp>
#include <filesystem>

export module ufox_render_core;

import ufox_lib;
import ufox_engine_lib;
import ufox_engine_core;
import ufox_nanoid;
import ufox_graphic_device;
import ufox_render_lib;

using namespace ufox::engine;

export namespace ufox::render {
    void CopyBufferToImage(const gpu::vulkan::GPUResources & gpu, const gpu::vulkan::Buffer& buffer, const Texture2D& texture, const vk::Extent3D extent, const vk::ImageSubresourceLayers& subresourceLayers = {vk::ImageAspectFlagBits::eColor,0,0,1}, const vk::Offset3D& offset = {0,0,0},vk::DeviceSize bufferOffset = 0, const uint32_t& bufferRowLength = 0, const uint32_t& bufferImageHeight = 0) {
        const auto cmb = gpu::vulkan::BeginSingleTimeCommands(gpu);
        gpu::vulkan::TransitionImageLayout(cmb, texture.image.value(), texture.format, texture.subresourceRange, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        vk::BufferImageCopy region{};
        region.setImageSubresource( subresourceLayers)
              .setImageOffset(offset)
              .setImageExtent(extent)
              .setBufferOffset(bufferOffset)
              .setBufferRowLength(bufferRowLength)
              .setBufferImageHeight(bufferImageHeight);

        cmb.copyBufferToImage(*buffer.data, *texture.image, vk::ImageLayout::eTransferDstOptimal, { region });

        gpu::vulkan::TransitionImageLayout(cmb, texture.image.value(), texture.format, texture.subresourceRange, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
        gpu::vulkan::EndSingleTimeCommands(gpu, cmb);
    }

    void CreateImage(const gpu::vulkan::GPUResources& gpu, Texture2D& texture,const vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties,const vk::SharingMode shareMode = vk::SharingMode::eExclusive) {
        gpu::vulkan::Buffer staging{};
        vk::DeviceSize bufferSize = texture.size();
        gpu::vulkan::MakeBuffer(staging, gpu, bufferSize,vk::BufferUsageFlagBits::eTransferSrc,vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        gpu::vulkan::CopyToDevice(*staging.memory, texture.pixels.data(), bufferSize);

        vk::ImageCreateInfo imageInfo{};
        imageInfo
            .setImageType(vk::ImageType::e2D)
            .setFormat(texture.format)
            .setExtent({ texture.width(), texture.height(), 1 })
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(texture.tiling)
            .setUsage(usage)
            .setSharingMode(shareMode);

        texture.image.emplace(gpu.device.value(), imageInfo);

        vk::MemoryRequirements memoryRequirements = texture.image->getMemoryRequirements();

        vk::MemoryAllocateInfo allocInfo{};
        allocInfo
            .setAllocationSize(memoryRequirements.size)
            .setMemoryTypeIndex(gpu::vulkan::FindMemoryType(gpu.physicalDevice.value().getMemoryProperties(), memoryRequirements.memoryTypeBits, properties));

        texture.memory.emplace(gpu.device.value(), allocInfo);

        texture.image->bindMemory(*texture.memory, 0);

        CopyBufferToImage(gpu, staging, texture, {texture.width(), texture.height(), 1});
    }

    void CreateTextureImageView(gpu::vulkan::GPUResources const & gpu, Texture2D & texture) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo
            .setImage(*texture.image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(texture.format)
            .setSubresourceRange(texture.subresourceRange);
        texture.view.emplace(gpu.device.value(), viewInfo);
    }

    void CreateTextureSampler(const gpu::vulkan::GPUResources & gpu, Texture2D& texture){
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

    namespace procedures {
        constexpr auto WHITE = "white";
        constexpr auto BLACK = "black";
        constexpr ResourceID BUILTIN_WHITE_ID  {WHITE};
        constexpr ResourceID BUILTIN_BLACK_ID  {BLACK};

        std::pair<std::vector<std::byte>, vk::Extent2D> CreateSolidColorPixels(const uint32_t width,const uint32_t height,const Color& color) {
            if (width == 0 || height == 0) return {};

            const auto pixelCount = static_cast<size_t>(width) * static_cast<size_t>(height);
            std::vector<std::byte> pixels(pixelCount * PIXEL_BYTE_SIZE);

            for (size_t i = 0; i < pixels.size(); i += PIXEL_BYTE_SIZE) {
                pixels[i + 0] = color.r;
                pixels[i + 1] = color.g;
                pixels[i + 2] = color.b;
                pixels[i + 3] = color.a;
            }

            return {std::move(pixels), vk::Extent2D{width, height}};
        }

        std::pair<std::vector<std::byte>, vk::Extent2D> CreateCheckerTexture(const uint32_t size,const Color& color1,const Color& color2) {
            if (size == 0) return {};
            std::vector<std::byte> pixels(size * size * PIXEL_BYTE_SIZE);

            const uint32_t cellSize = std::max(1u, size / 2);

            const auto writePixel = [&pixels, size](const uint32_t x, const uint32_t y, const Color& color) {
                const size_t pixelIndex = (static_cast<size_t>(y) * size + x) * PIXEL_BYTE_SIZE;
                pixels[pixelIndex + 0] = color.r;
                pixels[pixelIndex + 1] = color.g;
                pixels[pixelIndex + 2] = color.b;
                pixels[pixelIndex + 3] = color.a;
            };

            for (uint32_t y = 0; y < size; ++y) {
                for (uint32_t x = 0; x < size; ++x) {
                    const bool useFirstColor = (x / cellSize + y / cellSize) % 2 == 0;
                    const Color& selectedColor = useFirstColor ? color1 : color2;
                    writePixel(x, y, selectedColor);
                }
            }

            return {std::move(pixels), vk::Extent2D{size, size}};
        }

        constexpr std::pair<std::vector<std::byte>, vk::Extent2D> WhitePixel()  {return CreateSolidColorPixels(1, 1, Color::White());}
        constexpr std::pair<std::vector<std::byte>, vk::Extent2D> BlackPixel()  {return CreateSolidColorPixels(1, 1, Color::Black());}
        constexpr std::pair<std::vector<std::byte>, vk::Extent2D> BlackAndWhiteCheckerPixels(const uint32_t size = 128) {return CreateCheckerTexture(size, Color::Black(), Color::White());}
    }

    class TextureManager;
    const ResourceID* CreateWhiteTexture(TextureManager& manager);
    const ResourceID* CreateBlackTexture(TextureManager& manager);
    const ResourceID* CreateTextureContent(TextureManager& manager, const std::span<std::byte>& pixels, const vk::Extent2D& extent, const ResourceContextCreateInfo& info);

    bool LoadPixelsFromFile(const std::filesystem::path& sourcePath, std::vector<std::byte>& outPixels, vk::Extent2D& outExtent) {
        const std::string basePath = SDL_GetBasePath();
        const std::string assetPath = basePath + sourcePath.string();

        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> loadedSurface{
            IMG_Load(assetPath.c_str()), SDL_DestroySurface
        };
        if (!loadedSurface) {
            debug::log(debug::LogLevel::eError, "Failed to load texture: {}", SDL_GetError());
            return false;
        }

        std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)> convertedSurface{
            SDL_ConvertSurface(loadedSurface.get(), SDL_PIXELFORMAT_ABGR8888), SDL_DestroySurface
        };
        if (!convertedSurface) {
            debug::log(debug::LogLevel::eError, "Failed to convert texture: {}", SDL_GetError());
            return false;
        }

        outExtent = vk::Extent2D{
            static_cast<uint32_t>(convertedSurface->w),
            static_cast<uint32_t>(convertedSurface->h)
        };

        const auto* sourcePixels = static_cast<const std::byte*>(convertedSurface->pixels);
        const std::size_t pixelDataSize =
            static_cast<std::size_t>(outExtent.width) *
            static_cast<std::size_t>(outExtent.height) * PIXEL_BYTE_SIZE;

        outPixels.resize(pixelDataSize);
        std::memcpy(outPixels.data(), sourcePixels, pixelDataSize);

        return true;
    }

    BuiltInResources MakeTextureBuiltInResources(TextureManager& manager) {
        return {{procedures::WHITE, CreateWhiteTexture(manager)},
                {procedures::BLACK, CreateBlackTexture(manager)}};
    }

    class TextureManager final : public ResourceManagerBase {
    public:
        explicit TextureManager(const gpu::vulkan::GPUResources& gpu)
            : ResourceManagerBase(gpu, TEXTURE_RESOURCE_PATH, TEXTURE_RESOURCE_EXTENSIONS) {}

        ~TextureManager() override {
           clearAllGpuResources(*this);
        }

        void makeResource(const ResourceContextCreateInfo &info) override {
            std::vector<std::byte> pixels{};
            vk::Extent2D extent{};
            if (LoadPixelsFromFile(info.sourcePath, pixels, extent)) {
                CreateTextureContent(*this, pixels, extent, info);
            }
        }

        void init() override {
            ReadResourceContextMetaData(directory, sourceExtensions, this);
            builtInResources = MakeTextureBuiltInResources(*this);
            debug::log(debug::LogLevel::eInfo, "TextureManager: init: success");
        }

        const ResourceID* makeTexture(const std::span<std::byte>& pixels, const vk::Extent2D& extent, const ResourceContextCreateInfo& info) {
            const ResourceID& id = makeResourceContext(info.id);
            std::unique_ptr<ResourceBase> res = std::make_unique<Texture2D>(info.name, pixels, extent, id);
            debug::log(debug::LogLevel::eInfo, "TextureManager: makeTexture: created texture: {}", res->name);
            return bindResource(res, info);
        }

        void useTexture(TextureUser& user) {
            if (user.id == nullptr) return;
            auto* ctx = getResourceContext(*user.id);
            if (ctx == nullptr || ctx->dataPtr == nullptr) return;

            auto* texture = ctx->dataPtr->getContent<Texture2D>();

            if (texture == nullptr) return;

            user.texture = texture;
            if (const auto it = std::ranges::find(ctx->users, &user); it == ctx->users.end()) {
                ctx->users.push_back(&user);
            }

            makeTextureGpuResource(user.texture);
        }

        void unuseTexture(TextureUser& user) {
            if (user.id == nullptr) return;
            user.texture = nullptr;

            auto* ctx = getResourceContext(*user.id);
            if (ctx == nullptr) return;


            if (const auto it = std::ranges::find(ctx->users, &user); it != ctx->users.end()) {
                ctx->users.erase(it);
            }

            if (!ctx->users.empty() || ctx->dataPtr == nullptr) return;

            if (ctx->dataPtr->hasGpuResources()) {
                ctx->dataPtr->releaseGpuResources();
                debug::log(debug::LogLevel::eInfo, "MeshManager: unuseMesh [{}]: mesh released buffers", ctx->dataPtr->name);
            }
        }

    private:
        void makeTextureGpuResource(Texture2D* texture) const {
            if (texture == nullptr) return;
            if (texture->hasGpuResources()) return;
            texture->releaseGpuResources();
            CreateImage(*gpuResources, *texture, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,vk::MemoryPropertyFlagBits::eDeviceLocal);
            CreateTextureImageView(*gpuResources, *texture);
            CreateTextureSampler(*gpuResources, *texture);
        }

        void releaseResource(const ResourceID& id) {
            auto* ctx = getResourceContext(id);
            if (ctx == nullptr) return;

            for (ResourceUserBase* user : ctx->users) {
              if (const auto textureUser = dynamic_cast<TextureUser *>(user)) unuseTexture(*textureUser);
            }
            ctx->clear();

            if (ctx->dataPtr) {
                ctx->dataPtr->releaseGpuResources();
                debug::log(debug::LogLevel::eInfo, "Released buffers for texture: {}", ctx->dataPtr->name);
                ctx->dataPtr.reset();
            }

            container.erase(id);
        }
    };

    const ResourceID* CreateTextureContent(TextureManager& manager, const std::span<std::byte>& pixels, const vk::Extent2D& extent, const ResourceContextCreateInfo& info) {
        return manager.makeTexture(pixels, extent, info);
    }

    const ResourceID* CreateWhiteTexture(TextureManager& manager) {
        auto [pixels,extent] = procedures::WhitePixel();
        ResourceContextCreateInfo info{};
        info
        .setName(procedures::WHITE)
        .setSourceType(SourceType::eBuiltIn)
        .setCategory("Standard")
        .setID(procedures::BUILTIN_WHITE_ID);
        return CreateTextureContent(manager, pixels, extent, info);
    }
    const ResourceID* CreateBlackTexture(TextureManager& manager) {
        auto [pixels,extent]  = procedures::BlackPixel();
        ResourceContextCreateInfo info{};
        info
        .setName(procedures::BLACK)
        .setSourceType(SourceType::eBuiltIn)
        .setCategory("Standard")
        .setID(procedures::BUILTIN_BLACK_ID);

        return CreateTextureContent(manager, pixels, extent, info);
    }
}



