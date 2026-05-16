module;


#define GLM_FORCE_RADIANS
#include "glm/vec2.hpp"

#include <SDL3/SDL_surface.h>
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
import ufox_discadelta_lib;
import ufox_discadelta_core;
import ufox_render_lib;
import ufox_render_core;


using namespace ufox::geometry;
using namespace ufox::render;
using namespace ufox::engine;

export namespace ufox::gui {

    void PrintTreeDebugWithOffset(const discadelta::RectSegmentContext& ctx, int indent = 0) noexcept {
        std::string pad(indent * 4, ' ');
        std::cout << pad << ctx.config.name
                  << " | w: " << ctx.content.width
                  << " | h: " << ctx.content.height
                  << " | x: " << ctx.content.x
                  << " | y: " << ctx.content.y
                  << "\n";

        for (const auto* child : ctx.children) {
            PrintTreeDebugWithOffset(*child, indent + 1);
        }
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

    class VisualElement: public DiscadeltaRectLayoutBase {
    public:
        ~VisualElement() override {
            debug::log(debug::LogLevel::eInfo, "VisualElement Destructor");
        }


        std::string_view                                        name;
        Style                                                   style{};
        size_t                                                  ssboIndex = 0;
        ShapeUniformData*                                       data = nullptr;
        std::vector<VisualElement*>                             vChildren{};

        VisualElement(const ResourceID &_id,const std::string_view &name): DiscadeltaRectLayoutBase(_id), name(name) {
            baseStyle = &style;
        }

        [[nodiscard]] ShapeUniformData makeUniformData(const TextureManager& textureManager) const {
            const ResourceID imageID{style.backgroundImage};
            auto [texWidth, texHeight] = textureManager.getTextureSize<float>(imageID);

            ShapeUniformData data{};
            data.model = getTransformMat4();
            data.imageIndex = textureManager.getTextureIndex(imageID);
            data.imageColor = style.imageColor;
            data.backgroundColor = style.backgroundColor;
            data.imageOffsetAndTiling = ComputeImageUVRect(width, height, texWidth, texHeight, style);
            data.imageRepeatMode = static_cast<uint32_t>(style.imageRepeatMode);
            data.cornerRadius = style.getCornerRadiusSet();
            data.margin = style.getMarginSet();
            data.borderThickness = style.getBorderThicknessSet();
            data.borderTopColor = style.borderTopColor;
            data.borderLeftColor = style.borderLeftColor;
            data.borderRightColor = style.borderRightColor;
            data.borderBottomColor = style.borderBottomColor;

            return data;
        }

        void updateUniformData(const TextureManager& textureManager) const {
            if (data == nullptr) return;

            const ResourceID imageID{style.backgroundImage};
            auto [texWidth, texHeight] = textureManager.getTextureSize<float>(imageID);

            data->model = getTransformMat4();
            data->imageIndex = textureManager.getTextureIndex(imageID);
            data->imageColor = style.imageColor;
            data->backgroundColor = style.backgroundColor;
            data->imageOffsetAndTiling = ComputeImageUVRect(width, height, texWidth, texHeight, style);
            data->imageRepeatMode = static_cast<uint32_t>(style.imageRepeatMode);
            data->cornerRadius = style.getCornerRadiusSet();
            data->margin = style.getMarginSet();
            data->borderThickness = style.getBorderThicknessSet();
            data->borderTopColor = style.borderTopColor;
            data->borderLeftColor = style.borderLeftColor;
            data->borderRightColor = style.borderRightColor;
            data->borderBottomColor = style.borderBottomColor;
        }

        bool onAdd(DiscadeltaRectLayoutBase *target) override {
            vChildren.emplace_back(dynamic_cast<VisualElement*>(target));
            return true;
        }
        bool onRemove(DiscadeltaRectLayoutBase *target) override {
            auto it = std::find(vChildren.begin(), vChildren.end(), dynamic_cast<VisualElement*>(target));
            return true;
        }


    };

    void AccumulateVisualElementUniformData(std::vector<ShapeUniformData>& uniformDatas,
        VisualElement& element, const TextureManager& textureManager) {

        element.ssboIndex = uniformDatas.size();

        uniformDatas.push_back(element.makeUniformData(textureManager));
        element.data = &uniformDatas.back();

        for (const auto child : *element.getContents() | std::views::values) {

            auto* e = dynamic_cast<VisualElement*>(child);

            AccumulateVisualElementUniformData(uniformDatas, *e, textureManager);
        }
    }

    void RecordElementDrawCommands(const vk::raii::CommandBuffer& cmb, const VisualElement& element, uint32_t activeStencilValue, uint32_t& nextAvailableStencil) {

        // ------------------------------------------------------------
        // STEP 1: Render Current Shape Mesh (Respect Parent Boundaries)
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

        // Draw the visible background layer
        cmb.drawIndexed(std::size(RectIndices), 1, 0, 0, element.ssboIndex);

        // ------------------------------------------------------------
        // STEP 2: Handle Downstream Nesting and Clipping
        // ------------------------------------------------------------
        uint32_t stencilValueForChildren = activeStencilValue;

        if (!element.isHierarchyEmpty()) {
            if (element.style.displayOverflowMode == DisplayOverflowMode::eHidden) {
                // FIX 1: Increment the global stencil index counter for the next unique layer mask
                uint32_t maskWriteValue = nextAvailableStencil++;

                cmb.setStencilWriteMask(vk::StencilFaceFlagBits::eFrontAndBack, 0xFF); // Unlock writing
                cmb.setStencilCompareMask(vk::StencilFaceFlagBits::eFrontAndBack, 0xFF);

                // Set the operations to replace the stencil value with our new mask ID
                cmb.setStencilOp(
                    vk::StencilFaceFlagBits::eFrontAndBack,
                    vk::StencilOp::eKeep,
                    vk::StencilOp::eReplace, // CRITICAL: Forces the GPU to write the mask to memory
                    vk::StencilOp::eKeep,
                    activeStencilValue == 0 ? vk::CompareOp::eAlways : vk::CompareOp::eEqual
                );

                cmb.setStencilReference(vk::StencilFaceFlagBits::eFrontAndBack, maskWriteValue);

                // Re-draw the quad geometry to carve the clipping mask into the stencil buffer
                cmb.drawIndexed(std::size(RectIndices), 1, 0, 0, element.ssboIndex);

                // FIX 2: Force downstream children to test against this new mask layer
                stencilValueForChildren = maskWriteValue;
            }

            // STEP 3: Recursively Draw All Children
            for (const auto &child : element.vChildren) {
                // Pass the updated stencilValueForChildren down the layout tree
                RecordElementDrawCommands(cmb, *child, stencilValueForChildren, nextAvailableStencil);
            }
        }
    }

    inline ResourceID TESTER_TEXTURE_ID{"grok-image-b9472c20-0cfc-4f3d-b558-d5c3066aa081.jpg"};

    class  Document {
    public:
        explicit Document(UFoxWindow* _window = nullptr, MeshManager* _meshManager = nullptr, TextureManager* _textureManager = nullptr) : window(_window) , meshManager(_meshManager), textureManager(_textureManager) {
            if (_window != nullptr) {
                window->registerCallbackEvent<EventType::eSystemInit>([](void* user) { static_cast<Document*>(user)->init();}, this);
                window->registerCallbackEvent<EventType::ePostSystemInit>([](const float& w, const float& h, void* user) { static_cast<Document*>(user)->onPostSystemInit(w,h);}, this);
                window->registerCallbackEvent<EventType::eResize>([](const float& w, const float& h, void* user) {  static_cast<Document*>(user)->updateViewportSize(w,h);}, this);
                window->registerCallbackEvent<EventType::ePostGainsFocus>([](void* user) { static_cast<Document*>(user)->onPostGainsFocus();}, this);
                window->registerCallbackEvent<EventType::eUpdateBuffer>([](const uint32_t& currentImage, void* user){ static_cast<Document*>(user)->updateUniformBuffer(currentImage);}, this);
                window->registerCallbackEvent<EventType::eRenderPass>([](const vk::raii::CommandBuffer& cmb,const uint32_t& imageIndex, const gpu::WindowResource& winResource,
                const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& stencilAttachment, void* user ) { static_cast<Document*>(user)->drawCanvas(cmb, imageIndex, winResource, colorAttachment, stencilAttachment);}, this);
            }
        }

        ~Document() {
            if (window != nullptr) {
                window->unregisterCallbackEvent(EventType::eSystemInit,this);
                window->unregisterCallbackEvent(EventType::ePostSystemInit,this);
                window->unregisterCallbackEvent(EventType::eResize,this);
                window->unregisterCallbackEvent(EventType::ePostGainsFocus,this);
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

            rootElement.reset();
            rootElement.emplace(ResourceID{"root"},"root");
            v1.reset();
            v1.emplace(ResourceID{"v1"},"v1");

            rootElement->x = 100.0f;
            rootElement->y = 100.0f;
            rootElement->width = 400.0f;
            rootElement->height = 600.0f;
            rootElement->style.backgroundColor = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};
            rootElement->add(&*v1);
            rootElement->style.backgroundImage = TESTER_TEXTURE_ID.data;
            rootElement->style.displayOverflowMode = DisplayOverflowMode::eHidden;
            rootElement->style.borderTopRightRadius = 20.0f;
            rootElement->style.borderTopLeftRadius = 20.0f;
            rootElement->style.borderBottomRightRadius = 20.0f;
            rootElement->style.borderBottomLeftRadius = 20.0f;
            rootElement->style.marginTop = 10.0f;
            rootElement->style.marginBottom = 10.0f;
            rootElement->style.marginLeft = 10.0f;
            rootElement->style.marginRight = 10.0f;
            rootElement->style.borderTopWidth = 5.0f;
            rootElement->style.borderTopColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            rootElement->style.borderLeftWidth = 5.0f;
            rootElement->style.borderLeftColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            rootElement->style.borderRightWidth = 5.0f;
            rootElement->style.borderRightColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            rootElement->style.borderBottomWidth = 5.0f;
            rootElement->style.borderBottomColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};


            v1->style.backgroundImage = "shaderIcon.png";
            v1->style.imageColor = glm::vec4{0.0f, 0.0f, 1.0f, 1.0f};
            v1->width = 500.0f;
            v1->height = 500.0f;
            v1->x = 200.0f;
            v1->y = 50.0f;
            v1->style.backgroundColor = glm::vec4{1.0f, 0.2f, 0.5f, 1.0f};
            v1->style.borderTopRightRadius = 20.0f;
            v1->style.borderTopLeftRadius = 20.0f;
            v1->style.borderBottomRightRadius = 20.0f;
            v1->style.borderBottomLeftRadius = 20.0f;
            v1->style.marginTop = 10.0f;
            v1->style.marginBottom = 10.0f;
            v1->style.marginLeft = 10.0f;
            v1->style.marginRight = 10.0f;
            v1->style.borderTopWidth = 5.0f;
            v1->style.borderTopColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            v1->style.borderLeftWidth = 5.0f;
            v1->style.borderLeftColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            v1->style.borderRightWidth = 5.0f;
            v1->style.borderRightColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            v1->style.borderBottomWidth = 5.0f;
            v1->style.borderBottomColor = glm::vec4{1.0f, 1.0f, 1.0f, 1.0f};
            v1->style.imageScaleMode = ImageScaleMode::eScaleToFit;
            v1->style.imageAlignment = ImageAlignment::eCenter;
            v1->style.displayOverflowMode = DisplayOverflowMode::eHidden;

            makeUniformData();

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
            makeSSBO(window->gpuResource, window->getImageCount());
            makeDescriptorSetLayout(gpu);
            makePipelineLayout(gpu);
            makePipeline(gpu, window->depthResource->format, *window->windowResource->swapchainResource, shaderCode);
            makeDescriptorPool(gpu, *window->windowResource->swapchainResource);
            makeDescriptorSets(*window, window->getImageCount());
            updateViewportSize(width, height);

            debug::log(debug::LogLevel::eInfo, "GUI Document InitResource : Susses");
        }

        void onPostGainsFocus() {
            updateTextureDescriptorSets();
        }


        std::optional<VisualElement>                                        rootElement{};
        std::optional<VisualElement>                                        v1 {};


    private:
        UFoxWindow*                                                         window = nullptr;
        MeshManager*                                                        meshManager = nullptr;
        TextureManager*                                                     textureManager = nullptr;
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

        std::vector<gpu::Buffer>                                            shapeUniformBuffer{};
        std::vector<void*>                                                  shapeUniformMemory{};
        std::vector<ShapeUniformData>                                       shapeUniformData{};

        void makeDescriptorSetLayout(const gpu::GPUResources& gpu) {
            auto descriptorLayout = gpu::MakeDescriptorSetLayout(gpu,{
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
            const auto ssboInfo = makeDescriptorBufferInfo();
            const auto imageInfo = textureManager->makeSampledImageDescriptorImageInfos();
            const uint32_t descriptorCount = std::min<uint32_t>(samplerImageCount, static_cast<uint32_t>(imageInfo.size()));

            std::vector<vk::DescriptorImageInfo> samplerInfo;
            samplerInfo.reserve(repeatSamplers.size());

            for (const auto& sampler : repeatSamplers) {
                samplerInfo.emplace_back(MakeDescriptorSamplerOnlyInfo(sampler));
            }

            for (size_t i =0; i < imageCount; i++) {
                std::array<vk::WriteDescriptorSet, 4> write{};
                write[0].setDstSet(*sets[i])
                        .setDstBinding(0)
                        .setDstArrayElement(0)
                        .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                        .setDescriptorCount(1)
                        .setPBufferInfo(&ssboInfo[i]);
                write[1] = gpu::MakeBufferWriteDescriptorSet(sets[i], window.makeScreenViewProjectionBufferInfo(), 1, SCREEN_VIEW_PROJECTION_BUFFER_TYPE);
                write[2].setDstSet(*sets[i])
                        .setDstBinding(2)
                        .setDstArrayElement(0)
                        .setDescriptorType(vk::DescriptorType::eSampledImage)
                        .setDescriptorCount(descriptorCount)
                        .setPImageInfo(imageInfo.data());
                write[3].setDstSet(*sets[i])
                        .setDstBinding(3)
                        .setDstArrayElement(0)
                        .setDescriptorType(vk::DescriptorType::eSampler)
                        .setDescriptorCount(REPEAT_SAMPLER_CONFIGS.size())
                        .setPImageInfo(samplerInfo.data());
                descriptorSets.push_back(std::move(sets[i]));
                window.gpuResource.device->updateDescriptorSets(write, {});
            }
        }

        void makeUniformData() {
            std::map<const ResourceID, const uint32_t> textureMap{};
            shapeUniformData.clear();
            AccumulateVisualElementUniformData(shapeUniformData, *rootElement, *textureManager);
        }

        void makeSSBO(const gpu::GPUResources& gpu, const uint32_t imageCount) {
            shapeUniformBuffer.clear();
            shapeUniformMemory.clear();
            shapeUniformBuffer.reserve(imageCount);
            shapeUniformMemory.reserve(imageCount);
            const auto limited = gpu.maxStorageBufferRange / sizeof(ShapeUniformData);
            const auto max = std::min(MAX_GUI_ELEMENTS, static_cast<uint32_t>(limited));
            const vk::DeviceSize size = sizeof(ShapeUniformData) * max;
            for (size_t i = 0; i < imageCount; i++) {
                gpu::Buffer buffer;
                gpu::MakeBuffer(buffer, gpu, size, vk::BufferUsageFlagBits::eStorageBuffer,vk::MemoryPropertyFlagBits::eHostVisible |vk::MemoryPropertyFlagBits::eHostCoherent);
                shapeUniformBuffer.push_back(std::move(buffer));
                shapeUniformMemory.push_back(shapeUniformBuffer.back().memory->mapMemory(0, size));
            }
        }

        [[nodiscard]] std::vector<vk::DescriptorBufferInfo>  makeDescriptorBufferInfo() const {
            if (shapeUniformBuffer.empty()) return {};

            std::vector<vk::DescriptorBufferInfo> bufferInfo{};
            bufferInfo.reserve(shapeUniformBuffer.size());

            for (const auto &[memory, data] : shapeUniformBuffer) {
                vk::DescriptorBufferInfo info{};
                info
                .setBuffer(*data)
                .setOffset(0)
                .setRange(sizeof(ShapeUniformData) * shapeUniformData.size() );
                bufferInfo.push_back(info);
            }

            return std::move(bufferInfo);
        }

        void updateViewportSize(const float& width, const float& height) {
            rootElement->updateUniformData(*textureManager);
        }

        void updateUniformBuffer(const uint32_t& currentImage) const {
            memcpy(shapeUniformMemory[currentImage], shapeUniformData.data(), sizeof(ShapeUniformData) * shapeUniformData.size());
        }

        void updateTextureDescriptorSets() {
            if (descriptorSets.empty()) return;

            const std::vector<vk::DescriptorImageInfo> imageInfo = textureManager->makeSampledImageDescriptorImageInfos();
            const uint32_t descriptorCount = std::min<uint32_t>(samplerImageCount, static_cast<uint32_t>(imageInfo.size()));

            for (size_t i = 0; i < window->getImageCount(); i++) {
                vk::WriteDescriptorSet write{};
                write.setDstSet(*descriptorSets[i])
                    .setDstBinding(2)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eSampledImage)
                    .setDescriptorCount(descriptorCount)
                    .setPImageInfo(imageInfo.data());
                window->gpuResource.device->updateDescriptorSets({write}, {});
            }

            makeUniformData();
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
            cmb.setCullMode(vk::CullModeFlagBits::eBack);
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
