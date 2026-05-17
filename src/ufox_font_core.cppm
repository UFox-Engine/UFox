module;

#include <filesystem>
#include <fstream>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

export module ufox_font_core;

import ufox_font_lib;
import ufox_lib;
import ufox_engine_lib;
import ufox_engine_core;
import ufox_render_lib;
import ufox_render_core;

using namespace ufox::engine;

export namespace ufox::font {

    constexpr std::string GlyphUnicodeBlockToString(const GlyphUnicodeBlock& block) {
        switch (block) {
      case GlyphUnicodeBlock::eUnknown:
        return "unknown";
      case GlyphUnicodeBlock::eLatin:
        return "latin";
      case GlyphUnicodeBlock::eCyrillic:
        return "cyrillic";
      case GlyphUnicodeBlock::eGreek:
        return "greek";
      case GlyphUnicodeBlock::eHanzi:
        return "hanzi";
      case GlyphUnicodeBlock::eKanji:
        return "kanji";
      case GlyphUnicodeBlock::eHangul:
        return "hangul";
      case GlyphUnicodeBlock::eThai:
        return "thai";
      }
      return "unknown";
    }

    [[nodiscard]] inline nlohmann::json WriteGlyphContextJsonProperties(const GlyphContext& g) {
        return {
            {"codepoint", g.codepoint},
            {"advance",   g.advance},
            {"uvX",       g.uvX},
            {"uvY",       g.uvY},
            {"uvW",       g.uvW},
            {"uvH",       g.uvH}
        };
    }

    [[nodiscard]] inline GlyphContext ReadJsonPropertiesToGlyphContext(const nlohmann::json& j) {
        GlyphContext g{};

        g.codepoint = j.value("codepoint", 0u);
        g.advance   = j.value("advance", 0.0);
        g.uvX       = j.value("uvX", 0.0f);
        g.uvY       = j.value("uvY", 0.0f);
        g.uvW       = j.value("uvW", 0.0f);
        g.uvH       = j.value("uvH", 0.0f);

        return g;
    }

    [[nodiscard]] inline bool WriteGlyphToJson(const std::filesystem::path& jsonPath, const GlyphDataBindInfo& glyphBindInfo) {
        std::ofstream out(jsonPath);
        if (!out.is_open()) {
            return false;
        }

        const auto& [atlasInfo, block, style, glyphs] = glyphBindInfo;

        nlohmann::json j;

        j["atlasInfo"] = {
            { "name",          atlasInfo.name },
            { "id",            std::string{atlasInfo.id.view()} },
            { "sourcePath",    atlasInfo.sourcePath.string() },
            { "sourceType",    static_cast<int>(atlasInfo.sourceType) },
            { "category",      atlasInfo.category },
            { "lastWriteTime", atlasInfo.lastWriteTimeFromMeta }
        };

        j["block"] = block;
        j["style"] = style;
        j["glyphs"] = nlohmann::json::array();

        for (const auto& g : glyphs) {
            j["glyphs"].push_back(WriteGlyphContextJsonProperties(g));
        }

        out << j.dump(4);
        return static_cast<bool>(out);
    }

    [[nodiscard]] inline bool ReadGlyphFromJson(const std::filesystem::path& jsonPath, GlyphDataBindInfo& glyphBindInfo) {
        std::ifstream in(jsonPath);
        if (!in.is_open()) {
            return false;
        }

        try {
            const nlohmann::json j = nlohmann::json::parse(in);

            if (!j.contains("atlasInfo") || !j["atlasInfo"].is_object()) {
                return false;
            }

            auto& [atlasInfo, block, style, glyphs] = glyphBindInfo;
            const auto& atlasJson = j["atlasInfo"];

            atlasInfo.name = atlasJson.value("name", "");
            atlasInfo.id = ResourceID{atlasJson.value("id", "")};

            const std::filesystem::path sourcePath = atlasJson.value("sourcePath", "");
            atlasInfo.sourcePath = sourcePath;
            atlasInfo.sourceType = static_cast<SourceType>(atlasJson.value("sourceType", 0));
            atlasInfo.category = atlasJson.value("category", "");
            atlasInfo.lastWriteTimeFromMeta = atlasJson.value("lastWriteTime", "");

            block = static_cast<GlyphUnicodeBlock>(j.value("block", 0));
            style = static_cast<GlyphStyle>(j.value("style", 0));

            glyphs.clear();

            if (j.contains("glyphs") && j["glyphs"].is_array()) {
                glyphs.reserve(j["glyphs"].size());

                for (const auto& item : j["glyphs"]) {
                    glyphs.push_back(ReadJsonPropertiesToGlyphContext(item));
                }
            }

            return true;
        }
        catch (...) {
            return false;
        }
    }

