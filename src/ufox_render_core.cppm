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
        descriptor
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImageView(*texture.image.view)
            .setSampler(*texture.sampler);
        return descriptor;
    }

    vk::DescriptorImageInfo MakeDescriptorSampledImageInfo(const Texture& texture) noexcept {
        vk::DescriptorImageInfo descriptor{};
        descriptor
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setImageView(*texture.image.view);
        return descriptor;
    }

    vk::DescriptorImageInfo MakeDescriptorSamplerOnlyInfo(const vk::raii::Sampler& sampler) noexcept {
        vk::DescriptorImageInfo descriptor{};
        descriptor
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
            .setSampler(sampler);
        return descriptor;
    }

    class TextureManager final : public ResourceManagerBase {
    public:
        explicit TextureManager(UFoxWindow& _window,const gpu::GPUResources& gpu)
            : ResourceManagerBase(_window,gpu, TEXTURE_RESOURCE_PATH, TEXTURE_RESOURCE_EXTENSIONS) {
            window->registerCallbackEvent<EventType::eSystemInit>([](void* user){static_cast<TextureManager*>(user)->onSystemInit();}, this);
            window->registerCallbackEvent<EventType::ePostSystemInit>([](const float& w, const float& h, void* user){static_cast<TextureManager*>(user)->onPostSystemInit(w,h);}, this);
            window->registerCallbackEvent<EventType::ePostGainsFocus>([](void* user){static_cast<TextureManager*>(user)->onGainsFocus(); }, this);
            window->registerCallbackEvent<EventType::ePostGainsFocus>([](void* user) {static_cast<TextureManager*>(user)->onPostGainsFocus();}, this);
        }

        ~TextureManager() override {
            window->unregisterCallbackEvent(EventType::eSystemInit,this);
            window->unregisterCallbackEvent(EventType::ePostSystemInit,this);
            window->unregisterCallbackEvent(EventType::eGainsFocus,this);
            window->unregisterCallbackEvent(EventType::ePostGainsFocus,this);
            clearAllGpuResources(*this);
        }

        void onMakeResource(ResourceContextCreateInfo &info) override {
            TextureDataBindInfo bindInfo{};
            if (LoadPixelsFromFile(info.sourcePath, bindInfo)) {
                MakeTextureContent(*this, bindInfo, info);
            }
        }

        void onSystemInit() override {
            ReadResourceContextMetaData(directory, sourceExtensions, this);
            const auto limit = gpuResources->physicalDevice->getProperties().limits;
            maxDescriptorSetSamplerImages = limit.maxDescriptorSetSampledImages;
            builtInResources = MakeTextureBuiltInResources(*this);
            debug::log(debug::LogLevel::eInfo, "TextureManager: init: success");
        }

        void onPostSystemInit(const float &width, const float &height) override{
            makeTextureMap(textureMap, defaultMapIndex);
        }

        void onGainsFocus() override {
            refreshResource();
        }

        void onPostGainsFocus() override {
            for (auto &ctx : container | std::views::values) {
                auto& file = ctx.sourcePath;
                std::string lastWriteTimeStr;
                GetFileWriteTime(file, lastWriteTimeStr);

                if (lastWriteTimeStr != ctx.lastWriteTime) {
                    ResourceContextCreateInfo info{};
                    MakeResourceContextCreateInfo(ctx, info, lastWriteTimeStr);
                    onMakeResource(info);
                }
            }

            makeTextureMap(textureMap, defaultMapIndex);
        }

        [[nodiscard]] uint32_t getMaxDescriptorSetSamplerImages(const uint32_t& request) const noexcept {
            return std::max( maxDescriptorSetSamplerImages, request );
        }

        const ResourceID* makeTexture(TextureDataBindInfo& bindInfo, const ResourceContextCreateInfo& info) {
            auto& [pixels, extent] = bindInfo;
            ResourceID id = ResolveResourceID(info);
            if(!makeResourceContext(id, info.overwrite)) return nullptr;

            std::unique_ptr<ResourceBase> res = std::make_unique<Texture>(info.name, pixels, extent, id);
            const auto texture = dynamic_cast<Texture*>(res.get());
            MakeTexture2D(*gpuResources, *texture, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,vk::MemoryPropertyFlagBits::eDeviceLocal);
            MakeTexture2DImageView(*gpuResources, *texture);
            gpu::MakeSampler(*gpuResources, texture->sampler);
            return bindResourceToContext(res, info);
        }

        void makeTextureMap(std::map<const ResourceID,const uint32_t>& textureMap, uint32_t& defaultMapIndex) noexcept {
            textureMap.clear();

            uint32_t index = 0;
            for (const auto &id : container | std::views::keys) {
                textureMap.emplace(id, index);
                index++;
            }

            defaultMapIndex = textureMap.find(ResourceID{"default"})->second;
        }

        [[nodiscard]] std::vector<vk::DescriptorImageInfo> makeCombinedImageSamplerDescriptorImageInfos() const noexcept {
            std::vector<vk::DescriptorImageInfo> descriptorImageInfos{};
            descriptorImageInfos.reserve(container.size());

            for (const auto &ctx : container | std::views::values) {
                const auto * texture = dynamic_cast<Texture*>(ctx.dataPtr.get());
                descriptorImageInfos.emplace_back(MakeDescriptorImageInfo(*texture));
            }

            return std::move(descriptorImageInfos);
        }

        [[nodiscard]] std::vector<vk::DescriptorImageInfo> makeSampledImageDescriptorImageInfos() const noexcept {
            std::vector<vk::DescriptorImageInfo> descriptorImageInfos{};
            descriptorImageInfos.reserve(container.size());

            for (const auto &ctx : container | std::views::values) {
                const auto * texture = dynamic_cast<Texture*>(ctx.dataPtr.get());
                descriptorImageInfos.emplace_back(MakeDescriptorSampledImageInfo(*texture));
            }

            return std::move(descriptorImageInfos);
        }

        [[nodiscard]] Texture* getTexture(const ResourceID& id) {
            const auto* ctx = getResourceContext(id);
            if (ctx == nullptr || ctx->dataPtr == nullptr) return nullptr;

            auto* texture = dynamic_cast<Texture*>(ctx->dataPtr.get());
            return texture;
        }

        [[nodiscard]] const Texture* getTexture(const ResourceID& id) const{
            const auto* ctx = getResourceContext(id);
            if (ctx == nullptr || ctx->dataPtr == nullptr) return nullptr;

            const auto * texture = dynamic_cast<Texture*>(ctx->dataPtr.get());
            return texture;
        }

        template<Arithmetic T>
        void getTextureSize(const ResourceID& id, T& width, T& height) const noexcept {
            const auto* tex = getTexture(id);
            if (tex == nullptr) {
                width = 1;
                height = 1;
            }
            width = static_cast<T>(tex->width());
            height = static_cast<T>(tex->height());
        }

        template<Arithmetic T>
        std::pair<T,T> getTextureSize(const ResourceID& id) const noexcept {
            const auto* tex = getTexture(id);
            if (tex == nullptr) {
                return std::make_pair(1,1);
            }
            auto width = static_cast<T>(tex->width());
            auto height = static_cast<T>(tex->height());
            return std::make_pair(width, height);
        }

        [[nodiscard]] uint32_t getTextureIndex(const ResourceID& id) const noexcept {
            const auto it = textureMap.find(id);
            if (it == textureMap.end()) {
                return defaultMapIndex;
            }
            return it->second;
        }

    private:
        uint32_t                                                    maxDescriptorSetSamplerImages{1};
        std::map<const ResourceID, const uint32_t>                  textureMap{};
        uint32_t                                                    defaultMapIndex{0};
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



