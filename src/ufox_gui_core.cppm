module;


#define GLM_FORCE_RADIANS
#include "glm/vec2.hpp"

#include <SDL3/SDL_surface.h>
#include <_mingw_mac.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <map>
#include <ranges>
#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

export module ufox_gui_core;

import ufox_lib;
import ufox_engine_lib;
import ufox_engine_core;
import ufox_gpu_lib;
import ufox_gpu_core;
import ufox_geometry_core;
import ufox_geometry_lib;
import ufox_gui_lib;
import ufox_nanoid;
import ufox_render_lib;
import ufox_render_core;
import ufox_font_lib;
import ufox_font_core;
import ufox_key;
import ufox_core;
import ufox_windowing;

using namespace ufox::geometry;
using namespace ufox::render;
using namespace ufox::engine;

export namespace ufox::gui {
    class UIDocument;
    class VisualElement;
    void ForgetPointerTarget(UIDocument& document, VisualElement* removed) noexcept;

    glm::vec4 CalculateInnerRadius(const glm::vec4& borderThickness, const glm::vec4& cornerRadius) {
        return glm::max(glm::vec4(0.0f), cornerRadius - glm::vec4(
            glm::max(borderThickness.z, borderThickness.y), // BR
            glm::max(borderThickness.x, borderThickness.y), // TR
            glm::max(borderThickness.z, borderThickness.w), // BL
            glm::max(borderThickness.x, borderThickness.w)  // TL
        ));
    }

    SDRectInputData MakeSDRectInputData(glm::vec2 size, glm::vec4 margin, glm::vec4 borderThickness) {
        SDRectInputData sdInput{};

        glm::vec2 bgOffset = size * 0.5f;

        auto marginSpace = glm::vec2(margin.y + margin.w, margin.z + margin.x);
        auto borderSpace = glm::vec2(borderThickness.y + borderThickness.w, borderThickness.z + borderThickness.x);

        sdInput.size = bgOffset - marginSpace * 0.5f;
        sdInput.offsetAndTiling = glm::vec4(bgOffset, size);
        sdInput.pOffset = glm::vec2(margin.y - margin.w, margin.z - margin.x) * 0.5f;

        sdInput.iSize = sdInput.size - borderSpace * 0.5f;
        sdInput.iPOffset = glm::vec2(borderThickness.y - borderThickness.w, borderThickness.z - borderThickness.x) * 0.5f;

        return sdInput;
    }

    [[nodiscard]] glm::vec2 ComputeImageUVPositionX(const glm::vec2& tiling, const glm::vec2& borderTLOffset, const glm::vec2& imageRect, const ImageAlignment alignment) noexcept {
        float posY = -borderTLOffset.y;
        float posX = -borderTLOffset.x;

        switch (alignment) {
        case ImageAlignment::eTopLeft:
        case ImageAlignment::eBottomLeft:
        case ImageAlignment::eCenterLeft:
            return {posX, posY};

        case ImageAlignment::eTopCenter:
        case ImageAlignment::eBottomCenter:
        case ImageAlignment::eCenter:
            posX = -0.5f * (imageRect.x - 1 / tiling.x + borderTLOffset.x * 2);
            return {posX, posY};

        case ImageAlignment::eTopRight:
        case ImageAlignment::eBottomRight:
        case ImageAlignment::eCenterRight:
            posX = imageRect.x - 1 / tiling.x + borderTLOffset.x;
            return {-posX, posY};

        default:
            return {posX, posY};
        }
    }

    [[nodiscard]] glm::vec2 ComputeImageUVPositionY(const glm::vec2& tiling, const glm::vec2& borderTLOffset, const glm::vec2& imageRect, const ImageAlignment alignment) noexcept {
        float posY = -borderTLOffset.y;
        float posX = -borderTLOffset.x;
        switch (alignment) {
        case ImageAlignment::eBottomLeft:
        case ImageAlignment::eBottomRight:
        case ImageAlignment::eBottomCenter:
            posY = imageRect.y - 1 / tiling.y + borderTLOffset.y;
            return {posX, -posY};

        case ImageAlignment::eCenterLeft:
        case ImageAlignment::eCenterRight:
        case ImageAlignment::eCenter:
            posY = -0.5f * (imageRect.y - 1 / tiling.y + borderTLOffset.y * 2);
            return {posX, posY};

        case ImageAlignment::eTopRight:
        case ImageAlignment::eTopCenter:
            return {posX, posY};
        default:
            return {posX, posY};
        }
    }

    [[nodiscard]] glm::vec4 ComputeGlyphUVRect(const float elementWidth, const float elementHeight, const font::GlyphContext& glyphCtx, const Style& style) noexcept{
        //tiling
        const glm::vec2 imageFrame = style.getCropOutSpace();
        const glm::vec2 elementSize{elementWidth, elementHeight};
        const glm::vec2 trueElementSize = elementSize - imageFrame;

        // Guard against zero/negative content size
        if (trueElementSize.x <= 0.0f || trueElementSize.y <= 0.0f) {
            return {0.0f, 0.0f, 1.0f, 1.0f};
        }

        const glm::vec2 glyphSourceUVSize{glyphCtx.uvW, glyphCtx.uvH};
        const glm::vec2 tilingScaler = elementSize / trueElementSize;
        const glm::vec2 tiling = glyphSourceUVSize * tilingScaler;

        // Offset
        const glm::vec2 offsetSpacing = style.getUVTopLeftOffset() / trueElementSize / tilingScaler;
        const glm::vec2 atlasOffset = glm::vec2{glyphCtx.uvX, glyphCtx.uvY} / glyphSourceUVSize / tilingScaler;
        const glm::vec2 offset = atlasOffset - offsetSpacing;


        return {offset, tiling};
    }

    [[nodiscard]] glm::vec4 ComputeImageUVRect(const float elementWidth, const float elementHeight,const float textureWidth, const float textureHeight, const Style& style) noexcept{
        if (textureWidth <= 0.0f || textureHeight <= 0.0f || elementWidth <= 0.0f || elementHeight <= 0.0f)
            return {0.0f, 0.0f, 1.0f, 1.0f};

        const glm::vec2 imageFrame = style.getCropOutSpace();
        const ImageScaleMode& mode = style.imageScaleMode;
        const ImageAlignment& alignment = style.imageAlignment;

        const glm::vec2 elementSize{elementWidth, elementHeight};
        const glm::vec2 uvSize = elementSize - imageFrame;
        const glm::vec2 normalizedUVSize = uvSize / elementSize;
        const glm::vec2 texTiling = elementSize / uvSize;
        const glm::vec2 topLeftOffset = style.getUVTopLeftOffset() / elementSize ;

        const float elemAspect = elementWidth / elementHeight;
        const float texAspect  = textureWidth / textureHeight ;
        const float imageAspect = uvSize.x / uvSize.y ;

        switch (mode) {
        case ImageScaleMode::eStretchToFill:
            return {-topLeftOffset, texTiling};

        case ImageScaleMode::eScaleToFit: {
            if (imageAspect > texAspect) {
                const float uvWidth =  imageAspect / texAspect;
                const glm::vec2 tiling = glm::vec2{uvWidth, 1.0f} * texTiling;
                const glm::vec2 offset = ComputeImageUVPositionX(tiling, topLeftOffset, normalizedUVSize, alignment);
                return {offset, tiling } ;
            }
            const float uvHeight = texAspect / imageAspect;
            const glm::vec2 tiling = glm::vec2{1.0f, uvHeight} * texTiling;
            const glm::vec2 offset = ComputeImageUVPositionY(tiling, topLeftOffset, normalizedUVSize, alignment) ;
            return {offset, tiling};
        }

        case ImageScaleMode::eScaleAndCrop: {
            if (elemAspect > texAspect) {
                const float uvHeight = texAspect / elemAspect;
                return {0.0f, (1.0f - uvHeight) * 0.5f, 1.0f, uvHeight};
            }
            const float uvWidth =elemAspect / texAspect ;
            return {(1.0f - uvWidth) * 0.5f, 0.0f, uvWidth, 1.0f};
        }
        }
        return {-topLeftOffset, texTiling};
    }


    void MarkGUITreeStructureChanged(UIDocument& document) noexcept;
    void MarkGUILayoutChanged(UIDocument& document) noexcept;
    void MarkGUIStyleChanged(UIDocument& document) noexcept;

