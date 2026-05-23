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

    class UIPaletteManager {
    public:
        explicit UIPaletteManager(UFoxWindow* _window, TextureManager* _textureManager = nullptr) : window(_window), textureManager(_textureManager){
            if (window != nullptr) {
                window->registerCallbackEvent<EventType::eSystemInit>([](void* user) { static_cast<UIPaletteManager*>(user)->init();}, this);
            }
        }

        ~UIPaletteManager() {
            destroyBuffer();
            if (window != nullptr) {
                window->unregisterCallbackEvent(EventType::eSystemInit,this);
            }

            if (window != nullptr) {
                window = nullptr;
            }

            if (textureManager != nullptr) {
                textureManager = nullptr;
            }
        }

        void uploadStyleBuffer() const {
            if (_styles.empty() || !_stylePaletteMemory) return;

            const size_t copyCount = std::min(_styles.size(), static_cast<size_t>(_maxStyles));
            std::memcpy(_stylePaletteMemory, _styles.data(), copyCount * sizeof(StyleStaticData));
        }

        void reset() noexcept {
            _styleMap.clear();
            _styles.clear();

            _styles.push_back(DEFAULT_STYLE);
            _styleMap.emplace(DEFAULT_STYLE, 0);
        }


        [[nodiscard]] uint32_t getOrCreateStyle(const StyleStaticData& style) {
            if (auto it = _styleMap.find(style); it != _styleMap.end()) {
                return it->second;
            }

            const auto newIndex = static_cast<uint32_t>(_styles.size());
            _styles.push_back(style);
            _styleMap.emplace(style, newIndex);
            
            return newIndex;
        }

        [[nodiscard]] size_t getStyleCount() const noexcept { return _styles.size(); }

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
        UFoxWindow*                        window = nullptr;
        TextureManager*                    textureManager = nullptr;

        void init() {
            makeStylePaletteSSBO();
        }

        void makeStylePaletteSSBO() {
            if (window == nullptr) return;
            const gpu::GPUResources& gpu = window->gpuResource;
            destroyBuffer();

            const auto limited = gpu.maxStorageBufferRange / sizeof(StyleStaticData);
            _maxStyles = std::min(MAX_GUI_ELEMENTS, static_cast<uint32_t>(limited));
            const vk::DeviceSize size = sizeof(StyleStaticData) * _maxStyles;

            gpu::MakeBuffer(_stylePaletteSSBO, gpu, size,
                            vk::BufferUsageFlagBits::eStorageBuffer,
                            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

            _stylePaletteMemory = _stylePaletteSSBO.memory->mapMemory(0, size);

            _styles.push_back(DEFAULT_STYLE);
            _styleMap.emplace(DEFAULT_STYLE, 0);

            std::memcpy(_stylePaletteMemory, &DEFAULT_STYLE, sizeof(StyleStaticData));
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

        // Buffer allocations matching your custom framework architecture wrapper
        gpu::Buffer                                                                             _stylePaletteSSBO{};
        void*                                                                                   _stylePaletteMemory{nullptr};

        // Cache Maps
        std::unordered_map<StyleStaticData, uint32_t, std::hash<StyleStaticData>, StyleEqual>   _styleMap{};
        std::vector<StyleStaticData>                                                            _styles{};
        uint32_t                                                                                _maxStyles{0};
    };

    class VisualElement: public discadelta::DiscadeltaRectLayoutBase<VisualElement> {
    public:

        UFoxWindow*                                             window = nullptr;
        std::string_view                                        name;
        Style                                                   style{};
        size_t                                                  ssboIndex = 0;
        ElementData*                                            data = nullptr;
        std::vector<VisualElement*>                             leafChildren{};
        std::vector<VisualElement*>                             branchChildren{};
        VisualElement*                                          root = nullptr;
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

        [[nodiscard]] ElementData makeUniformData(const TextureManager& textureManager, UIPaletteManager& paletteManager)  {
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

        void updateElementData(const TextureManager& textureManager) const {
            if (data == nullptr) return;

            const ResourceID imageID{style.backgroundImage};
            auto [texWidth, texHeight] = textureManager.getTextureSize<float>(imageID);


            data->model = getTransformMat4();
            data->sdInput = MakeSDRectInputData(glm::vec2(width, height), style.getMarginSet(), style.getBorderThicknessSet());
            data->imageOffsetAndTiling = ComputeImageUVRect(width, height, texWidth, texHeight, style);
            data->styleIndex = styleDataIndex;
            data->scrollingOffset = {0,0};//not yet feature

            for (const auto* child : leafChildren) {
                child->updateElementData(textureManager);
            }
            for (const auto* child : branchChildren) {
                child->updateElementData(textureManager);
            }
        }
        
        void setAsRenderRoot(UFoxWindow* window) {
            this->window = window;
        }

    private:
        void onAdd(VisualElement* target) override {
            if (target->isContainerEmpty()) {
                addChildToLeaf(target);
            }else {
                addChildToBranch(target);
            }
            switchToBranch();
            
            if (!root) {
                target->root = this;
                if (window) {
                    window->requestPreDraw();
                }
            }else {
                target->root = root;
                if (root->window) {
                    root->window->requestPreDraw();
                }
            }

        }
        void onRemove(VisualElement* target) override {
            std::erase(leafChildren,target);
            std::erase(branchChildren,target);
            switchToLeaf();
            target->root = nullptr;

            if (!root) {
                if (window) {
                    window->requestPreDraw();
                }
            }else if (root->window) {
                root->window->requestPreDraw();
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

    void AccumulateVisualElementUniformData(std::vector<ElementData>& elementDatas,UIPaletteManager& paletteManager,VisualElement& element, const TextureManager& textureManager) {

        // 1. Pack the parent container element first
        element.ssboIndex = elementDatas.size();
        elementDatas.push_back(element.makeUniformData(textureManager, paletteManager));
        element.data = &elementDatas.back(); // Hook pointer safely for zero-allocation runtime frame updates

        // 2. YOUR THEORY: Pack the Leaf Batch contiguously back-to-back in memory
        if (!element.leafChildren.empty()) {
            element.flatLeafStartSsboIndex = static_cast<uint32_t>(elementDatas.size());
            element.flatLeafCount          = static_cast<uint32_t>(element.leafChildren.size());

            for (auto* child : element.leafChildren) {
                child->ssboIndex = elementDatas.size();
                elementDatas.push_back(child->makeUniformData(textureManager, paletteManager));
                child->data = &elementDatas.back();

                // Clear batch properties on leaf elements since simple leaves have no nested children
                child->flatLeafCount          = 0;
                child->flatLeafStartSsboIndex = 0;
            }
        } else {
            element.flatLeafCount          = 0;
            element.flatLeafStartSsboIndex = 0;
        }

        // 3. Recurse down structural sub-nested branch containers individually
        // This allows nested parent panels to safely establish their own contiguous leaf pools!
        for (auto* child : element.branchChildren) {
            AccumulateVisualElementUniformData(elementDatas, paletteManager, *child, textureManager);
        }
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

    inline ResourceID TESTER_TEXTURE_ID{"grok-image-b9472c20-0cfc-4f3d-b558-d5c3066aa081.jpg"};

    class  Document {
    public:
        explicit Document(UFoxWindow& _window, MeshManager* _meshManager = nullptr, TextureManager* _textureManager = nullptr, UIPaletteManager* _paletteManager = nullptr) : window(&_window) , meshManager(_meshManager), textureManager(_textureManager), paletteManager(_paletteManager) {
            window->registerCallbackEvent<EventType::eSystemInit>([](void* user) { static_cast<Document*>(user)->init();}, this);
            window->registerCallbackEvent<EventType::ePostSystemInit>([](const float& w, const float& h, void* user) { static_cast<Document*>(user)->onPostSystemInit(w,h);}, this);
            window->registerCallbackEvent<EventType::eStart>([](void* user) { static_cast<Document*>(user)->onStart();}, this);
            window->registerCallbackEvent<EventType::eResize>([](const float& w, const float& h, void* user) {  static_cast<Document*>(user)->onResize(w,h);}, this);
            window->registerCallbackEvent<EventType::ePreDraw>([](void* user) { static_cast<Document*>(user)->onPreDraw();}, this);
            window->registerCallbackEvent<EventType::eUpdateBuffer>([](const uint32_t& currentImage, void* user){ static_cast<Document*>(user)->updateUniformBuffer(currentImage);}, this);
            window->registerCallbackEvent<EventType::eRenderPass>([](const vk::raii::CommandBuffer& cmb,const uint32_t& imageIndex, const gpu::WindowResource& winResource,
            const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& stencilAttachment, void* user ) { static_cast<Document*>(user)->drawCanvas(cmb, imageIndex, winResource, colorAttachment, stencilAttachment);}, this);
        }

        ~Document() {
            if (window != nullptr) {
                window->unregisterCallbackEvent(EventType::eSystemInit,this);
                window->unregisterCallbackEvent(EventType::ePostSystemInit,this);
                window->unregisterCallbackEvent(EventType::eStart,this);
                window->unregisterCallbackEvent(EventType::eResize,this);
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
            rootElement->setAsRenderRoot(window);
            rootElement->style.backgroundColor = glm::vec4{0.0f, 1.0f, 0.0f, 1.0f};
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

        std::unique_ptr<VisualElement> rootElement{};

    private:
        UFoxWindow*                                                         window = nullptr;
        MeshManager*                                                        meshManager = nullptr;
        TextureManager*                                                     textureManager = nullptr;
        UIPaletteManager*                                                   paletteManager = nullptr;
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
        bool                                                                updateElementData{false};

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

            return std::move(info);
        }

        void onResize(const float& width, const float& height) {
            rootElement->updateLayout(width, height, true);
            updateElementData = true;
        }


        void remakeRenderResource() {
            updateTextureDescriptorSets();
            paletteManager->reset();
            elementData.clear();
            AccumulateVisualElementUniformData(elementData,*paletteManager, *rootElement, *textureManager );
            paletteManager->uploadStyleBuffer();
            debug::log(debug::LogLevel::eInfo, "GUI Document Remake Render Resource : Susses");
        }

        void updateUniformBuffer(const uint32_t& currentImage) {

            if (updateElementData) {
                rootElement->updateElementData(*textureManager);
                updateElementData = false;
                debug::log(debug::LogLevel::eInfo, "GUI Document Update Uniform Buffer : Susses");
            }

            memcpy(elementMemory, elementData.data(), sizeof(ElementData) * elementData.size());
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
            remakeRenderResource();
        }

        void onPreDraw() {
            remakeRenderResource();
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
}
