module;

#include <filesystem>
#include <fstream>
#include <msdf-atlas-gen/msdf-atlas-gen.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>
#include <thread>
#include <utility>
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
      case GlyphUnicodeBlock::eLGC:
        return "LGC";
      case GlyphUnicodeBlock::eJP:
        return "JP";
      case GlyphUnicodeBlock::eTHAI:
        return "thai";
      }
      return "unknown";
    }


    [[nodiscard]] inline nlohmann::json WriteGlyphContextJsonProperties(const GlyphContext& g) {
        return {
            {"codepoint",    g.codepoint},
            {"advance",      g.advance},
            {"uvX",          g.uvX},
            {"uvY",          g.uvY},
            {"uvW",          g.uvW},
            {"uvH",          g.uvH},
            {"planeLeft",    g.planeLeft},
            {"planeBottom",  g.planeBottom},
            {"planeRight",   g.planeRight},
            {"planeTop",     g.planeTop}
        };
    }


    [[nodiscard]] inline GlyphContext ReadJsonPropertiesToGlyphContext(const nlohmann::json& j) {
        GlyphContext g{};
        g.codepoint    = j.value("codepoint",    0u);
        g.advance      = j.value("advance",      0.0);
        g.uvX          = j.value("uvX",          0.0);
        g.uvY          = j.value("uvY",          0.0);
        g.uvW          = j.value("uvW",          0.0);
        g.uvH          = j.value("uvH",          0.0);
        g.planeLeft    = j.value("planeLeft",    0.0);
        g.planeBottom  = j.value("planeBottom",  0.0);
        g.planeRight   = j.value("planeRight",   0.0);
        g.planeTop     = j.value("planeTop",     0.0);
        return g;
    }


    [[nodiscard]] inline bool WriteGlyphToJson(const std::filesystem::path& jsonPath,const GlyphDataBindInfo& glyphBindInfo){
        std::ofstream out(jsonPath);
        if (!out.is_open()) return false;

        // Unpack all five fields
        const auto& [atlasInfo, block, style, metrics, glyphs] = glyphBindInfo;

        nlohmann::json j;
        j["atlasInfo"] = {
            { "name",          atlasInfo.name },
            { "id",            std::string{atlasInfo.id.view()} },
            { "sourcePath",    atlasInfo.sourcePath.string() },
            { "sourceType",    static_cast<int>(atlasInfo.sourceType) },
            { "category",      atlasInfo.category },
            { "lastWriteTime", atlasInfo.lastWriteTimeFromMeta }
        };
        j["block"]        = block;
        j["style"]        = style;
        j["GlyphFontMetrics"] = {
            {"ascender",  metrics.ascender},
            {"descender", metrics.descender},
            {"lineHeight",metrics.lineHeight},
            {"pxRange",   metrics.pxRange}
        };
        j["glyphs"]       = nlohmann::json::array();

        for (const auto& g : glyphs)
            j["glyphs"].push_back(WriteGlyphContextJsonProperties(g));

        out << j.dump(4);
        return static_cast<bool>(out);
    }

    [[nodiscard]] inline bool ReadGlyphFromJson(const std::filesystem::path& jsonPath,GlyphDataBindInfo& glyphBindInfo){
        std::ifstream in(jsonPath);
        if (!in.is_open()) return false;

        try {
            const nlohmann::json j = nlohmann::json::parse(in);
            if (!j.contains("atlasInfo") || !j["atlasInfo"].is_object()) return false;

            // Unpack all five fields
            auto& [atlasInfo, block, style, metrics, glyphs] = glyphBindInfo;
            const auto& atlasJson = j["atlasInfo"];
            const auto& metricsJson = j["GlyphFontMetrics"];

            atlasInfo.name                = atlasJson.value("name", "");
            atlasInfo.id                  = KeyID{atlasJson.value("id", "")};
            atlasInfo.sourcePath          = atlasJson.value("sourcePath", "");
            atlasInfo.sourceType          = static_cast<SourceType>(atlasJson.value("sourceType", 0));
            atlasInfo.category            = atlasJson.value("category", "");
            atlasInfo.lastWriteTimeFromMeta = atlasJson.value("lastWriteTime", "");

            block = static_cast<GlyphUnicodeBlock>(j.value("block", 0));
            style = static_cast<FontType>(j.value("style", 0));


            metrics.ascender   = metricsJson.value("ascender",   0.0);
            metrics.descender  = metricsJson.value("descender",   0.0);
            metrics.lineHeight = metricsJson.value("lineHeight",  0.0);
            metrics.pxRange    = metricsJson.value("pxRange",     0.0);

            glyphs.clear();
            if (j.contains("glyphs") && j["glyphs"].is_array()) {
                glyphs.reserve(j["glyphs"].size());
                for (const auto& item : j["glyphs"])
                    glyphs.push_back(ReadJsonPropertiesToGlyphContext(item));
            }
            return true;
        }
        catch (...) { return false; }
    }

    FontType GetGlyphStyle(const std::string_view name) {
        const bool isRegular = name.find("Regular") != std::string_view::npos;
        const bool isBold = name.find("Bold") != std::string_view::npos;
        const bool isItalic = name.find("Italic") != std::string_view::npos;

        if (isBold && isItalic) {
            return FontType::eBoldItalic;
        }

        if (isBold) {
            return FontType::eBold;
        }

        if (isItalic) {
            return FontType::eItalic;
        }

        if (isRegular) {
            return FontType::eRegular;
        }

        return FontType::eUnknown;
    }

    const std::set<msdf_atlas::unicode_t>* GetGlyphUnicodeBlock(const GlyphUnicodeBlock& bloc)  {
        switch (bloc) {
        case GlyphUnicodeBlock::eLGC : return &FONT_LATIN_UNICODE_BLOCK;
            break;
        case GlyphUnicodeBlock::eUnknown:
            return nullptr;
          break;
        case GlyphUnicodeBlock::eJP:
            return &FONT_KANJI_UNICODE_BLOCK;
        case GlyphUnicodeBlock::eTHAI:
            return &FONT_THAI_UNICODE_BLOCK;
          break;
        default: return nullptr;
        }
        return nullptr;
    }

    bool GenerateFreeTypeMSDFGlyph(GlyphDataBindInfo& binding,ResourceContextCreateInfo& outInfo){
        auto& [atlasInfo, outBlock, outStyle, outMetrics, outGlyphs] = binding;

        const auto* uCodes = GetGlyphUnicodeBlock(outBlock);
        if (!uCodes) {                                           // fix #8
            debug::log(debug::LogLevel::eWarning, "No charset for block {}", GlyphUnicodeBlockToString(outBlock));
            return false;
        }

        msdfgen::FreetypeHandle* ft = msdfgen::initializeFreetype();
        if (!ft) return false;
        msdfgen::FontHandle* font = msdfgen::loadFont(ft, outInfo.sourcePath.string().c_str());
        if (!font) { msdfgen::deinitializeFreetype(ft); return false; }

        bool success = false;
        {
            std::vector<msdf_atlas::GlyphGeometry> glyphs;
            msdf_atlas::FontGeometry fontGeometry(&glyphs);

            msdf_atlas::Charset charset;
            for (auto cp : *uCodes) {                            // fix #7
                if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) continue;
                charset.add(cp);
            }

            // fontScale 1.0 => everything in em units. preprocessGeometry + kerning on.
            fontGeometry.loadCharset(font, 1.0, charset, true, true);

            const auto& m = fontGeometry.getMetrics();
            outMetrics.ascender   = m.ascenderY;
            outMetrics.descender  = m.descenderY;
            outMetrics.lineHeight = m.lineHeight;

            // fix #4: best quality coloring
            constexpr double maxCornerAngle = 3.0;
            for (auto & glyph : glyphs)
                glyph.edgeColoring(&msdfgen::edgeColoringByDistance, maxCornerAngle, 0);

            msdf_atlas::TightAtlasPacker packer;
            packer.setDimensionsConstraint(msdf_atlas::DimensionsConstraint::POWER_OF_TWO_SQUARE);
            packer.setScale(ATLAS_SCALE);
            packer.setPixelRange(PX_RANGE);
            packer.setMiterLimit(1.0);
            packer.setSpacing(2);
            if (packer.pack(glyphs.data(), static_cast<int>(glyphs.size())) != 0) {   // fix #9
                debug::log(debug::LogLevel::eError, "Atlas pack failed (scale too big)");
                goto cleanup;
            }
            int width = 0, height = 0;
            packer.getDimensions(width, height);

            // fix #3: scanline pass + overlap support
            msdf_atlas::GeneratorAttributes attributes;
            attributes.config.overlapSupport = true;
            attributes.scanlinePass          = true;
            // attributes.config.errorCorrection keeps its default (edge-priority) – good

            const std::filesystem::path rangeDir = outInfo.sourcePath.parent_path();
            const std::filesystem::path cachesDir = rangeDir / "caches";
            std::error_code ec;
            std::filesystem::create_directories(cachesDir, ec);
            if (ec) {
                debug::log(debug::LogLevel::eError, "Failed to create caches dir: {} ({})", cachesDir.string(), ec.message());
                goto cleanup;
            }

            const std::string blockName = GlyphUnicodeBlockToString(outBlock);
            const std::string atlasName = outInfo.name + "_" + blockName + "_mtsdf";
            const std::filesystem::path pngPath = cachesDir / (atlasName + ".png");
            const std::filesystem::path jsonPath = cachesDir / (outInfo.name + "_" + blockName + ".json");

            msdf_atlas::ImmediateAtlasGenerator<float, 4, msdf_atlas::mtsdfGenerator,
                msdf_atlas::BitmapAtlasStorage<msdf_atlas::byte, 4>> generator(width, height);
            generator.setAttributes(attributes);
            generator.setThreadCount(std::max(1u, std::thread::hardware_concurrency()));
            generator.generate(glyphs.data(), static_cast<int>(glyphs.size()));

            atlasInfo.setName(atlasName)
                     .setSourcePath(pngPath)
                     .setCategory("Texture/Font")
                     .setSourceType(SourceType::eBuiltIn)
                     .setID(KeyID{atlasName})
                     .setOverwrite(outInfo.overwrite);
            msdfgen::savePng(generator.atlasStorage(), atlasInfo.sourcePath.string().c_str());

            std::string t; GetFileWriteTime(atlasInfo.sourcePath, t); atlasInfo.setLastWriteTime(t);

            // ... your existing glyph -> GlyphContext loop is correct, keep it ...
            // ... JSON write: also write outMetrics (ascender, descender, lineHeight, pxRange) ...

            auto atlasWidth = static_cast<double>(width);
            auto atlasHeight = static_cast<double>(height);

            outGlyphs.reserve(glyphs.size());

            for (const auto& g : glyphs) {
                GlyphContext ctx{};
                ctx.codepoint = g.getCodepoint();
                ctx.advance = g.getAdvance();

                double x,y,w,h;
                g.getQuadAtlasBounds(x, h, w, y);
                ctx.uvX = x/atlasWidth;
                ctx.uvY = (atlasHeight - y)/atlasHeight;
                ctx.uvW = (w - x)/atlasWidth;
                ctx.uvH = (y-h)/atlasHeight;

                // Plane bounds in em-units — bearing + glyph size
                double pl, pb, pr, pt;
                g.getQuadPlaneBounds(pl, pb, pr, pt);
                ctx.planeLeft   = pl;
                ctx.planeBottom = pb;
                ctx.planeRight  = pr;
                ctx.planeTop    = pt;

                outGlyphs.emplace_back(ctx);
            }

            outInfo.setCategory("Font");
            outInfo.attachments.clear();
            outInfo.attachments.reserve(2);

            if (WriteGlyphToJson(jsonPath, binding)) {
                outInfo.attachments.emplace_back(GLYPH_JSON_ATTACHMENT_KEY, jsonPath);

                if (!atlasInfo.sourcePath.empty()) {
                    outInfo.attachments.emplace_back(GLYPH_ATLAS_ATTACHMENT_KEY, atlasInfo.sourcePath);
                }
            }

            success = true;
        }
    cleanup:
        msdfgen::destroyFont(font);
        msdfgen::deinitializeFreetype(ft);
        return success;
    }

    [[nodiscard]] inline KeyID MakeFontFamilyKey(std::string_view name) {
        return KeyID{name.substr(0, name.find('-'))};
    }

    class GlyphManager;
    const KeyID* MakeGlyphContent(GlyphManager& manager,const KeyID& atlasTextureID,const ResourceContextCreateInfo& info,const GlyphUnicodeBlock& block,const FontType& style, const GlyphFontMetrics& metrics,const std::vector<GlyphContext>& glyphs);

    class GlyphManager final : public ResourceManagerBase {
    public:
        explicit GlyphManager(UFoxWindow& _window, render::TextureManager& _textureManager): ResourceManagerBase(_window, FONT_RESOURCE_PATH, FONT_RESOURCE_EXTENSIONS), textureManager(_textureManager) {
            window->registerCallbackEvent<EventType::eSystemInit>([](void* user){static_cast<GlyphManager*>(user)->onSystemInit(); }, this);
            window->registerCallbackEvent<EventType::eGainsFocus>([](void* user){static_cast<GlyphManager*>(user)->onGainsFocus(); }, this);

            for (const auto b : {GlyphUnicodeBlock::eLGC, GlyphUnicodeBlock::eJP, GlyphUnicodeBlock::eTHAI})
                fonts_.try_emplace(b);
        }

        ~GlyphManager() override {
            window->unregisterCallbackEvent(EventType::eSystemInit,this);
            window->unregisterCallbackEvent(EventType::eGainsFocus,this);
        }

        void onMakeResource(ResourceContextCreateInfo& info) override {
            const std::string folderName = info.sourcePath.parent_path().filename().string();
            const KeyID searchedID{folderName};

            const FontType resourceStyle = GetGlyphStyle(info.name);
            if (resourceStyle == FontType::eUnknown) {
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
                debug::log(debug::LogLevel::eWarning,"GlyphManager: onMakeResource: no glyph source type entry found for source path: {} searchedID: {}",info.sourcePath.string(),searchedID.view());
                return;
            }

            GlyphDataBindInfo bindInfo{};

            if (tryLoadBindInfoFromCache(info, bindInfo, it->second, resourceStyle)) {
                const auto& [atlasInfo, glyphBlock, glyphStyle, metrics, glyphContexts] = bindInfo;
                if (const KeyID* id = MakeGlyphContent(*this, atlasInfo.id, info, glyphBlock, glyphStyle, metrics, glyphContexts)) {
                    if (auto* slot = fonts_[glyphBlock][MakeFontFamilyKey(info.name)].slot(glyphStyle))
                        *slot = *id;      // copy the value, never keep the pointer
                }
            } else {
                debug::log(debug::LogLevel::eWarning,"GlyphManager: onMakeResource: skipped invalid glyph bind info for font: {}",info.name);
            }
        }

        void onSystemInit() override {
            ReadResourceContextMetaData(directory, sourceExtensions, this);
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

        const KeyID* makeGlyph(const KeyID& atlasTextureID,const ResourceContextCreateInfo& info,const GlyphUnicodeBlock& block,const FontType& style, const GlyphFontMetrics& metrics,const std::vector<GlyphContext>& glyphs){
            KeyID id = atlasTextureID;
            if (!makeResourceContext(id, info.overwrite)) {
                return nullptr;
            }
            std::unique_ptr<ResourceBase> res = std::make_unique<Glyph>(
                info.name, id, block, style, metrics, glyphs);  // ← pass extent
            return bindResourceToContext(res, info);
        }

        // by atlas/glyph ID
        [[nodiscard]] Glyph* getGlyph(const KeyID& id) {
            const auto* ctx = getResourceContext(id);
            if (ctx == nullptr || ctx->dataPtr == nullptr) return nullptr;
            return dynamic_cast<Glyph*>(ctx->dataPtr.get());
        }

        // by block + family + style, falls back to Regular
        [[nodiscard]] const KeyID* findGlyphID(const GlyphUnicodeBlock block, const KeyID& family,
                                                    const FontType style = FontType::eRegular) const {
            const auto b = fonts_.find(block);
            if (b == fonts_.end()) return nullptr;
            const auto f = b->second.find(family);
            if (f == b->second.end()) return nullptr;

            const KeyID* want = f->second.slot(style);
            if (want && want->IsValid()) return want;

            const KeyID* fallback = f->second.slot(FontType::eRegular);
            return (fallback && fallback->IsValid()) ? fallback : nullptr;
        }

        [[nodiscard]] Glyph* getGlyph(const GlyphUnicodeBlock block, const KeyID& family,
                                      const FontType style = FontType::eRegular) {
            const KeyID* id = findGlyphID(block, family, style);
            return id ? getGlyph(*id) : nullptr;
        }

        [[nodiscard]] Glyph* getGlyph(const GlyphUnicodeBlock block, const std::string_view family,
                                      const FontType style = FontType::eRegular) {
            return getGlyph(block, KeyID{family}, style);
        }

    private:
        render::TextureManager& textureManager;
        std::unordered_map<GlyphUnicodeBlock, std::unordered_map<KeyID, GlyphTypeKey>> fonts_;

        BuiltInResources MakeFontBuiltInResources(GlyphManager& manager);

        [[nodiscard]] bool tryLoadBindInfoFromCache(ResourceContextCreateInfo& info,GlyphDataBindInfo& bindInfo, const GlyphUnicodeBlock& block, const FontType resourceStyle) const {
            if (block == GlyphUnicodeBlock::eUnknown) {
                debug::log(
                    debug::LogLevel::eWarning,
                    "GlyphManager: tryLoadBindInfoFromCache: glyph block is unknown type{}",
                    info.name
                );
                return false;
            }

            const std::filesystem::path* glyphJsonPath{nullptr};

            for (const auto& [key, path] : info.attachments) {
                if (key == GLYPH_JSON_ATTACHMENT_KEY) {
                    glyphJsonPath = &path;
                }
            }

            if (!glyphJsonPath) {
                debug::log(
                    debug::LogLevel::eInfo,
                    "GlyphManager: tryLoadBindInfoFromCache: cache glyph json attachment missing for font: {}. Regenerating.",
                    info.name
                );
                return makeAtlasResourceBindInfos(info, bindInfo, block, resourceStyle);
            }

            if (!ReadGlyphFromJson(*glyphJsonPath, bindInfo)) {
                debug::log(
                    debug::LogLevel::eInfo,
                    "GlyphManager: tryLoadBindInfoFromCache: failed to read glyph cache: {}. Regenerating.",
                    glyphJsonPath->string()
                );
                return makeAtlasResourceBindInfos(info, bindInfo, block, resourceStyle);
            }

            auto& [atlasInfo, glyphBlock, glyphStyle, metrics, glyphContexts] = bindInfo;

            if (glyphBlock != block) {
                debug::log(
                    debug::LogLevel::eInfo,
                    "GlyphManager: tryLoadBindInfoFromCache: cached glyph block mismatch for font: {}. Regenerating.",
                    info.name
                );
                return makeAtlasResourceBindInfos(info, bindInfo, block, resourceStyle);
            }

            render::TextureDataBindInfo atlasBindInfo{};
            if (!render::LoadPixelsFromFile(atlasInfo.sourcePath, atlasBindInfo)) {
                debug::log(
                    debug::LogLevel::eInfo,
                    "GlyphManager: tryLoadBindInfoFromCache: failed to load cached atlas texture: {}. Regenerating.",
                    atlasInfo.sourcePath.string()
                );
                return makeAtlasResourceBindInfos(info, bindInfo, block, resourceStyle);
            }

            render::MakeTextureContent(textureManager, atlasBindInfo, atlasInfo);

            return true;
        }

        [[nodiscard]] bool makeAtlasResourceBindInfos(ResourceContextCreateInfo& info,GlyphDataBindInfo& bindInfo , const GlyphUnicodeBlock& block,const FontType resourceStyle) const {
            auto& [atlasInfo, glyphBlock, glyphStyle, metrics, glyphContexts] = bindInfo;

            glyphBlock = block;
            glyphStyle = resourceStyle;

            if (!GenerateFreeTypeMSDFGlyph(bindInfo, info)) {
                debug::log(debug::LogLevel::eWarning,"GlyphManager: makeAtlasResourceBindInfos: failed to generate atlas for font: {}",info.name);
                return false;
            }

            if (atlasInfo.sourcePath.empty() || atlasInfo.id.view().empty() || glyphContexts.empty()) {
                debug::log(debug::LogLevel::eWarning,"GlyphManager: makeAtlasResourceBindInfos: generated invalid glyph data for font: {}",info.name);
                return false;
            }

            render::TextureDataBindInfo atlasBindInfo{};
            if (!render::LoadPixelsFromFile(atlasInfo.sourcePath, atlasBindInfo)) {
                debug::log(debug::LogLevel::eWarning,"GlyphManager: makeAtlasResourceBindInfos: failed to load generated atlas texture: {}",atlasInfo.sourcePath.string());
                return false;
            }

            render::MakeTextureContent(textureManager, atlasBindInfo, atlasInfo);

            return true;
        }
    };

    const KeyID* MakeGlyphContent(GlyphManager& manager,const KeyID& atlasTextureID,const ResourceContextCreateInfo& info,const GlyphUnicodeBlock& block,const FontType& style, const GlyphFontMetrics& metrics,const std::vector<GlyphContext>& glyphs){
        return manager.makeGlyph(atlasTextureID, info, block, style, metrics, glyphs);
    }
}