    enum class ElementEventType : uint8_t {
        AttachmentEvent, DetachEvent,
        PointerMotion, PointerEnter, PointerLeave,
        PointerDown, PointerUp, PointerClick, PointerWheel
    };

    struct ElementPointerEvent {
        glm::vec2        pos{0};            // window space
        glm::vec2        local{0};          // relative to currentTarget's visible rect
        glm::vec2        delta{0};
        PointerButton    button{PointerButton::eNone};
        uint8_t          clicks{0};
        VisualElement*   target{nullptr};
        VisualElement*   currentTarget{nullptr};
        bool             stopped{false};
        void stopPropagation() noexcept { stopped = true; }
    };
    using ElementPointerHandler = void(*)(ElementPointerEvent& e, void* user);

    using AttachmentEventHandler = void(*)(void* user);
    using DetachEventHandler = void(*)(void* user);
    using PointerMotionEventHandler = void(*)(ElementPointerEvent& e, void* user);
    using PointerEnterEventHandler = void(*)(ElementPointerEvent& e, void* user);
    using PointerLeaveEventHandler = void(*)(ElementPointerEvent& e, void* user);
    using PointerDownEventHandler = void(*)(ElementPointerEvent& e, void* user);
    using PointerUpEventHandler = void(*)(ElementPointerEvent& e, void* user);
    using PointerClickEventHandler = void(*)(ElementPointerEvent& e, void* user);
    using PointerWheelEventHandler = void(*)(ElementPointerEvent& e, void* user);


    template<ElementEventType Type>
    struct EventTraits;

    template<>
    struct EventTraits<ElementEventType::AttachmentEvent> {
        using Handler = AttachmentEventHandler;
    };

    template<>
    struct EventTraits<ElementEventType::DetachEvent> {
        using Handler = DetachEventHandler;
    };

    template<>
    struct EventTraits<ElementEventType::PointerEnter> {
        using Handler = PointerEnterEventHandler;
    };

    template<>
    struct EventTraits<ElementEventType::PointerLeave> {
        using Handler = PointerLeaveEventHandler;
    };

    template<>
    struct EventTraits<ElementEventType::PointerMotion> {
        using Handler = PointerMotionEventHandler;
    };

    template<>
    struct EventTraits<ElementEventType::PointerUp> {
        using Handler = PointerUpEventHandler;
    };

    template<>
        struct EventTraits<ElementEventType::PointerDown> {
        using Handler = PointerDownEventHandler;
    };

    template<>
        struct EventTraits<ElementEventType::PointerClick> {
        using Handler = PointerClickEventHandler;
    };

    template<>
        struct EventTraits<ElementEventType::PointerWheel> {
        using Handler = PointerWheelEventHandler;
    };

    struct PendingPointer {
        VisualElement*      target;
        ElementEventType    type;
        ElementPointerEvent ev;
        bool                bubbles;
    };


    [[nodiscard]] StyleStaticData MakeStyleStaticData(const Style& style) noexcept {
        StyleStaticData s{};
        s.radius            = style.getCornerRadiusSet();
        s.iRadius           = CalculateInnerRadius(style.getBorderThicknessSet(),
                                                   style.getCornerRadiusSet());
        s.imageColor        = style.imageColor;
        s.backgroundColor   = style.backgroundColor;
        s.borderTopColor    = style.borderTopColor;
        s.borderLeftColor   = style.borderLeftColor;
        s.borderRightColor  = style.borderRightColor;
        s.borderBottomColor = style.borderBottomColor;
        s.imageRepeatMode   = static_cast<uint32_t>(style.imageRepeatMode);

        return s;
    }

    [[nodiscard]] StyleStaticData MakeFontStyleStaticData(glm::vec4 fontColor) {
        StyleStaticData sd{};
        sd.isFont = 1u;
        sd.radius = sd.iRadius = glm::vec4(0.0f);
        sd.imageColor = fontColor;
        sd.backgroundColor = glm::vec4(0.0f);
        sd.borderTopColor = sd.borderBottomColor = sd.borderLeftColor = sd.borderRightColor = glm::vec4(0.0f);
        sd.imageRepeatMode = 0u;
        return sd;
    }

    class GUIPaletteManager {
    public:
        explicit GUIPaletteManager(UFoxWindow* window, UIDocument* document) : _window(window), _document(document){
            if (_window != nullptr) {
                _window->registerCallbackEvent<EventType::eSystemInit>([](void* user) { static_cast<GUIPaletteManager*>(user)->init();}, this);
            }
        }

        ~GUIPaletteManager() {
            destroyBuffer();
            if (_window != nullptr) {
                _window->unregisterCallbackEvent(EventType::eSystemInit,this);
            }
        }

        void addPreset(const std::string_view name,const StyleStaticData& data) {
            _stylePreset.emplace(KeyID{name},data);

            MarkGUIStyleChanged(*_document);
        }

        void addPreset(const std::string_view name,const Style& style) {
            const auto data = MakeStyleStaticData(style);
            _stylePreset.emplace(KeyID{name},data);

            MarkGUIStyleChanged(*_document);
        }

        void removePreset(const std::string_view name) {
            if (const auto it = _stylePreset.find(KeyID{name}); it != _stylePreset.end()) {
                _stylePreset.erase(it);
            }

            MarkGUIStyleChanged(*_document);
        }

        void syncPresetToGpuSlot() {
            _staticMap.clear();
            _validatedPresetCount = 0;

            for (const auto &data : _stylePreset | std::views::values) {
                if (!_staticMap.contains(data)) {
                    _staticMap.emplace(data, _validatedPresetCount);
                    StyleStaticData* gpuSlot = static_cast<StyleStaticData*>(_stylePaletteMemory) + _validatedPresetCount;
                    *gpuSlot = data;
                    _validatedPresetCount++;
                }
            }
        }

        void clearDynamicMap() {
            _dynamicMap.clear();
        }

        [[nodiscard]] uint32_t getOrCreateStyle(const StyleStaticData& style) {
            if (const auto it = _staticMap.find(style); it != _staticMap.end()) {
                return it->second;
            }

            if (const auto it = _dynamicMap.find(style); it != _dynamicMap.end()) {
                return it->second;
            }
            const auto newIndex = _validatedPresetCount + static_cast<uint32_t>(_dynamicMap.size()) ;

            _dynamicMap.emplace(style, newIndex);
            StyleStaticData* gpuSlot = static_cast<StyleStaticData*>(_stylePaletteMemory) + newIndex;
            *gpuSlot = style;
            
            return newIndex;
        }


        [[nodiscard]] vk::DescriptorBufferInfo  makeDescriptorBufferInfo() const {
            assert(_stylePaletteMemory && "Style Palette Buffer not initialized");

            vk::DescriptorBufferInfo info{};
            info
                .setBuffer(*_stylePaletteSSBO.data)
                .setOffset(0)
                .setRange(sizeof(StyleStaticData) * MAX_GUI_ELEMENTS );

            return info;
        }

    private:


        void init() {
            makeStylePaletteSSBO();
        }

        void makeStylePaletteSSBO() {
            if (_window == nullptr) return;
            const gpu::GPUResources& gpu = _window->gpuResource;
            destroyBuffer();

            const auto limited = gpu.maxStorageBufferRange / sizeof(StyleStaticData);
            _maxStyles = std::min(MAX_GUI_ELEMENTS, static_cast<uint32_t>(limited));
            const vk::DeviceSize size = sizeof(StyleStaticData) * _maxStyles;

            gpu::MakeBuffer(_stylePaletteSSBO, gpu, size,
                            vk::BufferUsageFlagBits::eStorageBuffer,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            _stylePaletteMemory = _stylePaletteSSBO.memory->mapMemory(0, size);

            addPreset("Default", DEFAULT_STYLE);
        }

        void destroyBuffer() noexcept {
            if (_stylePaletteMemory && _stylePaletteSSBO.memory) {
                _stylePaletteSSBO.memory->unmapMemory();
            }
            _stylePaletteMemory = nullptr;
        }

        struct StyleEqual {
            bool operator()(const StyleStaticData& a, const StyleStaticData& b) const noexcept {
                return a.imageRepeatMode == b.imageRepeatMode &&
                       a.isFont          == b.isFont &&
                       a.radius          == b.radius &&
                       a.iRadius         == b.iRadius &&
                       a.imageColor      == b.imageColor &&
                       a.backgroundColor == b.backgroundColor &&
                       a.borderBottomColor == b.borderBottomColor &&
                       a.borderTopColor    == b.borderTopColor &&
                       a.borderLeftColor   == b.borderLeftColor &&
                       a.borderRightColor  == b.borderRightColor;
            }
        };

        UFoxWindow*                                                                             _window{nullptr};
        UIDocument*                                                                               _document{nullptr};
        gpu::Buffer                                                                             _stylePaletteSSBO{};
        void*                                                                                   _stylePaletteMemory{nullptr};

        // Cache Maps
        std::unordered_map<StyleStaticData, uint32_t, std::hash<StyleStaticData>, StyleEqual>   _staticMap{};
        std::unordered_map<StyleStaticData, uint32_t, std::hash<StyleStaticData>, StyleEqual>   _dynamicMap{};
        std::unordered_map<KeyID,StyleStaticData, KeyIDHash, KeyIDEq>            _stylePreset{};
        uint32_t                                                                                _validatedPresetCount{0};
        uint32_t                                                                                _maxStyles{0};
    };

