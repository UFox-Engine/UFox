module;


#define GLM_FORCE_RADIANS
#include "glm/vec2.hpp"

#include <SDL3/SDL_surface.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <map>
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

    class Viewpanel;
    class VisualElement;
    constexpr void DestroyVisualElement(VisualElement* element) noexcept;
    using VisualElementHandler = std::unique_ptr<VisualElement, decltype(&DestroyVisualElement)>;

    class VisualElement: public RectLayout {
        public:
        std::string_view                                        name;
        Viewpanel*                                              panel{nullptr};
        VisualElement*                                          parent{nullptr};

        std::vector<VisualElement*>                             hierarchy{};
        Style                                                   style{};
        float z{0.0f};


        explicit VisualElement(const std::string_view& _name = "VisualElement") : name {_name} {}


        [[nodiscard]] glm::mat4 getTransformMat4() const {
            return MakeTransformModel(glm::vec3{x, y, z}, IDENTITY_QUAT, glm::vec3{width, height, 1.0f});
        }

        [[nodiscard]] UniformData makeUniformData(const TextureManager& textureManager) const {
            const ResourceID imageID{style.backgroundImage};
            auto [texWidth, texHeight] = textureManager.getTextureSize<float>(imageID);

            UniformData data{};
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

        void updateUniformData(UniformData& data, const TextureManager& textureManager) const {
            const ResourceID imageID{style.backgroundImage};
            auto [texWidth, texHeight] = textureManager.getTextureSize<float>(imageID);

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
        }

        void link(VisualElement& element) {
            if (&element == this) return;
            if (element.parent != nullptr) {
                element.parent->remove(element);
            }
            element.parent = this;

            element.panel = panel;
            hierarchy.push_back(&element);
        }

        void add(VisualElementHandler& element) {
            children.push_back(std::move(element));
            const auto & child = children.back();
            link(*child.get());
        }

        void remove(VisualElement& element) {
            if (&element == this) return;
            const auto it = std::ranges::find(hierarchy, &element);
            if (it == hierarchy.end()) return;
            hierarchy.erase(it);

            element.parent = nullptr;
            element.panel = nullptr;

            auto cIt = std::ranges::find_if(children, [&](const auto& child) { return child.get() == &element; });
            if (cIt != children.end()) {
                children.erase(cIt);
            }
        }

        VisualElement* getChild(const std::string_view& name) {
            auto it = std::ranges::find_if(hierarchy, [&](const auto& child) { return child->name == name; });
            if (it == hierarchy.end()) return nullptr;
            return *it;
        }

    private:
        std::vector<VisualElementHandler> children{};

    };

    constexpr void DestroyVisualElement(VisualElement* element) noexcept {
        if (element == nullptr) return;
        if (element->parent != nullptr) {
            element->parent->remove(*element);
        }

        element->hierarchy.clear();
        element->panel = nullptr;
        element->parent = nullptr;
    }

    constexpr auto CreateVisualElement(const std::string_view& name) -> VisualElementHandler {
        return VisualElementHandler{new VisualElement(name), DestroyVisualElement};
    }

    void AccumulateVisualElementUniformData(std::vector<UniformData>& uniformDatas, std::vector<std::pair<const VisualElement*,UniformData*>>& bindings, const VisualElement& element, const TextureManager& textureManager) {

        uniformDatas.push_back(element.makeUniformData(textureManager));

        const std::pair<const VisualElement*, UniformData*> bind = std::make_pair(&element, &uniformDatas.back());
        bindings.push_back(bind);

        // Recurse into THIS element's children — not root!
        for (const VisualElement* child : element.hierarchy) {
            if (child) {
                AccumulateVisualElementUniformData(uniformDatas, bindings, *child, textureManager);
            }
        }
    }

    void UpdateVisualElementUniformData(std::vector<std::pair<const VisualElement*,UniformData*>>& bindings, const TextureManager& textureManager) {
        if (bindings.empty()) return;
        for (auto& [element, data] : bindings) {
            element->updateUniformData(*data, textureManager);
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
                    const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& depthAttachment, void* user ) { static_cast<Document*>(user)->drawCanvas(cmb, imageIndex, winResource, colorAttachment, depthAttachment);}, this);
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
            rootElement = CreateVisualElement("root-panel-element");

            v1 = CreateVisualElement("v1");
            v2 = CreateVisualElement("v2");
            v3 = CreateVisualElement("v3");

            v2->style.backgroundColor = glm::vec4{0.0f, 1.0f, 0.0f, 1.0f};
            v2->style.imageColor = glm::vec4{0.0f, 0.0f, 1.0f, 1.0f};
            v2->style.backgroundImage = "shaderIcon.png";
            //v2->style.imageScaleMode = ImageScaleMode::eScaleToFit;
            //v2->style.imageRepeatMode = ImageRepeatMode::eNone;
            //v2->style.imageAlignment = ImageAlignment::eCenter;
            v3->style.backgroundImage = "NotoSans-Regular_latin_mtsdf";

            rootElement->width = 400.0f;
            rootElement->height = 800.0f;
            rootElement->style.backgroundColor = glm::vec4{0.0f, 0.0f, 0.0f, 1.0f};

            rootElement->link(*v1.get());
            //rootElement->style.backgroundImage = TESTER_TEXTURE_ID.data;


            v1->style.backgroundImage = "shaderIcon.png";
            v1->style.imageColor = glm::vec4{0.0f, 0.0f, 1.0f, 1.0f};
            v1->width = 400.0f;
            v1->height = 400.0f;
            v1->style.backgroundColor = glm::vec4{1.0f, 1.0f, 1.0f, 0.0f};
            v1->style.borderTopRightRadius = 40.0f;
            v1->style.borderTopLeftRadius = 40.0f;
            v1->style.borderBottomRightRadius = 40.0f;
            v1->style.borderBottomLeftRadius = 40.0f;
            v1->style.marginTop = 10.0f;
            v1->style.marginBottom = 10.0f;
            v1->style.marginLeft = 10.0f;
            v1->style.marginRight = 10.0f;
            v1->style.borderTopWidth = 10.0f;
            v1->style.borderTopColor = glm::vec4{1.0f, 1.0f, 1.0f, 0.20f};
            v1->style.borderLeftWidth = 5.0f;
            v1->style.borderLeftColor = glm::vec4{1.0f, 1.0f, 1.0f, 0.20f};
            v1->style.borderRightWidth = 30.0f;
            v1->style.borderRightColor = glm::vec4{1.0f, 1.0f, 1.0f, 0.20f};
            v1->style.borderBottomWidth = 50.0f;
            v1->style.borderBottomColor = glm::vec4{1.0f, 1.0f, 1.0f, 0.20f};
            v1->style.imageScaleMode = ImageScaleMode::eScaleToFit;
            v1->style.imageAlignment = ImageAlignment::eCenter;

            v1->link(*v2.get());


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


        VisualElementHandler                                        rootElement{nullptr, DestroyVisualElement};
        VisualElementHandler                                        v1 {nullptr, DestroyVisualElement};
        VisualElementHandler                                        v2 {nullptr, DestroyVisualElement};
        VisualElementHandler                                        v3 {nullptr, DestroyVisualElement};

    private:
        UFoxWindow*                                                 window = nullptr;
        MeshManager*                                                meshManager = nullptr;
        TextureManager*                                             textureManager = nullptr;
        const ResourceID*                                           meshID{nullptr};
        const Mesh*                                                 mesh{nullptr};
        const Texture*                                              texture{nullptr};
        uint32_t                                                    samplerImageCount = MAX_GUI_TEXTURES;

        std::optional<vk::raii::PipelineCache>                      pipelineCache{};
        std::optional<vk::raii::Pipeline>                           pipeline{};
        std::optional<vk::raii::DescriptorSetLayout>                descriptorSetLayout{};
        std::optional<vk::raii::PipelineLayout>                     pipelineLayout{};
        std::optional<vk::raii::DescriptorPool>                     descriptorPool{};
        std::vector<vk::raii::DescriptorSet>                        descriptorSets{};
        std::vector<vk::raii::Sampler>                              repeatSamplers{};

        std::vector<gpu::Buffer>                                    uniformBuffer{};
        std::vector<void*>                                          uniformMemory{};
        std::vector<UniformData>                                    uniformData{};
        std::vector<std::pair<const VisualElement*, UniformData*>>  bindings{};

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

            std::array dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eCullMode,
                                         vk::DynamicState::eFrontFace, vk::DynamicState::ePrimitiveTopology };
            vk::PipelineDynamicStateCreateInfo dynamicState{};
            dynamicState
                .setDynamicStateCount(dynamicStates.size())
                .setPDynamicStates(dynamicStates.data());

            vk::PipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil
                .setDepthTestEnable(false)
                .setDepthWriteEnable(false)
                .setDepthCompareOp(vk::CompareOp::eLessOrEqual)
                .setDepthBoundsTestEnable(false)
                .setStencilTestEnable(false);

            vk::PipelineRenderingCreateInfo renderingInfo{};
            renderingInfo
            .setColorAttachmentCount(1)
            .setPColorAttachmentFormats(&swapchain.colorFormat)
            .setDepthAttachmentFormat(depthFormat);

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
            uniformData.clear();
            bindings.clear();
            AccumulateVisualElementUniformData(uniformData, bindings, *rootElement.get(), *textureManager);
            debug::log(debug::LogLevel::eInfo, "Viewpanel::makeUniformData: data {}", bindings.size() );
        }

        void makeSSBO(const gpu::GPUResources& gpu, const uint32_t imageCount) {
            uniformBuffer.clear();
            uniformMemory.clear();
            uniformBuffer.reserve(imageCount);
            uniformMemory.reserve(imageCount);
            const auto limited = gpu.maxStorageBufferRange / sizeof(UniformData);
            const auto max = std::min(1048576u, static_cast<uint32_t>(limited));
            const vk::DeviceSize size = sizeof(UniformData) * max;
            for (size_t i = 0; i < imageCount; i++) {
                gpu::Buffer buffer;
                gpu::MakeBuffer(buffer, gpu, size, vk::BufferUsageFlagBits::eStorageBuffer,vk::MemoryPropertyFlagBits::eHostVisible |vk::MemoryPropertyFlagBits::eHostCoherent);
                uniformBuffer.push_back(std::move(buffer));
                uniformMemory.push_back(uniformBuffer.back().memory->mapMemory(0, size));
            }
        }

        [[nodiscard]] std::vector<vk::DescriptorBufferInfo>  makeDescriptorBufferInfo() const {
            if (uniformBuffer.empty()) return {};

            std::vector<vk::DescriptorBufferInfo> bufferInfo{};
            bufferInfo.reserve(uniformBuffer.size());

            for (const auto &[memory, data] : uniformBuffer) {
                vk::DescriptorBufferInfo info{};
                info
                .setBuffer(*data)
                .setOffset(0)
                .setRange(sizeof(UniformData) * uniformData.size() );
                bufferInfo.push_back(info);
            }

            return std::move(bufferInfo);
        }

        void updateViewportSize(const float& width, const float& height) {
            rootElement->width = width;
            rootElement->height = height;
            v1->width = width;
            v1->height = height;
            UpdateVisualElementUniformData(bindings, *textureManager);
        }

        void updateUniformBuffer(const uint32_t& currentImage) const {
            memcpy(uniformMemory[currentImage], uniformData.data(), sizeof(UniformData) * uniformData.size());
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

        void drawCanvas(const vk::raii::CommandBuffer &cmb, const uint32_t& imageIndex, const gpu::WindowResource& winResource,
            const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& depthAttachment) const {

            int height, width;

            if (winResource.getExtent(width, height)) return;

            const vk::Rect2D rect{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};

            vk::RenderingInfo renderingInfo{};
            renderingInfo.setRenderArea(rect)
                         .setLayerCount(1)
                         .setColorAttachmentCount(1)
                         .setPColorAttachments(&colorAttachment);
                         //.setPDepthAttachment(&depthAttachment);
            cmb.beginRendering(renderingInfo);

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
            cmb.drawIndexed(std::size(RectIndices), uniformData.size(), 0, 0, 0);

            cmb.endRendering();
        }
    };
}
