//
// Created by b-boy on 11.11.2025.
//
module;

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <memory>
#include <ranges>
#include <unordered_map>
#include <map>
#include <vulkan/vulkan_raii.hpp>
#include <glm/glm.hpp>


export module ufox_render_core;

import ufox_lib;
import ufox_engine_lib;
import ufox_engine_core;
import ufox_nanoid;
import ufox_gpu_lib;
import ufox_gpu_core;
import ufox_render_lib;

using namespace ufox::engine;

export namespace ufox::render {
    namespace procedures {
        inline auto WHITE = "white";
        inline auto BLACK = "black";
        inline ResourceID BUILTIN_WHITE_ID  {"white"};
        inline ResourceID BUILTIN_BLACK_ID  {"black"};



        TextureDataBindInfo CreateSolidColorPixels(const uint32_t width,const uint32_t height,const Color& color) {
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

        TextureDataBindInfo CreateCheckerPixels(const uint32_t size,const Color& color1,const Color& color2) {
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

        constexpr TextureDataBindInfo WhitePixel()  {return CreateSolidColorPixels(1, 1, Color::White());}
        constexpr TextureDataBindInfo BlackPixel()  {return CreateSolidColorPixels(1, 1, Color::Black());}
        constexpr TextureDataBindInfo BlackAndWhiteCheckerPixels(const uint32_t size = 128) {return CreateCheckerPixels(size, Color::Black(), Color::White());}
    }

    class TextureManager;
    const ResourceID* MakeWhiteTexture(TextureManager& manager);
    const ResourceID* MakeBlackTexture(TextureManager& manager);
    const ResourceID* MakeDefaultTexture(TextureManager& manager);
    const ResourceID* MakeTextureContent(TextureManager& manager, TextureDataBindInfo& bindInfo, const ResourceContextCreateInfo& info);

    bool LoadPixelsFromFile(const std::filesystem::path& sourcePath, TextureDataBindInfo& bindInfo) {
        auto& [outPixels, outExtent] = bindInfo;
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
        return {{procedures::WHITE, MakeWhiteTexture(manager)},{"default", MakeDefaultTexture(manager)},{procedures::BLACK, MakeBlackTexture(manager)}};
    }

    void MakeTexture2D(const gpu::GPUResources& gpu, Texture& texture,const vk::ImageUsageFlags& usage, const vk::MemoryPropertyFlags& properties,const vk::SharingMode& shareMode = vk::SharingMode::eExclusive) {
        gpu::Buffer staging{};
        vk::DeviceSize bufferSize = texture.size();
        gpu::MakeBuffer(staging, gpu, bufferSize,vk::BufferUsageFlagBits::eTransferSrc,vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
        gpu::CopyToDevice(*staging.memory, texture.pixels.data(), bufferSize);
        gpu::MakeImage(gpu, texture.image, { texture.width(), texture.height(), 1 }, texture.format, texture.tiling, usage, properties, texture.type, shareMode);
        gpu::CopyBufferToImage(gpu, staging, texture.image, texture.format, {texture.width(), texture.height(), 1}, texture.subresourceRange);
    }

    void MakeTexture2DImageView(gpu::GPUResources const & gpu, Texture & texture) {
        vk::ImageViewCreateInfo viewInfo{};
        viewInfo
            .setImage(*texture.image.data)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(texture.format)
            .setSubresourceRange(texture.subresourceRange);
        texture.image.view.emplace(gpu.device.value(), viewInfo);
    }

    vk::DescriptorImageInfo MakeDescriptorImageInfo(const Texture& texture) noexcept {
        vk::DescriptorImageInfo descriptor{};
        descriptor.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        descriptor.imageView = *texture.image.view;
        descriptor.sampler = *texture.sampler;
        return descriptor;
    }

    class TextureManager final : public ResourceManagerBase {
    public:
        explicit TextureManager(UFoxWindow& _window,const gpu::GPUResources& gpu)
            : ResourceManagerBase(_window,gpu, TEXTURE_RESOURCE_PATH, TEXTURE_RESOURCE_EXTENSIONS) {
            window->registerGainsFocusEventHandlers([](void* user){static_cast<TextureManager*>(user)->refreshResource(); }, this);
        }

        ~TextureManager() override {
            window->unregisterGainsFocusEventHandlers(this);
            clearAllGpuResources(*this);
            updateTextureEventHandlers.clear();
        }

        void makeResource(ResourceContextCreateInfo &info) override {
            TextureDataBindInfo bindInfo{};
            if (LoadPixelsFromFile(info.sourcePath, bindInfo)) {
                MakeTextureContent(*this, bindInfo, info);
            }
        }

        void updateResource() override {
            debug::log(debug::LogLevel::eInfo, "TextureManager: updateResource");
            for (auto &ctx : container | std::views::values) {
                auto& file = ctx.sourcePath;
                std::string lastWriteTimeStr;
                GetFileWriteTime(file, lastWriteTimeStr);

                if (lastWriteTimeStr != ctx.lastWriteTime) {
                    ResourceContextCreateInfo info{};
                    MakeResourceContextCreateInfo(ctx, info, lastWriteTimeStr);
                    makeResource(info);
                }
            }

            gpuResources->device->waitIdle();
            for (auto& [handler, userData] : updateTextureEventHandlers) {
                handler(userData);
            }
        }

        void init() override {
            ReadResourceContextMetaData(directory, sourceExtensions, this);
            builtInResources = MakeTextureBuiltInResources(*this);
            const auto limit = gpuResources->physicalDevice->getProperties().limits;
            maxDescriptorSetSamplerImages = limit.maxDescriptorSetSampledImages;

            debug::log(debug::LogLevel::eInfo, "TextureManager: init: success");
        }

        [[nodiscard]] uint32_t getMaxDescriptorSetSamplerImages(const uint32_t& request) const noexcept {
            return std::min( maxDescriptorSetSamplerImages, request );
        }

        const ResourceID* makeTexture(TextureDataBindInfo& bindInfo, const ResourceContextCreateInfo& info) {
            auto& [pixels, extent] = bindInfo;
            ResourceID id = ResolveResourceID(info);
            if(!makeResourceContext(id, info.overwrite)) {
                debug::log(debug::LogLevel::eWarning, "TextureManager: makeTexture: skip");
                return nullptr;
            }

            std::unique_ptr<ResourceBase> res = std::make_unique<Texture>(info.name, pixels, extent, id);
            const auto texture = dynamic_cast<Texture*>(res.get());
            MakeTexture2D(*gpuResources, *texture, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,vk::MemoryPropertyFlagBits::eDeviceLocal);
            MakeTexture2DImageView(*gpuResources, *texture);
            gpu::MakeSampler(*gpuResources, texture->sampler);
            debug::log(debug::LogLevel::eInfo, "TextureManager: makeTexture: created texture: {}", res->name);
            return bindResourceToContext(res, info);
        }

        void rebuildTextureMap(std::map<const ResourceID,const uint32_t>* textureMap) const noexcept {
            textureMap->clear();

            uint32_t index = 0;
            for (const auto &id : container | std::views::keys) {
                if (textureMap) {
                    textureMap->emplace(id, index);
                }

                index++;
            }
        }

        [[nodiscard]] std::vector<vk::DescriptorImageInfo> makeImageDescriptorImageInfos() const noexcept {
            std::vector<vk::DescriptorImageInfo> descriptorImageInfos{};
            descriptorImageInfos.reserve(container.size());

            for (const auto &ctx : container | std::views::values) {
                const auto * texture = dynamic_cast<Texture*>(ctx.dataPtr.get());
                descriptorImageInfos.emplace_back(MakeDescriptorImageInfo(*texture));
            }

            return std::move(descriptorImageInfos);
        }

        [[nodiscard]] Texture* getTexture(const ResourceID& id) {
            const auto* ctx = getResourceContext(id);
            if (ctx == nullptr || ctx->dataPtr == nullptr) return nullptr;

            auto* texture = dynamic_cast<Texture*>(ctx->dataPtr.get());
            return texture;
        }

        void registerUpdateTextureEventHandler(UpdateTextureEventHandler handler, void* userData) {
            updateTextureEventHandlers.emplace_back(handler, userData);
        }

        void unregisterUpdateTextureEventHandler(void* userData) {
            std::erase_if(updateTextureEventHandlers, [userData](auto& p){ return p.second == userData; });
        }

    private:
        uint32_t maxDescriptorSetSamplerImages{1};
        std::vector<std::pair<UpdateTextureEventHandler,void*>> updateTextureEventHandlers{};
    };

    const ResourceID* MakeTextureContent(TextureManager& manager, TextureDataBindInfo& bindInfo, const ResourceContextCreateInfo& info) {
        return manager.makeTexture(bindInfo, info);
    }

    const ResourceID* MakeWhiteTexture(TextureManager& manager) {
            auto bindInfo = procedures::WhitePixel();
            ResourceContextCreateInfo info{};
            info
            .setName(procedures::WHITE)
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(procedures::BUILTIN_WHITE_ID);
            return MakeTextureContent(manager, bindInfo, info);
        }

    const ResourceID* MakeBlackTexture(TextureManager& manager) {
            auto bindInfo  = procedures::BlackPixel();
            ResourceContextCreateInfo info{};
            info
            .setName(procedures::BLACK)
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(procedures::BUILTIN_BLACK_ID);

            return MakeTextureContent(manager, bindInfo, info);
        }

    const ResourceID* MakeDefaultTexture(TextureManager& manager) {
        auto bindInfo = procedures::CreateSolidColorPixels(
        1, 1, Color{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}});
            ResourceContextCreateInfo info{};
            info
            .setName("default")
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(ResourceID{"default"});
            return MakeTextureContent(manager, bindInfo, info);
        }



}