    [[nodiscard]] const font::Glyph* GetGlyph(const UIDocument& doc, const Style& style) noexcept;

    class VisualElement: public discadelta::DiscadeltaRectLayoutBase<VisualElement> {
    public:
        UIDocument*                                               document{nullptr};
        std::string_view                                        name;
        Style                                                   style{};
        size_t                                                  ssboIndex = 0;
        ElementData                                             elementData{};
        ElementData*                                            gpuSlot{nullptr};
        std::vector<VisualElement*>                             leafChildren{};
        std::vector<VisualElement*>                             branchChildren{};
        uint32_t                                                flatLeafStartSsboIndex = 0;
        uint32_t                                                flatLeafCount = 0;
        uint32_t                                                styleDataIndex = 0;

        const font::GlyphContext* const                         glyphContext{nullptr};

        virtual ~VisualElement() = default;

        [[nodiscard]] virtual uint32_t extraSlotCount() const noexcept { return 0; }
        [[nodiscard]] virtual bool isBatchableLeaf() const noexcept { return isContainerEmpty(); }

        VisualElement(const KeyID& id, std::string_view name_): VisualElement(id, name_, nullptr){}

        VisualElement(const KeyID& id, std::string_view name_,const font::GlyphContext* glyph_): DiscadeltaRectLayoutBase(id), name(name_), glyphContext(glyph_){initVisualElement();}

        explicit VisualElement(std::string_view name_): VisualElement(KeyID{name_}, name_, nullptr){}

        [[nodiscard]] bool isGlyphElement() const noexcept {return glyphContext != nullptr;}

        [[nodiscard]] ElementData makeUniformData(const TextureManager& textureManager, GUIPaletteManager& paletteManager)  {
            const KeyID imageID{style.backgroundImage};
            auto [texWidth, texHeight] = textureManager.getTextureSize<float>(imageID);

            StyleStaticData styleData{};
            styleData.radius            = style.getCornerRadiusSet();
            styleData.iRadius           = CalculateInnerRadius(style.getBorderThicknessSet(), style.getCornerRadiusSet() );
            styleData.imageColor        = style.imageColor;
            styleData.backgroundColor   = style.backgroundColor;
            styleData.borderTopColor    = style.borderTopColor;
            styleData.borderLeftColor   = style.borderLeftColor;
            styleData.borderRightColor  = style.borderRightColor;
            styleData.borderBottomColor = style.borderBottomColor;

            styleData.imageRepeatMode   = static_cast<uint32_t>(style.imageRepeatMode);

            styleDataIndex = paletteManager.getOrCreateStyle(styleData);

            ElementData data{};
            data.model = getTransformMat4();
            data.sdInput = MakeSDRectInputData(glm::vec2(width, height), style.getMarginSet(), style.getBorderThicknessSet());
            data.imageOffsetAndTiling = isGlyphElement()? ComputeGlyphUVRect(width,height, *glyphContext, style): ComputeImageUVRect(width, height, texWidth, texHeight, style);
            data.styleIndex = styleDataIndex;
            data.imageID = textureManager.getTextureIndex(imageID);
            data.scrollingOffset = {0,0};//not yet feature

            return data;
        }

        void markStyleChanged() noexcept {
            UIDocument* doc = root ? root->document : document;
            if (doc != nullptr) MarkGUIStyleChanged(*doc);
        }

        // Called once per rebuild: assigns slot, writes initial data, registers pointer
        void bindGpuSlot(ElementData* slot, const TextureManager& tex, GUIPaletteManager& palette) {
            gpuSlot = slot;
            syncToGpu(tex, palette);
        }

        // Called on layout change — writes directly to mapped memory, no copy
        virtual void syncToGpu(const TextureManager& tex, GUIPaletteManager& palette) {
            if (!gpuSlot) return;
            const KeyID imageID{style.backgroundImage};
            auto [tw, th] = tex.getTextureSize<float>(imageID);

            const StyleStaticData styleData = MakeStyleStaticData(style);
            styleDataIndex = palette.getOrCreateStyle(styleData);

            elementData.model              = getTransformMat4();
            elementData.sdInput            = MakeSDRectInputData(glm::vec2(width, height),
                                                 style.getMarginSet(),
                                                 style.getBorderThicknessSet());
            elementData.imageOffsetAndTiling = isGlyphElement()? ComputeGlyphUVRect(width,height, *glyphContext, style):ComputeImageUVRect(width, height, tw, th, style);
            elementData.styleIndex         = styleDataIndex;
            elementData.imageID            = tex.getTextureIndex(imageID);
            elementData.scrollingOffset    = {0.0f, 0.0f};

            *gpuSlot = elementData; // single struct copy directly into mapped memory
        }

        void setAsRenderRoot(UIDocument* document) {
            this->document = document;
        }

        template<ElementEventType Type>
        void registerCallbackEvent(EventTraits<Type>::Handler callback, void* user) {
            getEventList<Type>().add(callback, user);
        }

        void unregisterCallbackEvent(const ElementEventType type, void* user) {
            switch (type) {
            case ElementEventType::AttachmentEvent: onAttachment.remove(user);break;
            case ElementEventType::DetachEvent: onDetachment.remove(user);break;
            case ElementEventType::PointerMotion: onPointerMotion.remove(user);break;
            case ElementEventType::PointerEnter: onPointerEnter.remove(user);break;
            case ElementEventType::PointerLeave: onPointerLeave.remove(user);break;
            case ElementEventType::PointerDown: onPointerDown.remove(user);break;
            case ElementEventType::PointerUp: onPointerUp.remove(user);break;
            case ElementEventType::PointerClick: onPointerClick.remove(user);break;
            case ElementEventType::PointerWheel: onPointerWheel.remove(user);break;
            }
        }

        template<ElementEventType Type>
        auto& getEventList() {
            if constexpr (Type == ElementEventType::AttachmentEvent) {
                return onAttachment;
            }
            else if constexpr (Type == ElementEventType::DetachEvent) {
                return onDetachment;
            }
            else if constexpr (Type == ElementEventType::PointerMotion) {
                return onPointerMotion;
            }
            else if constexpr (Type == ElementEventType::PointerEnter) {
                return onPointerEnter;
            }
            else if constexpr (Type == ElementEventType::PointerLeave) {
                return onPointerLeave;
            }
            else if constexpr (Type == ElementEventType::PointerDown) {
                return onPointerDown;
            }
            else if constexpr (Type == ElementEventType::PointerUp) {
                return onPointerUp;
            }
            else if constexpr (Type == ElementEventType::PointerClick) {
                return onPointerClick;
            }
            else if constexpr (Type == ElementEventType::PointerWheel) {
                return onPointerWheel;
            }
        }

        [[nodiscard]] VisualElement* parentElement() const noexcept {
            return dynamic_cast<VisualElement*>(parent);
        }

        [[nodiscard]] bool wantsPointer() const noexcept {
            return !(onPointerMotion.empty() && onPointerEnter.empty() && onPointerLeave.empty() &&
                     onPointerDown.empty() && onPointerUp.empty() && onPointerClick.empty() && onPointerWheel.empty());
        }

