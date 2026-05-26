module;


#define GLM_FORCE_RADIANS
#include "glm/vec2.hpp"
#include "nanoid/nanoid.h"

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


using namespace ufox::geometry;
using namespace ufox::render;
using namespace ufox::engine;

export namespace ufox::gui {
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

    [[nodiscard]] glm::vec4 ComputeImageUVRect(const float elementWidth, const float elementHeight,const float textureWidth, const float textureHeight, const Style& style) noexcept{
        if (textureWidth <= 0.0f || textureHeight <= 0.0f || elementWidth <= 0.0f || elementHeight <= 0.0f)
            return {0.0f, 0.0f, 1.0f, 1.0f};

        const glm::vec2 imageSpace = style.getImageSpace();
        const ImageScaleMode& mode = style.imageScaleMode;
        const ImageAlignment& alignment = style.imageAlignment;

        const glm::vec2 rectSize{elementWidth, elementHeight};
        const glm::vec2 imageSize = rectSize - imageSpace;
        const glm::vec2 imageRect = imageSize / rectSize;
        const glm::vec2 texTiling = rectSize / imageSize;
        const glm::vec2 borderTLOffset = style.getImageTLSpaceOffset() / rectSize ;


        const float elemAspect = elementWidth / elementHeight;
        const float texAspect  = textureWidth / textureHeight ;
        const float imageAspect = imageSize.x / imageSize.y ;


        switch (mode) {
        case ImageScaleMode::eStretchToFill:
            return {-borderTLOffset, texTiling};

        case ImageScaleMode::eScaleToFit: {
            if (imageAspect > texAspect) {
                const float uvWidth =  imageAspect / texAspect;
                const glm::vec2 tiling = glm::vec2{uvWidth, 1.0f} * texTiling;
                const glm::vec2 offset = ComputeImageUVPositionX(tiling, borderTLOffset, imageRect, alignment);
                return {offset, tiling } ;
            }
            const float uvHeight = texAspect / imageAspect;
            const glm::vec2 tiling = glm::vec2{1.0f, uvHeight} * texTiling;
            const glm::vec2 offset = ComputeImageUVPositionY(tiling, borderTLOffset, imageRect, alignment) ;
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
        return {-borderTLOffset, texTiling};
    }

    class Document;
    void MarkGUITreeStructureChanged(Document& document) noexcept;
    void MarkGUILayoutChanged(Document& document) noexcept;
    void MarkGUIStyleChanged(Document& document) noexcept;


    StyleStaticData MakeStyleStaticData(const Style& style) noexcept {
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

    class GUIPaletteManager {
    public:
        explicit GUIPaletteManager(UFoxWindow* window, Document* document) : _window(window), _document(document){
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
            _stylePreset.emplace(ResourceID{name},data);

            MarkGUIStyleChanged(*_document);
        }

        void addPreset(const std::string_view name,const Style& style) {
            const auto data = MakeStyleStaticData(style);
            _stylePreset.emplace(ResourceID{name},data);

            MarkGUIStyleChanged(*_document);
        }

        void removePreset(const std::string_view name) {
            if (const auto it = _stylePreset.find(ResourceID{name}); it != _stylePreset.end()) {
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
        Document*                                                                               _document{nullptr};
        gpu::Buffer                                                                             _stylePaletteSSBO{};
        void*                                                                                   _stylePaletteMemory{nullptr};

        // Cache Maps
        std::unordered_map<StyleStaticData, uint32_t, std::hash<StyleStaticData>, StyleEqual>   _staticMap{};
        std::unordered_map<StyleStaticData, uint32_t, std::hash<StyleStaticData>, StyleEqual>   _dynamicMap{};
        std::unordered_map<ResourceID,StyleStaticData, ResourceIDHash, ResourceIDEq>            _stylePreset{};
        uint32_t                                                                                _validatedPresetCount{0};
        uint32_t                                                                                _maxStyles{0};
    };



    class VisualElement: public discadelta::DiscadeltaRectLayoutBase<VisualElement> {
    public:
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

        VisualElement(const ResourceID& _id, const std::string_view _name)
            : DiscadeltaRectLayoutBase(_id)
            , name(_name)
        {
            baseStyle = &style;
        }

        explicit VisualElement(const std::string_view _name): DiscadeltaRectLayoutBase(ResourceID{_name}), name(_name) {}

        VisualElement() = default;

        [[nodiscard]] ElementData makeUniformData(const TextureManager& textureManager, GUIPaletteManager& paletteManager)  {
            const ResourceID imageID{style.backgroundImage};
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
            data.imageOffsetAndTiling = ComputeImageUVRect(width, height, texWidth, texHeight, style);
            data.styleIndex = styleDataIndex;
            data.imageID = textureManager.getTextureIndex(imageID);
            data.scrollingOffset = {0,0};//not yet feature

            return data;
        }

        // Called once per rebuild: assigns slot, writes initial data, registers pointer
        void bindGpuSlot(ElementData* slot, const TextureManager& tex, GUIPaletteManager& palette) {
            gpuSlot = slot;
            syncToGpu(tex, palette);
        }

        // Called on layout change — writes directly to mapped memory, no copy
        void syncToGpu(const TextureManager& tex, GUIPaletteManager& palette) {
            if (!gpuSlot) return;
            const ResourceID imageID{style.backgroundImage};
            auto [tw, th] = tex.getTextureSize<float>(imageID);

            const StyleStaticData styleData = MakeStyleStaticData(style);
            styleDataIndex = palette.getOrCreateStyle(styleData);

            elementData.model              = getTransformMat4();
            elementData.sdInput            = MakeSDRectInputData(glm::vec2(width, height),
                                                 style.getMarginSet(),
                                                 style.getBorderThicknessSet());
            elementData.imageOffsetAndTiling = ComputeImageUVRect(width, height, tw, th, style);
            elementData.styleIndex         = styleDataIndex;
            elementData.imageID            = tex.getTextureIndex(imageID);
            elementData.scrollingOffset    = {0.0f, 0.0f};

            *gpuSlot = elementData; // single struct copy directly into mapped memory
        }

        void setAsRenderRoot(Document* document) {
            this->document = document;
        }

    private:
        Document*                           document{nullptr};
        VisualElement*                      root = nullptr;

        friend VisualElement;

        void onAdd(VisualElement* target) override {
            if (target->isContainerEmpty()) {
                addChildToLeaf(target);
            }else {
                addChildToBranch(target);
            }
            switchToBranch();

            if (!root) {
                target->root = this;
            }else {
                target->root = root;
            }
        }
        void onRemove(VisualElement* target) override {
            std::erase(leafChildren,target);
            std::erase(branchChildren,target);
            switchToLeaf();
            target->root = nullptr;
        }

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
            if (!parent || !isContainerEmpty()) return;
            auto* p = dynamic_cast<VisualElement*>(parent);
            if (const auto it = std::ranges::find(p->leafChildren, this);
                it != p->leafChildren.end()) return;
            p->leafChildren.push_back(this);
            std::erase(p->branchChildren, this);
        }
    };

    uint32_t AssignGpuSlots(VisualElement& element,ElementData* mappedBase, uint32_t slotIndex,const TextureManager& tex,GUIPaletteManager& palette){
        // Assign this element
        element.ssboIndex = slotIndex;
        element.bindGpuSlot(mappedBase + slotIndex, tex, palette);
        slotIndex++;

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

    inline ResourceID TESTER_TEXTURE_ID{"grok-image-b9472c20-0cfc-4f3d-b558-d5c3066aa081.jpg"};

    class  Document {
    public:
        explicit Document(UFoxWindow& _window, MeshManager* _meshManager = nullptr, TextureManager* _textureManager = nullptr) : window(&_window) , meshManager(_meshManager), textureManager(_textureManager) {
            window->registerCallbackEvent<EventType::eSystemInit>([](void* user) { static_cast<Document*>(user)->init();}, this);
            window->registerCallbackEvent<EventType::ePostSystemInit>([](const float& w, const float& h, void* user) { static_cast<Document*>(user)->onPostSystemInit(w,h);}, this);
            window->registerCallbackEvent<EventType::eStart>([](void* user) { static_cast<Document*>(user)->onStart();}, this);
            window->registerCallbackEvent<EventType::eResize>([](const float& w, const float& h, void* user) {  static_cast<Document*>(user)->onResize(w,h);}, this);
            window->registerCallbackEvent<EventType::eGainsFocus>([](void* user) { static_cast<Document*>(user)->onGainsFocus();}, this);
            window->registerCallbackEvent<EventType::ePreDraw>([](void* user) { static_cast<Document*>(user)->onPreDraw();}, this);
            window->registerCallbackEvent<EventType::eUpdateBuffer>([](const uint32_t& currentImage, void* user){ static_cast<Document*>(user)->onLayoutChanged(currentImage);}, this);
            window->registerCallbackEvent<EventType::eRenderPass>([](const vk::raii::CommandBuffer& cmb,const uint32_t& imageIndex, const gpu::WindowResource& winResource,
            const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& stencilAttachment, void* user ) { static_cast<Document*>(user)->drawCanvas(cmb, imageIndex, winResource, colorAttachment, stencilAttachment);}, this);
            paletteManager = std::make_unique<GUIPaletteManager>(window, this);
        }

        ~Document() {
            if (window != nullptr) {
                window->unregisterCallbackEvent(EventType::eSystemInit,this);
                window->unregisterCallbackEvent(EventType::ePostSystemInit,this);
                window->unregisterCallbackEvent(EventType::eStart,this);
                window->unregisterCallbackEvent(EventType::eResize,this);
                window->unregisterCallbackEvent(EventType::eGainsFocus,this);
                window->unregisterCallbackEvent(EventType::ePreDraw,this);
                window->unregisterCallbackEvent(EventType::eUpdateBuffer,this);
                window->unregisterCallbackEvent(EventType::eRenderPass,this);
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

            rootElement = std::make_unique<VisualElement>(ResourceID("root"),"Root");
            rootElement->setAsRenderRoot(this);
            rootElement->style.backgroundColor = glm::vec4{0.0f, 0.0f, 0.0f, 0.0f};
            rootElement->style.direction = discadelta::FlexDirection::eRow;
            rootElement->style.displayOverflowMode = DisplayOverflowMode::eHidden;
            rootElement->style.flexCompress =1.0f;
            rootElement->style.flexExpand =1.0f;

            debug::log(debug::LogLevel::eInfo, "GUI Document Init : Susses");
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

        std::unique_ptr<VisualElement> rootElement{};

    private:
        UFoxWindow*                                                         window = nullptr;
        MeshManager*                                                        meshManager = nullptr;
        TextureManager*                                                     textureManager = nullptr;
        std::unique_ptr<GUIPaletteManager>                                  paletteManager{nullptr};
        const ResourceID*                                                   meshID{nullptr};
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

            for (size_t i =0; i < imageCount; i++) {
                std::array<vk::WriteDescriptorSet, 5> write{};
                write[0] = gpu::MakeBufferWriteDescriptorSet(sets[i],0, makeDescriptorBufferInfo(), vk::DescriptorType::eStorageBuffer);
                write[1] = gpu::MakeBufferWriteDescriptorSet(sets[i],1, paletteManager->makeDescriptorBufferInfo(), vk::DescriptorType::eStorageBuffer);
                write[2] = gpu::MakeBufferWriteDescriptorSet(sets[i],2, window.makeScreenViewProjectionBufferInfo(), SCREEN_VIEW_PROJECTION_BUFFER_TYPE);
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
                        .setDescriptorCount(REPEAT_SAMPLER_CONFIGS.size())
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
            debug::log(debug::LogLevel::eInfo, "GUI Document Remake Render Resource : Susses");
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
        }

        void drawCanvas(const vk::raii::CommandBuffer &cmb, const uint32_t& imageIndex, const gpu::WindowResource& winResource, const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& stencilAttachment
                ) const {


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

    void MarkGUITreeStructureChanged(Document& document) noexcept {
        document.markGUITreeStructureChanged();
    }

    void MarkGUILayoutChanged(Document& document) noexcept {
        document.markGUILayoutChanged();
    }

    void MarkGUIStyleChanged(Document& document) noexcept {
        document.markGUIStyleChanged();
    }
}
