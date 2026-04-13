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
#include <ft2build.h>
#include FT_FREETYPE_H
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

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

        std::pair<std::vector<std::byte>, vk::Extent2D> CreateCheckerPixels(const uint32_t size,const Color& color1,const Color& color2) {
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
        constexpr std::pair<std::vector<std::byte>, vk::Extent2D> BlackAndWhiteCheckerPixels(const uint32_t size = 128) {return CreateCheckerPixels(size, Color::Black(), Color::White());}
    }

    class TextureManager;
    const ResourceID* MakeWhiteTexture(TextureManager& manager);
    const ResourceID* MakeBlackTexture(TextureManager& manager);
    const ResourceID* MakeDefaultTexture(TextureManager& manager);
    const ResourceID* MakeTextureContent(TextureManager& manager, const std::span<std::byte>& pixels, const vk::Extent2D& extent, const ResourceContextCreateInfo& info);

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
        explicit TextureManager(const gpu::GPUResources& gpu)
            : ResourceManagerBase(gpu, TEXTURE_RESOURCE_PATH, TEXTURE_RESOURCE_EXTENSIONS) {}

        ~TextureManager() override {
            clearAllGpuResources(*this);
        }

        void makeResource(const ResourceContextCreateInfo &info) override {
            std::vector<std::byte> pixels{};
            vk::Extent2D extent{};
            if (LoadPixelsFromFile(info.sourcePath, pixels, extent)) {
                MakeTextureContent(*this, pixels, extent, info);
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

        const ResourceID* makeTexture(const std::span<std::byte>& pixels, const vk::Extent2D& extent, const ResourceContextCreateInfo& info) {
            const ResourceID& id = info.sourceType == SourceType::eBuiltIn ? info.id : ResourceID{info.sourcePath.filename().string()};
            makeResourceContext(id);
            std::unique_ptr<ResourceBase> res = std::make_unique<Texture>(info.name, pixels, extent, id);
            const auto texture = dynamic_cast<Texture*>(res.get());
            MakeTexture2D(*gpuResources, *texture, vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,vk::MemoryPropertyFlagBits::eDeviceLocal);
            MakeTexture2DImageView(*gpuResources, *texture);
            gpu::MakeSampler(*gpuResources, texture->sampler);
            debug::log(debug::LogLevel::eInfo, "TextureManager: makeTexture: created texture: {}", res->name);
            return bindResourceToContext(res, info);
        }

        void buildTextureMap(std::map<const ResourceID,const uint32_t>* textureMap) const noexcept {
            textureMap->clear();

            uint32_t index = 0;
            for (const auto &id : container | std::views::keys) {
                if (textureMap) {
                    textureMap->emplace(id, index);
                }
                debug::log(debug::LogLevel::eInfo, "TextureManager: buildTextureMap: texture: {} -> index: {}", id.view(), index);
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

    private:
        uint32_t maxDescriptorSetSamplerImages{1};
    };

    const ResourceID* MakeTextureContent(TextureManager& manager, const std::span<std::byte>& pixels, const vk::Extent2D& extent, const ResourceContextCreateInfo& info) {
        debug::log(debug::LogLevel::eInfo, "MakeWhiteTexture: id: {}", info.id.view());
        return manager.makeTexture(pixels, extent, info);
    }

    const ResourceID* MakeWhiteTexture(TextureManager& manager) {
            auto [pixels,extent] = procedures::WhitePixel();
            ResourceContextCreateInfo info{};
            info
            .setName(procedures::WHITE)
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(procedures::BUILTIN_WHITE_ID);
        debug::log(debug::LogLevel::eInfo, "MakeWhiteTexture: id: {}", info.id.view());
            return MakeTextureContent(manager, pixels, extent, info);
        }

    const ResourceID* MakeBlackTexture(TextureManager& manager) {
            auto [pixels,extent]  = procedures::BlackPixel();
            ResourceContextCreateInfo info{};
            info
            .setName(procedures::BLACK)
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(procedures::BUILTIN_BLACK_ID);

            return MakeTextureContent(manager, pixels, extent, info);
        }

    const ResourceID* MakeDefaultTexture(TextureManager& manager) {
        auto [pixels, extent] = procedures::CreateSolidColorPixels(
        1, 1, Color{std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0}});
            ResourceContextCreateInfo info{};
            info
            .setName("default")
            .setSourceType(SourceType::eBuiltIn)
            .setCategory("Standard")
            .setID(ResourceID{"default"});
            return MakeTextureContent(manager, pixels, extent, info);
        }

    namespace msdf {

        MSDFFontData GenerateRobotoMSDF(const msdf_atlas::ImageType type) {
            const std::string fontPath = "res/fonts/Roboto-Regular.ttf";
            const std::string name = std::filesystem::path(fontPath).stem().string();

            debug::log(debug::LogLevel::eInfo, "=== msdf-atlas-gen 1.4: Generating Roboto MSDF Atlas: {} ===", name);

            MSDFFontData result;

            if (msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype()) {
                if (msdfgen::FontHandle* font = msdfgen::loadFont(ft, fontPath.c_str())) {
                constexpr double fontSizeEm = 32.0;

                std::vector<msdf_atlas::GlyphGeometry> glyphs;
                    msdf_atlas::FontGeometry fontGeometry(&glyphs);

                    fontGeometry.loadCharset(font, 1.0, msdf_atlas::Charset::ASCII);

                    // Edge coloring
                    for (auto& glyph : glyphs) {
                      constexpr double maxCornerAngle = 3.0;
                      glyph.edgeColoring(&msdfgen::edgeColoringInkTrap, maxCornerAngle, 0);
                    }

                    // Tight packing
                    msdf_atlas::TightAtlasPacker packer;
                    packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::POWER_OF_TWO_SQUARE);
                    packer.setScale(fontSizeEm);
                    packer.setPixelRange(8.0);
                    packer.setMiterLimit(1.0);
                    packer.setSpacing(5);
                    packer.pack(glyphs.data(), static_cast<int>(glyphs.size()));

                    int width = 0, height = 0;
                    packer.getDimensions(width, height);

                    const std::string namePreFix {"res/fonts/"};
                    const std::string extension{".png"};
                    std::string nameEndFix{};

                    switch (type) {
                    case msdf_atlas::ImageType::MTSDF: {
                        msdf_atlas::ImmediateAtlasGenerator<float, 4,msdf_atlas::mtsdfGenerator,
                        msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>> generator(width, height);
                        msdf_atlas::GeneratorAttributes attributes;
                        generator.setAttributes(attributes);
                        generator.setThreadCount(4);
                        generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

                        msdfgen::BitmapConstRef<msdf_atlas::byte, 4> bitmapRef = generator.atlasStorage();

                        size_t finalWidth = bitmapRef.width;
                        size_t finalHeight = bitmapRef.height;
                        result.pixels.resize(finalWidth * finalHeight * 4);

                        for (int y = 0; y < finalHeight; ++y) {
                            int srcY = static_cast<int>(finalHeight) - 1 - y;                                 // flip Y
                            const msdf_atlas::byte* srcRow = bitmapRef.pixels + srcY * finalWidth * 4;
                            std::byte* dstRow = result.pixels.data() + y * finalWidth * 4;

                            for (int x = 0; x < finalWidth; ++x) {
                                size_t offset = x * 4;
                                dstRow[offset + 0] = static_cast<std::byte>(srcRow[offset + 0]); // R
                                dstRow[offset + 1] = static_cast<std::byte>(srcRow[offset + 1]); // G
                                dstRow[offset + 2] = static_cast<std::byte>(srcRow[offset + 2]); // B
                                dstRow[offset + 3] = static_cast<std::byte>(srcRow[offset + 3]); // A
                            }
                        }

                        result.extent = vk::Extent2D{
                            static_cast<uint32_t>(bitmapRef.width),
                            static_cast<uint32_t>(bitmapRef.height)
                        };

                        nameEndFix = "_mtsdf";

                        msdfgen::savePng(bitmapRef, (namePreFix + name + nameEndFix + extension).c_str());
                    }
                        break;
                    case msdf_atlas::ImageType::MSDF: {
                        msdf_atlas::ImmediateAtlasGenerator<float, 3,msdf_atlas::msdfGenerator,
                        msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 3>> generator(width, height);
                        msdf_atlas::GeneratorAttributes attributes;
                        generator.setAttributes(attributes);
                        generator.setThreadCount(4);
                        generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

                        msdfgen::BitmapConstRef<msdf_atlas::byte, 3> bitmapRef = generator.atlasStorage();

                        size_t finalWidth = bitmapRef.width;
                        size_t finalHeight = bitmapRef.height;
                        result.pixels.resize(finalWidth * finalHeight * 4);

                        for (int y = 0; y < finalHeight; ++y) {
                            int srcY = static_cast<int>(finalHeight) - 1 - y;                                 // flip Y
                            const msdf_atlas::byte* srcRow = bitmapRef.pixels + srcY * finalWidth * 3;
                            std::byte* dstRow = result.pixels.data() + y * finalWidth * 4;

                            for (int x = 0; x < finalWidth; ++x) {
                                size_t offset = x * 4;
                                dstRow[offset + 0] = static_cast<std::byte>(srcRow[offset + 0]); // R
                                dstRow[offset + 1] = static_cast<std::byte>(srcRow[offset + 1]); // G
                                dstRow[offset + 2] = static_cast<std::byte>(srcRow[offset + 2]); // B
                                dstRow[offset + 3] = std::byte{255}; // A
                            }
                        }

                        result.extent = vk::Extent2D{
                            static_cast<uint32_t>(bitmapRef.width),
                            static_cast<uint32_t>(bitmapRef.height)
                        };

                        nameEndFix = "_msdf";

                        msdfgen::savePng(bitmapRef, (namePreFix + name + nameEndFix + extension).c_str());
                    }
                        break;
                    default:

                        break;
                    }

                    // Glyph data (UVs stay the same - they are already in atlas space)
                    for (const auto& g : glyphs) {
                        GlyphContext ctx{};
                        ctx.codepoint = g.getCodepoint();
                        ctx.advance = g.getAdvance();
                        g.getBoxSize(ctx.boxWidth, ctx.boxHeight);
                        g.getQuadPlaneBounds(ctx.plane.x, ctx.plane.y, ctx.plane.z, ctx.plane.w);
                        g.getQuadAtlasBounds(ctx.atlas.x, ctx.atlas.y, ctx.atlas.z, ctx.atlas.w);
                        result.glyphs.push_back(ctx);
                    }

                    // ResourceID from font path (as you requested)
                    std::filesystem::path p(fontPath);

                    ResourceContextCreateInfo info{};
                    info.setName("Roboto-Regular-msdf")
                        .setSourceType(SourceType::eBuiltIn)//dont build meta
                        .setCategory("Font")
                        .setID(ResourceID{p.filename().string()})
                        .setSourcePath(p)
                        .setLastWriteTime("");

                    result.contextCreateInfo = info;


                    // Save JSON (matching your original style)
                    nlohmann::json j;
                    j["resourceID"] = result.contextCreateInfo.id.view();
                    j["font"] = "Roboto-Regular";
                    j["size"] = fontSizeEm;
                    j["glyphs"] = nlohmann::json::array();

                    for (const auto& g : result.glyphs) {
                        j["glyphs"].push_back({
                            {"codepoint", g.codepoint},
                            {"advance",   g.advance},
                            {"boxWidth",  g.boxWidth},
                            {"boxHeight", g.boxHeight},
                            {"plane", {
                                {"left",   g.plane.x},
                                {"bottom", g.plane.y},
                                {"right",  g.plane.z},
                                {"top",    g.plane.w}
                            }},
                            {"atlas", {
                                {"left",   g.atlas.x},
                                {"bottom", g.atlas.y},
                                {"right",  g.atlas.z},
                                {"top",    g.atlas.w}
                            }}
                        });
                    }

                    std::ofstream out(namePreFix + name + ".json");
                    if (out.is_open()) {
                        out << j.dump(4);
                        out.close();
                    }

                    msdfgen::destroyFont(font);
                }
                msdfgen::deinitializeFreetype(ft);
            }

            return std::move(result);
        }
    }
}