        // topmost element under p, following draw order (last branch is drawn last = on top)
        [[nodiscard]] VisualElement* hitTest(const glm::vec2& p) {
            const glm::vec2 tl   = glm::vec2(x, y) + glm::vec2(style.marginLeft, style.marginTop);
            const glm::vec2 size = glm::vec2(width, height) - style.getMarginSpace();
            const bool inside = p.x >= tl.x && p.y >= tl.y && p.x < tl.x + size.x && p.y < tl.y + size.y;

            if (!inside && style.displayOverflowMode == DisplayOverflowMode::eHidden) return nullptr;  // clipped

            for (auto it = branchChildren.rbegin(); it != branchChildren.rend(); ++it)
                if (auto* h = (*it)->hitTest(p)) return h;
            for (auto it = leafChildren.rbegin(); it != leafChildren.rend(); ++it)
                if (auto* h = (*it)->hitTest(p)) return h;
            return inside ? this : nullptr;
        }

        void dispatchPointer(const ElementEventType type, ElementPointerEvent& e) {
            e.currentTarget = this;
            e.local = e.pos - (glm::vec2(x, y) + glm::vec2(style.marginLeft, style.marginTop));
            switch (type) {
            case ElementEventType::PointerMotion: if (!onPointerMotion.empty()) onPointerMotion.execute(e); break;
            case ElementEventType::PointerEnter:  if (!onPointerEnter.empty())  onPointerEnter.execute(e);  break;
            case ElementEventType::PointerLeave:  if (!onPointerLeave.empty())  onPointerLeave.execute(e);  break;
            case ElementEventType::PointerDown:   if (!onPointerDown.empty())   onPointerDown.execute(e);   break;
            case ElementEventType::PointerUp:     if (!onPointerUp.empty())     onPointerUp.execute(e);     break;
            case ElementEventType::PointerClick:  if (!onPointerClick.empty())  onPointerClick.execute(e);  break;
            case ElementEventType::PointerWheel:  if (!onPointerWheel.empty())  onPointerWheel.execute(e);  break;
            default: break;
            }
        }

    protected:
        VisualElement*                      root = nullptr;
        void onTreeStructureChanged() override {
            if (root) {
                if (root->document) {
                    MarkGUITreeStructureChanged(*root->document);
                    MarkGUILayoutChanged(*root->document);
                }
            }else if (document) {
                MarkGUITreeStructureChanged(*document);
                MarkGUILayoutChanged(*document);
            }
        }

        void executeOnAttachment() {
            if (onAttachment.empty()) return;
            onAttachment.execute();
        }

        void executeOnDetachment() {
            if (onDetachment.empty()) return;
            onDetachment.execute();
        }
    private:
        EventList<AttachmentEventHandler>    onAttachment{};
        EventList<DetachEventHandler>        onDetachment{};
        EventList<PointerEnterEventHandler>  onPointerEnter{};
        EventList<PointerLeaveEventHandler>  onPointerLeave{};
        EventList<PointerMotionEventHandler> onPointerMotion{};
        EventList<PointerUpEventHandler>     onPointerUp{};
        EventList<PointerDownEventHandler>   onPointerDown{};
        EventList<PointerClickEventHandler>  onPointerClick{};
        EventList<PointerWheelEventHandler>  onPointerWheel{};


        void initVisualElement() {
            baseStyle = &style;
        }

        void onAdd(VisualElement* target) override {
            if (target->isBatchableLeaf()) addChildToLeaf(target); else addChildToBranch(target);
            switchToBranch();
            target->root = root? root : this;
            target->executeOnAttachment();
        }
        void onRemove(VisualElement* target) override {
            if (UIDocument* doc = root ? root->document : document)
                ForgetPointerTarget(*doc, target);

            std::erase(leafChildren,target);
            std::erase(branchChildren,target);
            switchToLeaf();
            target->root = nullptr;
            target->executeOnDetachment();
        }

        void addChildToBranch(VisualElement* child) {
            if (const auto it = std::ranges::find(branchChildren, child);
                it != branchChildren.end()) return;
            branchChildren.push_back(child);
        }

        void addChildToLeaf(VisualElement* child) {
            if (const auto it = std::ranges::find(leafChildren, child);
                it != leafChildren.end()) return;
            leafChildren.push_back(child);
        }

        void switchToBranch() {
            if (!parent) return;
            auto* p = dynamic_cast<VisualElement*>(parent);
            if (const auto it = std::ranges::find(p->branchChildren, this);
                it != p->branchChildren.end()) return;
            p->branchChildren.push_back(this);
            std::erase(p->leafChildren, this);
        }

        void switchToLeaf() {
            if (!parent || !isBatchableLeaf()) return;
            auto* p = dynamic_cast<VisualElement*>(parent);
            if (const auto it = std::ranges::find(p->leafChildren, this);
                it != p->leafChildren.end()) return;
            p->leafChildren.push_back(this);
            std::erase(p->branchChildren, this);
        }


    };

    class Label final : public VisualElement {
    public:

        Label(const KeyID& id, std::string_view name,std::string_view utf8 = {}, float fontSize = 12.0f, float lineHeightEm = 1.5f): VisualElement(id, name), text_(DecodeUtf8(utf8)), lineHeightEm_(lineHeightEm){
           init(fontSize);
        }

        ~Label() override {
            release();
        }

        void setText(std::string_view utf8) { text_ = DecodeUtf8(utf8); rebuild(); }
        void setFontSize(float px)          { style.fontSize = px;      rebuild(); }
        void setLineHeight(float em)        { lineHeightEm_ = em;       rebuild(); }
        void setColor(const glm::vec4& c)   { style.imageColor = c; }   // text color

        uint32_t extraSlotCount() const noexcept override { return visible_; }
        bool isBatchableLeaf() const noexcept override { return false; }

        void syncToGpu(const TextureManager& tex, GUIPaletteManager& palette) override {
            VisualElement::syncToGpu(tex, palette);            // background box of the label
            if (!gpuSlot || visible_ == 0) return;
            if (font_ == nullptr) return;

            // one shared style for all glyphs of this label
            StyleStaticData sd = MakeFontStyleStaticData(style.fontColor);
            const uint32_t styleIdx = palette.getOrCreateStyle(sd);
            const uint32_t atlasIdx = tex.getTextureIndex(KeyID{font_->atlasTextureID.view()});

            const float fs    = style.fontSize;
            const auto& m     = font_->metrics;                          // ascender > 0, descender < 0 (em)
            const glm::vec2 o = glm::vec2(x, y) + style.getUVTopLeftOffset();   // content origin
            const float baseline = std::round(o.y + (((lineHeightEm_ - static_cast<float>(m.ascender - m.descender)) * 0.5f)
                                                     + static_cast<float>(m.ascender)) * fs);
            static const Style kNoFrame{};                                // zero margin/border
            float penX = o.x;
            ElementData* out = gpuSlot + 1;

            for (const auto* g : glyphs_) {
                if (g->uvW > 0.0f && g->uvH > 0.0f) {
                    const float qw = (g->planeRight - g->planeLeft) * fs;
                    const float qh = (g->planeTop   - g->planeBottom) * fs;
                    const float qx = penX + g->planeLeft * fs;
                    const float qy = baseline - g->planeTop * fs;         // y-down, overhang is fine

                    ElementData d{};
                    d.model = MakeTransformModel(glm::vec3{qx, qy, 0.0f}, IDENTITY_QUAT, glm::vec3{qw, qh, 1.0f});
                    d.sdInput = MakeSDRectInputData({qw, qh}, glm::vec4(0.0f), glm::vec4(0.0f));
                    d.imageOffsetAndTiling = ComputeGlyphUVRect(qw, qh, *g, kNoFrame);
                    d.styleIndex = styleIdx;
                    d.imageID = atlasIdx;

                    *out++ = d;
                }
                penX += static_cast<float>(g->advance) * fs;                           // kerning goes here later
            }
        }

    private:
        const font::Glyph*                          font_{nullptr};
        std::u32string                              text_{};
        std::vector<const font::GlyphContext*>      glyphs_{};
        uint32_t                                    visible_{0};
        float                                       lineHeightEm_{1.5f};