    GlyphStyle GetGlyphStyle(const std::string_view name) {
        const bool isRegular = name.find("Regular") != std::string_view::npos;
        const bool isBold = name.find("Bold") != std::string_view::npos;
        const bool isItalic = name.find("Italic") != std::string_view::npos;

        if (isBold && isItalic) {
            return GlyphStyle::eBoldItalic;
        }

        if (isBold) {
            return GlyphStyle::eBold;
        }

        if (isItalic) {
            return GlyphStyle::eItalic;
        }

        if (isRegular) {
            return GlyphStyle::eRegular;
        }

        return GlyphStyle::eUnknown;
    }

    const std::set<msdf_atlas::unicode_t>* GetGlyphUnicodeBlock(const GlyphUnicodeBlock& bloc)  {
        switch (bloc) {
        case GlyphUnicodeBlock::eLatin : return &FONT_LATIN_UNICODE_BLOCK;
            break;
        case GlyphUnicodeBlock::eUnknown:
            return nullptr;
          break;
        case GlyphUnicodeBlock::eCyrillic:
            return &FONT_CYRILLIC_UNICODE_BLOCK;
          break;
        case GlyphUnicodeBlock::eGreek:
            return &FONT_GREEK_UNICODE_BLOCK;
          break;
            case GlyphUnicodeBlock::eHanzi:
          break;
        case GlyphUnicodeBlock::eKanji:
            return &FONT_KANJI_UNICODE_BLOCK;
          break;
        case GlyphUnicodeBlock::eHangul:
            return nullptr;
          break;
        case GlyphUnicodeBlock::eThai:
            return &FONT_THAI_UNICODE_BLOCK;
          break;
        default: return nullptr;
        }
        return nullptr;
    }