        void init(const float fontSize) {
            style.fontSize = fontSize;
            style.fontType = font::FontType::eRegular;
            style.fontFamily = "NotoSans";
            style.backgroundColor = glm::vec4(0.0f);

            registerCallbackEvent<ElementEventType::AttachmentEvent>([](void* user) {
                auto label = static_cast<Label*>(user);
                label->rebuild();
            }, this);
        }

        void release() {
            unregisterCallbackEvent(ElementEventType::AttachmentEvent, this);
        }

        void resolveFont() {
            if (font_ != nullptr) return;
            if (root == nullptr || root->document == nullptr) return;     // not attached yet
            font_ = GetGlyph(*root->document, style);
            if (font_ == nullptr)
                debug::log(debug::LogLevel::eError, "Font not found - {}", style.fontFamily);
        }

        void rebuild() {
            resolveFont();

            glyphs_.clear();
            visible_ = 0;
            float w = 0.0f;
            const float fs = style.fontSize;

            if (font_ != nullptr) {
                for (const char32_t cp : text_) {
                    const auto* g = font_->find(cp);
                    if (!g) g = font_->find('?');
                    if (!g) continue;
                    glyphs_.push_back(g);
                    w += static_cast<float>(g->advance) * fs;
                    if (g->uvW > 0.0f && g->uvH > 0.0f) ++visible_;
                }
            }

            style.width  = discadelta::Length(discadelta::LengthMode::eFlat, w);
            style.height = discadelta::Length(discadelta::LengthMode::eFlat, lineHeightEm_ * fs);
            style.flexExpand = 0.0f;
            style.flexCompress = 0.0f;

            updateMetrics();
            onTreeStructureChanged();
        }

        static std::u32string DecodeUtf8(std::string_view s) {
            std::u32string out;
            for (size_t i = 0; i < s.size();) {
                const unsigned char c = s[i];
                const int n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
                char32_t cp = n == 1 ? c : c & (0xFF >> (n + 1));
                for (int k = 1; k < n && i + k < s.size(); ++k) cp = (cp << 6) | (s[i + k] & 0x3F);
                out.push_back(cp);
                i += n;
            }
            return out;
        }
    };

    uint32_t AssignGpuSlots(VisualElement& element,ElementData* mappedBase, uint32_t slotIndex,const TextureManager& tex,GUIPaletteManager& palette){
        // Assign this element
        element.ssboIndex = slotIndex;
        element.bindGpuSlot(mappedBase + slotIndex, tex, palette);
        slotIndex += 1 + element.extraSlotCount();

        // Leaf batch — contiguous block, same as before
        element.flatLeafStartSsboIndex = slotIndex;
        element.flatLeafCount          = static_cast<uint32_t>(element.leafChildren.size());

        for (auto* leaf : element.leafChildren) {
            leaf->ssboIndex = slotIndex;
            leaf->bindGpuSlot(mappedBase + slotIndex, tex, palette);
            slotIndex++;
        }

        // Branch recursion
        for (auto* branch : element.branchChildren)
            slotIndex = AssignGpuSlots(*branch, mappedBase, slotIndex, tex, palette);

        return slotIndex;
    }