    bool GenerateFreeTypeMSDFGlyph(const msdf_atlas::ImageType type, GlyphDataBindInfo& binding, ResourceContextCreateInfo& outInfo) {

        bool success =false;

        debug::log(debug::LogLevel::eInfo, "=== msdf-atlas-gen 1.4: Generating Roboto MSDF Atlas: {} ===", outInfo.name);

        if (msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype()) {
            if (msdfgen::FontHandle* font = msdfgen::loadFont(ft, outInfo.sourcePath.string().c_str())) {
                constexpr double fontSizeEm = 40.0;

                auto& [atlasInfo, outBlock, outStyle, outGlyphs] = binding;

                std::vector<msdf_atlas::GlyphGeometry> glyphs;
                msdf_atlas::FontGeometry fontGeometry(&glyphs);
                msdf_atlas::Charset charset;
                std::string blockName = GlyphUnicodeBlockToString(outBlock);

                for (const std::set<msdf_atlas::unicode_t> &uCodes = *GetGlyphUnicodeBlock(outBlock);
                     auto & code : uCodes) {
                    charset.add(code);
                     }

                fontGeometry.loadCharset(font, 1.0, charset);

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

                const std::string namePreFix = outInfo.sourcePath.parent_path().string() + "/";
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


                    std::string atlasName = outInfo.name + "_" + blockName + "_mtsdf";

                    atlasInfo
                    .setName(atlasName)
                    .setSourcePath(namePreFix + atlasName + extension)
                    .setCategory("Texture/Font")
                    .setSourceType(SourceType::eBuiltIn)
                    .setID(ResourceID{atlasName})
                    .setOverwrite(outInfo.overwrite);

                    msdfgen::savePng(bitmapRef, atlasInfo.sourcePath.string().c_str());

                    std::filesystem::path atlasPath = atlasInfo.sourcePath;
                    std::string lastWriteTimeStr;
                    GetFileWriteTime(atlasPath, lastWriteTimeStr);
                    atlasInfo.setLastWriteTime(lastWriteTimeStr);
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

                    std::string atlasName = outInfo.name + "_" + blockName + "_msdf";

                    atlasInfo
                    .setName(atlasName)
                    .setSourcePath(namePreFix + atlasName + extension)
                    .setCategory("Texture/Font")
                    .setSourceType(SourceType::eBuiltIn)
                    .setID(ResourceID{atlasName});

                    msdfgen::savePng(bitmapRef, atlasInfo.sourcePath.string().c_str());

                    std::filesystem::path atlasPath = atlasInfo.sourcePath;
                    std::string lastWriteTimeStr;
                    GetFileWriteTime(atlasPath, lastWriteTimeStr);
                    atlasInfo.setLastWriteTime(lastWriteTimeStr);

                }
                    break;
                default:

                    break;
                }

                auto atlasWidth = static_cast<double>(width);
                auto atlasHeight = static_cast<double>(height);

                outGlyphs.reserve(glyphs.size());

                for (const auto& g : glyphs) {
                    GlyphContext ctx{};
                    ctx.codepoint = g.getCodepoint();
                    ctx.advance = g.getAdvance();
                    double x,y,w,h;
                    g.getQuadAtlasBounds(x, h, w, y);
                    ctx.uvX = static_cast<float>(x/atlasWidth);
                    ctx.uvY = static_cast<float>((atlasHeight - y)/atlasHeight);
                    ctx.uvW = static_cast<float>((w - x)/atlasWidth);
                    ctx.uvH = static_cast<float>((y-h)/atlasHeight);
                    outGlyphs.emplace_back(ctx);
                }

                outInfo.setCategory("Font");

                std::string jsonPath = namePreFix + outInfo.name + "_" + blockName+".json";
                if (WriteGlyphToJson(jsonPath, binding)) {
                    outInfo.attachments.emplace_back(GLYPH_JSON_ATTACHMENT_KEY, jsonPath);

                    if (!atlasInfo.sourcePath.empty()) {
                        outInfo.attachments.emplace_back(GLYPH_ATLAS_ATTACHMENT_KEY, atlasInfo.sourcePath);
                    }
                }

                success = true;
                msdfgen::destroyFont(font);
            }
            msdfgen::deinitializeFreetype(ft);
        }

        return success;
    }

    class GlyphManager;
    const ResourceID* MakeGlyphContent(GlyphManager& manager,const ResourceID& atlasTextureID, const std::vector<GlyphContext>& glyphs,const GlyphUnicodeBlock& block, const GlyphStyle& style, const ResourceContextCreateInfo& info);

    class GlyphManager final : public ResourceManagerBase {
    public:
        explicit GlyphManager(UFoxWindow& _window, render::TextureManager& _textureManager): ResourceManagerBase(_window, FONT_RESOURCE_PATH, FONT_RESOURCE_EXTENSIONS), textureManager(_textureManager) {
            window->registerCallbackEvent<EventType::eSystemInit>([](void* user){static_cast<GlyphManager*>(user)->onSystemInit(); }, this);
            window->registerCallbackEvent<EventType::eGainsFocus>([](void* user){static_cast<GlyphManager*>(user)->onGainsFocus(); }, this);
        }

        ~GlyphManager() override {
            window->unregisterCallbackEvent(EventType::eSystemInit,this);
            window->unregisterCallbackEvent(EventType::eGainsFocus,this);
        }


        void onMakeResource(ResourceContextCreateInfo& info) override {
            const ResourceID searchedID{info.sourcePath.parent_path().filename().string()};

            const GlyphStyle resourceStyle = GetGlyphStyle(info.name);
            if (resourceStyle == GlyphStyle::eUnknown) {
                debug::log(
                    debug::LogLevel::eInfo,
                    "GlyphManager: onMakeResource: skipped unsupported glyph style for font: {}",
                    info.name
                );
                return;
            }

            const auto& sourceTypeMap = GLYPH_SOURCE_TYPE_MAP;

            const auto it = sourceTypeMap.find(searchedID);
            if (it == sourceTypeMap.end()) {
                debug::log(
                    debug::LogLevel::eWarning,
                    "GlyphManager: onMakeResource: no glyph source type entry found for source path: {} searchedID: {}",
                    info.sourcePath.string(),
                    searchedID.view()
                );
                return;
            }

            const std::vector<GlyphDataBindInfo> bindInfos = tryLoadBindInfosFromCache(info, it->second, resourceStyle);
            if (bindInfos.empty()) {
                debug::log(
                    debug::LogLevel::eWarning,
                    "GlyphManager: onMakeResource: no glyph bind infos were created for font: {}",
                    info.name
                );
                return;
            }

            for (const auto& bind : bindInfos) {
                const auto& [atlasInfo, glyphBlock, glyphStyle, glyphContexts] = bind;

                if (atlasInfo.id.view().empty() || glyphContexts.empty()) {
                    debug::log(
                        debug::LogLevel::eWarning,
                        "GlyphManager: onMakeResource: skipped invalid glyph bind info for font: {}",
                        info.name
                    );
                    continue;
                }

                MakeGlyphContent(*this, atlasInfo.id, glyphContexts, glyphBlock, glyphStyle, info);
            }
        }

        void onSystemInit() override {
            ReadResourceContextMetaData(directory, sourceExtensions, this);
            // builtInResources will be filled later
            debug::log(debug::LogLevel::eInfo, "GlyphManager: init success");
        }

        void onPostSystemInit(const float &width, const float &height) override {}

        void onGainsFocus() override {
            refreshResource();

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
        }

        void onPreDraw() override {}

        // Placeholder for future registration (empty for now)
        const ResourceID* makeGlyph(const ResourceID& atlasTextureID, const std::vector<GlyphContext>& glyphs,const GlyphUnicodeBlock& block, const GlyphStyle& style, const ResourceContextCreateInfo& info) {
            ResourceID id = atlasTextureID;
            if(!makeResourceContext(id, info.overwrite)) {
                debug::log(debug::LogLevel::eWarning, "GlyphManager: makeGlyph: skip");
                return nullptr;
            }

            std::unique_ptr<ResourceBase> res = std::make_unique<Glyph>(info.name, id, block, style, glyphs);
            return bindResourceToContext(res,info);
        }

        // Placeholder lookup
        [[nodiscard]] Glyph* getFont(std::string_view languageKey) const noexcept;

    private:
        render::TextureManager& textureManager;
        BuiltInResources MakeFontBuiltInResources(GlyphManager& manager);
        [[nodiscard]] std::vector<GlyphDataBindInfo> tryLoadBindInfosFromCache(ResourceContextCreateInfo& info, const std::vector<GlyphUnicodeBlock>& blocks, const GlyphStyle resourceStyle) const {
            if (blocks.empty()) {
                debug::log(
                    debug::LogLevel::eWarning,
                    "GlyphManager: tryLoadBindInfosFromCache: no glyph blocks configured for font: {}",
                    info.name
                );
                return {};
            }

            std::vector<std::filesystem::path> glyphJsonPaths;
            glyphJsonPaths.reserve(blocks.size());

            for (const auto& [key, path] : info.attachments) {
                if (key == GLYPH_JSON_ATTACHMENT_KEY) {
                    glyphJsonPaths.push_back(path);
                }
            }

            // If the glyph JSON attachment size does not match the block size, remake the data.
            if (blocks.size() != glyphJsonPaths.size()) {
                debug::log(
                    debug::LogLevel::eInfo,
                    "GlyphManager: tryLoadBindInfosFromCache: cache glyph json attachment count mismatch for font: {}. Regenerating.",
                    info.name
                );
                return makeAtlasResourceBindInfos(info, blocks, resourceStyle);
            }

            std::vector<GlyphDataBindInfo> bindInfos;
            bindInfos.reserve(blocks.size());

            for (size_t i = 0; i < blocks.size(); ++i) {
                const auto& jsonPath = glyphJsonPaths[i];

                GlyphDataBindInfo bindInfo{};
                if (!ReadGlyphFromJson(jsonPath, bindInfo)) {
                    debug::log(
                        debug::LogLevel::eInfo,
                        "GlyphManager: tryLoadBindInfosFromCache: failed to read glyph cache: {}. Regenerating.",
                        jsonPath.string()
                    );
                    return makeAtlasResourceBindInfos(info, blocks, resourceStyle);
                }

                auto& [atlasInfo, glyphBlock, glyphStyle, glyphContexts] = bindInfo;

                if (glyphBlock != blocks[i]) {
                    debug::log(
                        debug::LogLevel::eInfo,
                        "GlyphManager: tryLoadBindInfosFromCache: cached glyph block mismatch for font: {}. Regenerating.",
                        info.name
                    );
                    return makeAtlasResourceBindInfos(info, blocks, resourceStyle);
                }

                if (atlasInfo.sourcePath.empty() || atlasInfo.id.view().empty() || glyphContexts.empty()) {
                    debug::log(
                        debug::LogLevel::eInfo,
                        "GlyphManager: tryLoadBindInfosFromCache: invalid cached glyph data for font: {}. Regenerating.",
                        info.name
                    );
                    return makeAtlasResourceBindInfos(info, blocks, resourceStyle);
                }

                render::TextureDataBindInfo atlasBindInfo{};
                if (!render::LoadPixelsFromFile(atlasInfo.sourcePath, atlasBindInfo)) {
                    debug::log(
                        debug::LogLevel::eInfo,
                        "GlyphManager: tryLoadBindInfosFromCache: failed to load cached atlas texture: {}. Regenerating.",
                        atlasInfo.sourcePath.string()
                    );
                    return makeAtlasResourceBindInfos(info, blocks, resourceStyle);
                }

                ufox::render::MakeTextureContent(textureManager, atlasBindInfo, atlasInfo);

                bindInfos.push_back(std::move(bindInfo));
            }

            return bindInfos;
        }

        [[nodiscard]] std::vector<GlyphDataBindInfo> makeAtlasResourceBindInfos(ResourceContextCreateInfo& info, const std::vector<GlyphUnicodeBlock>& blocks,const GlyphStyle resourceStyle) const {
            std::vector<GlyphDataBindInfo> bindInfos;
            bindInfos.reserve(blocks.size());
            info.attachments.clear();
            info.attachments.reserve(blocks.size());

            for (const auto& block : blocks) {
                GlyphDataBindInfo bindInfo{};
                auto& [atlasInfo, glyphBlock, glyphStyle, glyphContexts] = bindInfo;

                glyphBlock = block;
                glyphStyle = resourceStyle;

                if (!GenerateFreeTypeMSDFGlyph(msdf_atlas::ImageType::MTSDF, bindInfo, info)) {
                    debug::log(
                        debug::LogLevel::eWarning,
                        "GlyphManager: makeAtlasResourceBindInfos: failed to generate atlas for font: {}",
                        info.name
                    );
                    continue;
                }

                if (atlasInfo.sourcePath.empty() || atlasInfo.id.view().empty() || glyphContexts.empty()) {
                    debug::log(
                        debug::LogLevel::eWarning,
                        "GlyphManager: makeAtlasResourceBindInfos: generated invalid glyph data for font: {}",
                        info.name
                    );
                    continue;
                }

                render::TextureDataBindInfo atlasBindInfo{};
                if (!render::LoadPixelsFromFile(atlasInfo.sourcePath, atlasBindInfo)) {
                    debug::log(
                        debug::LogLevel::eWarning,
                        "GlyphManager: makeAtlasResourceBindInfos: failed to load generated atlas texture: {}",
                        atlasInfo.sourcePath.string()
                    );
                    continue;
                }

                ufox::render::MakeTextureContent(textureManager, atlasBindInfo, atlasInfo);

                bindInfos.push_back(std::move(bindInfo));
            }

            return bindInfos;
        }

    };

    const ResourceID* MakeGlyphContent(GlyphManager& manager, const ResourceID& atlasTextureID, const std::vector<GlyphContext>& glyphs,const GlyphUnicodeBlock& block, const GlyphStyle& style, const ResourceContextCreateInfo& info) {
       return manager.makeGlyph(atlasTextureID, glyphs, block, style, info);
    }
}