    void RecordElementDrawCommands(const vk::raii::CommandBuffer& cmb,const VisualElement& element,uint32_t activeStencilValue,uint32_t& nextAvailableStencil) {
        // ------------------------------------------------------------
        // STEP 1: Render Current Element Background Mesh (The Container)
        // ------------------------------------------------------------
        cmb.setStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, 0x00); // Lock writing to protect existing masks
        cmb.setStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 0xFF);
        cmb.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, activeStencilValue);

        cmb.setStencilOp(
            vk::StencilFaceFlagBits::eFrontAndBack,
            vk::StencilOp::eKeep,
            vk::StencilOp::eKeep,
            vk::StencilOp::eKeep,
            activeStencilValue == 0 ? vk::CompareOp::eAlways : vk::CompareOp::eEqual
        );

        // Draw the visible background plate for this element
        cmb.drawIndexed(
            std::size(RectIndices),
            1,              // instanceCount is exactly 1 for the parent
            0, 0,
            static_cast<uint32_t>(element.ssboIndex) // Passes to shader as SV_StartInstanceLocation
        );

        // Draw Sub Elements
        if (const uint32_t n = element.extraSlotCount(); n > 0)
            cmb.drawIndexed(std::size(RectIndices), n, 0, 0, static_cast<uint32_t>(element.ssboIndex) + 1);

        // If this element has absolutely zero children, we stop immediately
        if (element.leafChildren.empty() && element.branchChildren.empty()) return;

        // Initialize the tracking ID that children under this element will inherit
        uint32_t stencilValueForChildren = activeStencilValue;

        // ------------------------------------------------------------
        // STEP 2: Configure Stencil Buffer Mask If Overflow::Hidden
        // ------------------------------------------------------------
        if (element.style.displayOverflowMode == DisplayOverflowMode::eHidden) {
            uint32_t maskWriteValue = nextAvailableStencil++;

            cmb.setStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, 0xFF); // Unlock stencil writing
            cmb.setStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 0xFF);

            // Dynamic compare switch: ALWAYS pass, REPLACE buffer value with our new mask layer ID
            cmb.setStencilOp(
                vk::StencilFaceFlagBits::eFrontAndBack,
                vk::StencilOp::eKeep,
                vk::StencilOp::eReplace, // Forces the GPU to physically write the mask boundary shape
                vk::StencilOp::eKeep,
                activeStencilValue == 0 ? vk::CompareOp::eAlways : vk::CompareOp::eEqual
            );

            cmb.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, maskWriteValue);

            // Re-draw the shared quad geometry to carve the clipping box into the stencil buffer
            cmb.drawIndexed(std::size(RectIndices), 1, 0, 0, static_cast<uint32_t>(element.ssboIndex));

            // Downstream children must now evaluate against this newly updated mask value
            stencilValueForChildren = maskWriteValue;
        }

        // ------------------------------------------------------------
        // STEP 3: YOUR THEORY - Batch Render All Flat Leaf Children
        // ------------------------------------------------------------
        if (element.flatLeafCount > 0) {

            // 1. LOCK STENCIL WRITING: Stop child items from corrupting our mask canvas
            cmb.setStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, 0x00);
            cmb.setStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 0xFF);
            cmb.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, stencilValueForChildren);

            // 2. SET TEST-ONLY MODE: Fail, DepthFail, and Pass must retain the mask value (eKeep)
            // Only render fragment pixels if they sit perfectly inside our parent mask (eEqual)
            cmb.setStencilOp(
                vk::StencilFaceFlagBits::eFrontAndBack,
                vk::StencilOp::eKeep,
                vk::StencilOp::eKeep,
                vk::StencilOp::eKeep,
                stencilValueForChildren == 0 ? vk::CompareOp::eAlways : vk::CompareOp::eEqual
            );

            // 3. DISPATCH THE ENTIRE CONTIGUOUS SIBLING BATCH AT ONCE
            cmb.drawIndexed(
                std::size(RectIndices),
                element.flatLeafCount,
                0, 0,
                element.flatLeafStartSsboIndex
            );
        }

        // ------------------------------------------------------------
        // STEP 4: Process Complex Branch Containers Individually
        // ------------------------------------------------------------
        // Complex children are skipped from the batch phase above and processed safely here,
        // allowing them to cleanly establish their own sub-nested clipping hierarchies!
        for (auto* complexChild : element.branchChildren) {
            RecordElementDrawCommands(cmb, *complexChild, stencilValueForChildren, nextAvailableStencil);
        }
    }

    void SyncSubtree(VisualElement& el, const TextureManager& textureManager, GUIPaletteManager& paletteManager) {
        el.syncToGpu(textureManager, paletteManager);
        for (auto* leaf   : el.leafChildren)   leaf->syncToGpu(textureManager, paletteManager);
        for (auto* branch : el.branchChildren) SyncSubtree(*branch, textureManager, paletteManager);
    }

    inline KeyID TESTER_TEXTURE_ID{"grok-image-b9472c20-0cfc-4f3d-b558-d5c3066aa081.jpg"};

    class  UIDocument {
    public:
        explicit UIDocument(UFoxWindow& _window, MeshManager& _meshManager, font::GlyphManager& _glyphManager, TextureManager* _textureManager = nullptr) : window(&_window) , meshManager(&_meshManager), glyphManager(&_glyphManager), textureManager(_textureManager) {
            window->registerCallbackEvent<EventType::eSystemInit>([](void* user) { static_cast<UIDocument*>(user)->init();}, this);
            window->registerCallbackEvent<EventType::ePostSystemInit>([](const float& w, const float& h, void* user) { static_cast<UIDocument*>(user)->onPostSystemInit(w,h);}, this);
            window->registerCallbackEvent<EventType::eStart>([](void* user) { static_cast<UIDocument*>(user)->onStart();}, this);
            window->registerCallbackEvent<EventType::eResize>([](const float& w, const float& h, void* user) {  static_cast<UIDocument*>(user)->onResize(w,h);}, this);
            //window->registerCallbackEvent<EventType::eGainsFocus>([](void* user) { static_cast<UIDocument*>(user)->onGainsFocus();}, this);
            window->registerCallbackEvent<EventType::ePreDraw>([](void* user) { static_cast<UIDocument*>(user)->onPreDraw();}, this);
            window->registerCallbackEvent<EventType::eUpdateBuffer>([](const uint32_t& currentImage, void* user){ static_cast<UIDocument*>(user)->onLayoutChanged(currentImage);}, this);
            window->registerCallbackEvent<EventType::eRenderPass>([](const vk::raii::CommandBuffer& cmb,const uint32_t& imageIndex, const windowing::WindowResource& winResource,
            const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& stencilAttachment, void* user ) { static_cast<UIDocument*>(user)->drawCanvas(cmb, imageIndex, winResource, colorAttachment, stencilAttachment);}, this);
            window->registerCallbackEvent<EventType::ePointer>([](const PointerEvent& e, void* user) { return static_cast<UIDocument*>(user)->onPointer(e); }, this);
            window->registerCallbackEvent<EventType::eProcessInput>([](void* user) { static_cast<UIDocument*>(user)->flushPointerQueue(); }, this);paletteManager = std::make_unique<GUIPaletteManager>(window, this);
        }

        ~UIDocument() {
            hovered_ = nullptr;
            pressed_ = nullptr;
            pointerQueue_.clear();
            rootElement.reset();

            if (window != nullptr) {
                window->unregisterCallbackEvent(EventType::eSystemInit,this);
                window->unregisterCallbackEvent(EventType::ePostSystemInit,this);
                window->unregisterCallbackEvent(EventType::eStart,this);
                window->unregisterCallbackEvent(EventType::eResize,this);
                //window->unregisterCallbackEvent(EventType::eGainsFocus,this);
                window->unregisterCallbackEvent(EventType::ePreDraw,this);
                window->unregisterCallbackEvent(EventType::eUpdateBuffer,this);
                window->unregisterCallbackEvent(EventType::eRenderPass,this);
                window->unregisterCallbackEvent(EventType::ePointer, this);
                window->unregisterCallbackEvent(EventType::eProcessInput, this);

                window = nullptr;
            }

            if (meshManager != nullptr) {
                meshManager = nullptr;
            }

            if (textureManager != nullptr) {
                textureManager = nullptr;
            }
        }

        void init() {
            const auto& gpuResources = window->gpuResource;
            repeatSamplers.reserve(REPEAT_SAMPLER_CONFIGS.size());

            for (auto [addressModeU, addressModeV, addressModeW, borderColor] : REPEAT_SAMPLER_CONFIGS) {
                gpu::MakeSampler(
                    gpuResources,
                    repeatSamplers,
                    addressModeU,
                    addressModeV,
                    addressModeW,
                    borderColor
                );
            }

            rootElement = std::make_unique<VisualElement>(KeyID("root"),"Root");
            rootElement->setAsRenderRoot(this);
            rootElement->style.backgroundColor = glm::vec4{0.0f, 0.0f, 0.0f, 0.0f};
            rootElement->style.direction = discadelta::FlexDirection::eRow;
            rootElement->style.displayOverflowMode = DisplayOverflowMode::eHidden;
            rootElement->style.justifyContent = discadelta::JustifyContentMode::eStart;
            rootElement->style.flexCompress =1.0f;
            rootElement->style.flexExpand =1.0f;
            rootElement->style.alignItems = discadelta::AlignMode::eStart;

            debug::log(debug::LogLevel::eInfo, "GUI UIDocument Init : Susses");
        }

        void onPostSystemInit(const float& width, const float& height) {
            if (meshManager != nullptr){
                ResourceContextCreateInfo info{};
                info.setName(RECT_MESH_NAME)
                    .setSourceType(SourceType::eBuiltIn)
                    .setCategory("GUI");
                meshID = MakeMeshContent(*meshManager, RectVertices, RectIndices, info);
                mesh = meshManager->getMesh(*meshID);
            }

            samplerImageCount = textureManager->getMaxDescriptorSetSamplerImages(samplerImageCount);

            const gpu::GPUResources& gpu = window->gpuResource;
            const std::vector<char> shaderCode = UFoxWindow::ReadFileChar("res/shaders/test.slang.spv");
            makeSSBO(window->gpuResource);
            makeDescriptorSetLayout(gpu);
            makePipelineLayout(gpu);
            makePipeline(gpu, window->depthResource->format, *window->windowResource->swapchainResource, shaderCode);
            makeDescriptorPool(gpu, *window->windowResource->swapchainResource);
            makeDescriptorSets(*window, window->getImageCount());
        }

        void markGUITreeStructureChanged() noexcept {
            window->requestPreDraw();
            treeStructureChangedFlag = true;
        }

        void markGUILayoutChanged() noexcept {
            float width, height;
            window->windowResource->getExtent(width, height);
            rootElement->updateLayout(width, height, true);
            window->requestPreDraw();
            layoutChangedFlag = true;
        }

        void markGUIStyleChanged() noexcept {
            window->requestPreDraw();
            styleChangedFlag = true;
        }

        void forgetPointerTarget(VisualElement* gone) noexcept {
            auto inSubtree = [gone](VisualElement* e) {
                for (; e != nullptr; e = e->parentElement())
                    if (e == gone) return true;
                return false;
            };
            if (inSubtree(hovered_)) hovered_ = nullptr;
            if (inSubtree(pressed_)) pressed_ = nullptr;
            for (auto& p : pointerQueue_)
                if (inSubtree(p.target)) p.target = nullptr;      // flush skips null targets
        }

        [[nodiscard]] font::Glyph* GetGlyph(const Style& style) const {
            if (glyphManager == nullptr) return nullptr;
            return glyphManager->getGlyph(style.languageCodeBlock, style.fontFamily, style.fontType);
        }

        std::unique_ptr<VisualElement>                                      rootElement{};
    private:
        UFoxWindow*                                                         window {nullptr};
        MeshManager*                                                        meshManager {nullptr};
        font::GlyphManager*                                                 glyphManager {nullptr};
        TextureManager*                                                     textureManager {nullptr};
        std::unique_ptr<GUIPaletteManager>                                  paletteManager{nullptr};


        std::vector<PendingPointer> pointerQueue_{};
        VisualElement*              hovered_{nullptr};   // element under the pointer
        VisualElement*              pressed_{nullptr};   // got the Down, owns Move/Up until release

        const KeyID*                                                        meshID{nullptr};
        const Mesh*                                                         mesh{nullptr};
        const Texture*                                                      texture{nullptr};

        uint32_t                                                            samplerImageCount = MAX_GUI_TEXTURES;

        std::optional<vk::raii::PipelineCache>                              pipelineCache{};
        std::optional<vk::raii::Pipeline>                                   pipeline{};
        std::optional<vk::raii::DescriptorSetLayout>                        descriptorSetLayout{};
        std::optional<vk::raii::PipelineLayout>                             pipelineLayout{};
        std::optional<vk::raii::DescriptorPool>                             descriptorPool{};
        std::vector<vk::raii::DescriptorSet>                                descriptorSets{};
        std::vector<vk::raii::Sampler>                                      repeatSamplers{};

        gpu::Buffer                                                         elementSSBO{};
        void*                                                               elementMemory{};
        std::vector<ElementData>                                            elementData{};
        bool                                                                treeStructureChangedFlag{false};
        bool                                                                layoutChangedFlag{false};
        bool                                                                styleChangedFlag{false};

        bool onPointer(const PointerEvent& e) {
            if (!rootElement) return false;

            VisualElement* hit    = rootElement->hitTest(e.pos);
            VisualElement* target = (pressed_ && e.phase != PointerPhase::eDown) ? pressed_ : hit;

            ElementPointerEvent ev{};
            ev.pos = e.pos; ev.delta = e.delta; ev.button = e.button; ev.clicks = e.clicks;

            switch (e.phase) {
            case PointerPhase::eMove:
                if (hit != hovered_) {
                    enqueue(hovered_, ElementEventType::PointerLeave, ev, false);
                    enqueue(hit,      ElementEventType::PointerEnter, ev, false);
                    hovered_ = hit;
                }
                enqueueMotion(target, ev);
                break;
            case PointerPhase::eDown:
                pressed_ = hit;
                enqueue(hit, ElementEventType::PointerDown, ev, true);
                break;
            case PointerPhase::eUp:
                enqueue(pressed_, ElementEventType::PointerUp, ev, true);
                if (pressed_ != nullptr && pressed_ == hit)
                    enqueue(hit, ElementEventType::PointerClick, ev, true);
                pressed_ = nullptr;
                break;
            case PointerPhase::eWheel:
                enqueue(hit, ElementEventType::PointerWheel, ev, true);
                break;
            case PointerPhase::eLeave:
                enqueue(hovered_, ElementEventType::PointerLeave, ev, false);
                hovered_ = nullptr;
                break;
            }

            // consumed only if the target or one of its ancestors listens; otherwise main gets it
            for (auto* el = target; el != nullptr; el = el->parentElement())
                if (el->wantsPointer()) return true;
            return false;
        }

void enqueue(VisualElement* target, const ElementEventType type, const ElementPointerEvent& ev, const bool bubbles) {
    if (target == nullptr) return;
    pointerQueue_.push_back({target, type, ev, bubbles});
}

void enqueueMotion(VisualElement* target, const ElementPointerEvent& ev) {
    if (target == nullptr) return;
    if (!pointerQueue_.empty() && pointerQueue_.back().type == ElementEventType::PointerMotion
        && pointerQueue_.back().target == target) {
        pointerQueue_.back().ev = ev;                     // coalesce: keep the newest position
        return;
    }
    enqueue(target, ElementEventType::PointerMotion, ev, false);
}

void flushPointerQueue() {
    for (size_t i = 0; i < pointerQueue_.size(); ++i) {
        auto& p = pointerQueue_[i];
        p.ev.target = p.target;
        for (auto* el = p.target; el != nullptr; ) {
            auto* next = el->parentElement();             // read before a handler can detach el
            el->dispatchPointer(p.type, p.ev);
            if (p.ev.stopped || !p.bubbles) break;
            el = next;
        }
    }
    pointerQueue_.clear();
}

        void makeDescriptorSetLayout(const gpu::GPUResources& gpu) {
            auto descriptorLayout = gpu::MakeDescriptorSetLayout(gpu,{
                gpu::MakeStorageBufferLayoutBinding(1),
                gpu::MakeStorageBufferLayoutBinding(1),
                gpu::MakeUniformBufferLayoutBinding(1),
                gpu::MakeSampledImageLayoutBinding(samplerImageCount),
                gpu::MakeSamplerLayoutBinding(REPEAT_SAMPLER_CONFIGS.size())
            });

            descriptorSetLayout.emplace(std::move(descriptorLayout));
        }

        void makePipelineLayout(const gpu::GPUResources& gpu) {
            vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo
                .setSetLayoutCount(1)
                .setPSetLayouts(&*descriptorSetLayout.value())
                .setPushConstantRangeCount(0);

            pipelineLayout.emplace(*gpu.device, pipelineLayoutInfo);
        }

        void makePipeline(const gpu::GPUResources& gpu, const vk::Format& depthFormat, const gpu::SwapchainResource& swapchain, const std::vector<char>& shaderCode) {
            vk::raii::ShaderModule shaderModule = gpu::CreateShaderModule(gpu ,shaderCode);
            std::array stages = {
                vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, *shaderModule, "vertMain" },
                vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, *shaderModule, "fragMain" }
            };

            std::array bindingDescription{Vertex::getBindingDescription()};
            auto attributeDescriptions = Vertex::getAttributeDescriptions();

            vk::PipelineVertexInputStateCreateInfo vertexInput = gpu::MakePipeVertexInputState(bindingDescription, attributeDescriptions);

            vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly
                .setTopology(vk::PrimitiveTopology::eTriangleList)
                .setPrimitiveRestartEnable(false);

            vk::PipelineViewportStateCreateInfo viewportState{};
            viewportState
                .setViewportCount(1)
                .setScissorCount(1);

            vk::PipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer
                .setPolygonMode(vk::PolygonMode::eFill)
                .setDepthBiasEnable(false)
                .setDepthClampEnable(false)
                .setRasterizerDiscardEnable(false)
                .setLineWidth(1.0f);

            vk::PipelineMultisampleStateCreateInfo multisample{};
            multisample
                .setRasterizationSamples(vk::SampleCountFlagBits::e1);

            vk::PipelineColorBlendAttachmentState blendAttachment{};
            blendAttachment
                .setBlendEnable(true)
                .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha) // Use alpha for color
                .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha) // 1 - alpha for a background
                .setColorBlendOp(vk::BlendOp::eAdd) // Add blended colors
                .setSrcAlphaBlendFactor(vk::BlendFactor::eOne) // Preserve alpha
                .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
                .setAlphaBlendOp(vk::BlendOp::eAdd)
                .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);

            vk::PipelineColorBlendStateCreateInfo blendState{};
            blendState
                .setAttachmentCount(1)
                .setPAttachments(&blendAttachment);

            std::array dynamicStates = {
                vk::DynamicState::eViewport,
                vk::DynamicState::eScissor,
                vk::DynamicState::eCullMode,
                vk::DynamicState::eFrontFace,
                vk::DynamicState::ePrimitiveTopology,
                vk::DynamicState::eStencilWriteMask,
                vk::DynamicState::eStencilCompareMask,
                vk::DynamicState::eStencilReference,
                vk::DynamicState::eStencilOp
            };

            vk::PipelineDynamicStateCreateInfo dynamicState{};
            dynamicState
                .setDynamicStateCount(dynamicStates.size())
                .setPDynamicStates(dynamicStates.data());

            vk::PipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setDepthBoundsTestEnable(false)
                .setDepthCompareOp(vk::CompareOp::eNever)
                .setStencilTestEnable(true);

            vk::PipelineRenderingCreateInfo renderingInfo{};
            renderingInfo
            .setColorAttachmentCount(1)
            .setPColorAttachmentFormats(&swapchain.colorFormat)
            .setDepthAttachmentFormat(depthFormat)
            .setStencilAttachmentFormat(depthFormat);

            vk::GraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo
                .setStageCount(stages.size())
                .setPStages(stages.data())
                .setPVertexInputState(&vertexInput)
                .setPInputAssemblyState(&inputAssembly)
                .setPViewportState(&viewportState)
                .setPRasterizationState(&rasterizer)
                .setPMultisampleState(&multisample)
                .setPColorBlendState(&blendState)
                .setPDynamicState(&dynamicState)
                .setPDepthStencilState(&depthStencil)
                .setLayout(*pipelineLayout.value())
                .setRenderPass(nullptr)
                .setSubpass(0)
                .setPNext(&renderingInfo);

            pipelineCache.emplace(*gpu.device, vk::PipelineCacheCreateInfo());
            pipeline.emplace(*gpu.device, *pipelineCache, pipelineInfo);
        }

        void makeDescriptorPool(const gpu::GPUResources& gpu, const gpu::SwapchainResource& swapchain) {
            uint32_t imageCount = swapchain.getImageCount();
            std::array poolSizes {
                vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, imageCount),
                vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, imageCount),
                vk::DescriptorPoolSize(SCREEN_VIEW_PROJECTION_BUFFER_TYPE, imageCount),
                vk::DescriptorPoolSize(vk::DescriptorType::eSampledImage, imageCount),
                vk::DescriptorPoolSize(vk::DescriptorType::eSampler, imageCount)
                };
            vk::DescriptorPoolCreateInfo poolInfo{};
            poolInfo
                .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
                .setMaxSets(imageCount)
                .setPoolSizes(poolSizes);
            descriptorPool.emplace(*gpu.device, poolInfo);
        }

        void makeDescriptorSets(const UFoxWindow& window, const uint32_t& imageCount) {
            std::vector<vk::DescriptorSetLayout> layouts(imageCount, *descriptorSetLayout);
            vk::DescriptorSetAllocateInfo allocInfo{};
            allocInfo
                .setDescriptorPool( *descriptorPool )
                .setDescriptorSetCount( static_cast<uint32_t>( layouts.size() ) )
                .setPSetLayouts( layouts.data() );

            descriptorSets.clear();

            auto sets = window.gpuResource.device->allocateDescriptorSets(allocInfo);
            const auto imageInfo = textureManager->makeSampledImageDescriptorImageInfos();
            const uint32_t descriptorCount = std::min<uint32_t>(samplerImageCount, static_cast<uint32_t>(imageInfo.size()));

            std::vector<vk::DescriptorImageInfo> samplerInfo;
            samplerInfo.reserve(repeatSamplers.size());

            for (const auto& sampler : repeatSamplers) {
                samplerInfo.emplace_back(MakeDescriptorSamplerOnlyInfo(sampler));
            }

            const vk::DescriptorBufferInfo elementInfo = makeDescriptorBufferInfo();
            const vk::DescriptorBufferInfo styleInfo   = paletteManager->makeDescriptorBufferInfo();
            const vk::DescriptorBufferInfo screenInfo  = window.makeScreenViewProjectionBufferInfo();

            for (size_t i =0; i < imageCount; i++) {
                std::array<vk::WriteDescriptorSet, 5> write{};

                write[0] = gpu::MakeBufferWriteDescriptorSet(sets[i], 0, elementInfo, vk::DescriptorType::eStorageBuffer);
                write[1] = gpu::MakeBufferWriteDescriptorSet(sets[i], 1, styleInfo,   vk::DescriptorType::eStorageBuffer);
                write[2] = gpu::MakeBufferWriteDescriptorSet(sets[i], 2, screenInfo,  SCREEN_VIEW_PROJECTION_BUFFER_TYPE);

                write[3].setDstSet(*sets[i])
                        .setDstBinding(3)
                        .setDstArrayElement(0)
                        .setDescriptorType(vk::DescriptorType::eSampledImage)
                        .setDescriptorCount(descriptorCount)
                        .setPImageInfo(imageInfo.data());
                write[4].setDstSet(*sets[i])
                        .setDstBinding(4)
                        .setDstArrayElement(0)
                        .setDescriptorType(vk::DescriptorType::eSampler)
                        .setDescriptorCount(static_cast<uint32_t>(samplerInfo.size()))
                        .setPImageInfo(samplerInfo.data());
                descriptorSets.push_back(std::move(sets[i]));
                window.gpuResource.device->updateDescriptorSets(write, {});
            }
        }

        void makeSSBO(const gpu::GPUResources& gpu) {
            elementSSBO.clear();
            elementMemory = nullptr;

            const auto limited = gpu.maxStorageBufferRange / sizeof(ElementData);
            const auto max = std::min(MAX_GUI_ELEMENTS, static_cast<uint32_t>(limited));
            const vk::DeviceSize size = sizeof(ElementData) * max;
            gpu::MakeBuffer(elementSSBO, gpu, size, vk::BufferUsageFlagBits::eStorageBuffer,vk::MemoryPropertyFlagBits::eHostVisible |vk::MemoryPropertyFlagBits::eHostCoherent);
            elementMemory = elementSSBO.memory->mapMemory(0, size);
        }

        [[nodiscard]] vk::DescriptorBufferInfo  makeDescriptorBufferInfo() const {
            if (elementSSBO.isDataEmpty()) return {};

            vk::DescriptorBufferInfo info{};
            info
            .setBuffer(*elementSSBO.data)
            .setOffset(0)
            .setRange(sizeof(ElementData) * MAX_GUI_ELEMENTS);

            return info;
        }

        void onResize(const float& width, const float& height) {
            rootElement->updateLayout(width, height, true);
            layoutChangedFlag = true;
        }

        void onGainsFocus() {
            treeStructureChangedFlag = true;
        }

        void onTreeStructureChanged() {
            if (!treeStructureChangedFlag) return;
            updateTextureDescriptorSets();
            paletteManager->clearDynamicMap();
            AssignGpuSlots(*rootElement, static_cast<ElementData*>(elementMemory),0,*textureManager,*paletteManager);
            treeStructureChangedFlag = false;
            debug::log(debug::LogLevel::eInfo, "GUI UIDocument Remake Render Resource : Susses");
        }

        void onLayoutChanged(const uint32_t&) {
            if (!layoutChangedFlag) return;

            SyncSubtree(*rootElement, *textureManager, *paletteManager);
            layoutChangedFlag = false;
        }

        void onStyleChanged() {
            if (!styleChangedFlag) return;

            paletteManager->syncPresetToGpuSlot();

            markGUITreeStructureChanged();

            styleChangedFlag = false;
        }

        void updateTextureDescriptorSets() const {
            if (descriptorSets.empty()) return;

            const std::vector<vk::DescriptorImageInfo> imageInfo = textureManager->makeSampledImageDescriptorImageInfos();
            const uint32_t descriptorCount = std::min<uint32_t>(samplerImageCount, static_cast<uint32_t>(imageInfo.size()));

            for (size_t i = 0; i < window->getImageCount(); i++) {
                vk::WriteDescriptorSet write{};
                write.setDstSet(*descriptorSets[i])
                    .setDstBinding(3)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eSampledImage)
                    .setDescriptorCount(descriptorCount)
                    .setPImageInfo(imageInfo.data());
                window->gpuResource.device->updateDescriptorSets({write}, {});
            }
        }

        void onStart() {
            float w,h;
            window->windowResource->getExtent(w,h);
            rootElement->updateLayout(w,h,true);
        }

        void onPreDraw() {
            onStyleChanged();
            onTreeStructureChanged();
            debug::log(debug::LogLevel::eInfo, "GUI UIDocument PreDraw : Susses");
        }

        void drawCanvas(const vk::raii::CommandBuffer &cmb, const uint32_t& imageIndex, const windowing::WindowResource& winResource, const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& stencilAttachment) const {


            int height, width;
            if (winResource.getExtent(width, height)) return;
            const vk::Rect2D rect{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

            // 3. MANDATORY CORE FIX: Open your rendering channel right here!
            vk::RenderingInfo renderingInfo{};
            renderingInfo.setRenderArea(rect)
                         .setLayerCount(1)
                         .setColorAttachmentCount(1)
                         .setPColorAttachments(&colorAttachment)
                         .setPDepthAttachment(&stencilAttachment)
                         .setPStencilAttachment(&stencilAttachment);

            cmb.beginRendering(renderingInfo);

            // Direct pipeline state attachments
            cmb.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

            cmb.setViewport(0, vk::Viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
            cmb.setScissor(0, rect);
            cmb.setCullMode(vk::CullModeFlagBits::eNone);
            cmb.setFrontFace(vk::FrontFace::eClockwise);
            cmb.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

            vk::Buffer vertexBuffers[] = {*mesh->vertexBuffer->data};
            vk::DeviceSize offsets[] = {0};
            cmb.bindVertexBuffers(0, vertexBuffers, offsets);
            cmb.bindIndexBuffer(*mesh->indexBuffer->data, 0, vk::IndexType::eUint16);
            cmb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptorSets[imageIndex], nullptr);

            // Execute tree layout stencil masking logic seamlessly
            uint32_t globalStencilCounter = 1;
            uint32_t baselineActiveMask = 0;
            RecordElementDrawCommands(cmb, *rootElement, baselineActiveMask, globalStencilCounter);

            cmb.endRendering();
        }
    };

    [[nodiscard]] const font::Glyph* GetGlyph(const UIDocument& doc, const Style& style) noexcept {
        return doc.GetGlyph(style);
    }

    void MarkGUITreeStructureChanged(UIDocument& document) noexcept {
        document.markGUITreeStructureChanged();
    }

    void MarkGUILayoutChanged(UIDocument& document) noexcept {
        document.markGUILayoutChanged();
    }

    void MarkGUIStyleChanged(UIDocument& document) noexcept {
        document.markGUIStyleChanged();
    }

    void ForgetPointerTarget(UIDocument& document, VisualElement* removed) noexcept {
        document.forgetPointerTarget(removed);
    }
}
