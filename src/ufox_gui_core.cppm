module;


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>
#include <iostream>
#include <SDL3/SDL_surface.h>
#include <map>


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

    class Viewpanel;
    class VisualElement;
    constexpr void DestroyVisualElement(VisualElement* element) noexcept;
    using VisualElementHandler = std::unique_ptr<VisualElement, decltype(&DestroyVisualElement)>;

    class  VisualElement {
        public:
        Viewpanel*                                              panel{nullptr};
        VisualElement*                                          parent{nullptr};
        discadelta::RectSegmentContextHandler                   rect{nullptr, discadelta::DestroySegmentContext<discadelta::RectSegmentContext>};
        std::vector<VisualElement*>                             hierarchy{};
        Style                                                   style{};

        explicit VisualElement(const std::string_view& name = "VisualElement") {
            rect = discadelta::CreateSegmentContext<discadelta::RectSegmentContext, discadelta::RectSegmentCreateInfo>({
            .name          = name.data(),
            .width         = 0.0f,
            .widthMin      = 0.0f,
            .widthMax      = std::numeric_limits<float>::max(),
            .height        = std::numeric_limits<float>::max(),
            .heightMin     = 0.0f,
            .heightMax     = std::numeric_limits<float>::max(),
            .direction     = discadelta::FlexDirection::Row,
            .flexCompress  = 1.0f,
            .flexExpand    = 1.0f,
            .order         = 0
            });
        }

        void link(VisualElement& element) {
            if (&element == this) return;
            if (element.parent != nullptr) {
                element.parent->remove(element);
            }
            element.parent = this;

            discadelta::Link(*rect.get(), *element.rect.get());
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
            discadelta::Unlink(*element.rect.get());
            element.parent = nullptr;
            element.panel = nullptr;

            auto cIt = std::ranges::find_if(children, [&](const auto& child) { return child.get() == &element; });
            if (cIt != children.end()) {
                children.erase(cIt);
            }
        }

        VisualElement* getChild(const std::string_view& name) {
            auto it = std::ranges::find_if(hierarchy, [&](const auto& child) { return child->rect->config.name == name; });
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

    [[nodiscard]] glm::vec4 ComputeImageUVRect(const float elementWidth, const float elementHeight,const float textureWidth, const float textureHeight,const ImageScaleMode mode) noexcept{
        if (textureWidth <= 0.0f || textureHeight <= 0.0f || elementWidth <= 0.0f || elementHeight <= 0.0f)
            return {0.0f, 0.0f, 1.0f, 1.0f};

        const float elemAspect = elementWidth / elementHeight;
        const float texAspect  = textureWidth / textureHeight;

        switch (mode) {
            case ImageScaleMode::eStretchToFill:
                return {0.0f, 0.0f, 1.0f, 1.0f};

            case ImageScaleMode::eScaleToFit: {
                if (elemAspect > texAspect) {
                    const float uvWidth = elemAspect / texAspect;
                    return {(1.0f - uvWidth) * 0.5f, 0.0f, uvWidth, 1.0f};
                }
                const float uvHeight = texAspect / elemAspect;
                return {0.0f, (1.0f - uvHeight) * 0.5f, 1.0f, uvHeight};
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
        return {0.0f, 0.0f, 1.0f, 1.0f};
    }

    void AccumulateVisualElementUniformData(std::vector<UniformData>& uniformDatas, std::vector<std::pair<const VisualElement*,UniformData*>>& bindings, const VisualElement& element, const TextureManager& textureManager) {
        if (!element.rect || !element.rect.get()) {
            return;  // safety - invalid rect context
        }

        const auto& seg = element.rect.get()->content;

        // Build model matrix – position + non-uniform scale
        // Note: we're using scale for width/height — very common for 2D UI
        const glm::mat4 model = MakeTransformModel(
            glm::vec3{seg.x, seg.y, 0.0f},
            glm::quat{1.0f, 0.0f, 0.0f, 0.0f},     // identity quaternion (no rotation for now)
            glm::vec3{seg.width, seg.height, 1.0f}
        );

        const ResourceID imageID{element.style.backgroundImage};
        auto [texWidth, texHeight] = textureManager.getTextureSize<float>(imageID);

        UniformData data{};
        data.imageIndex = textureManager.getTextureIndex(imageID);
        data.model = model;
        data.imageColor = element.style.imageColor;
        data.backgroundColor = element.style.backgroundColor;
        data.uvRect = ComputeImageUVRect(seg.width, seg.height,texWidth, texHeight,element.style.imageScaleMode);
        data.imageRepeatMode = static_cast<uint32_t>(element.style.imageRepeatMode);

        uniformDatas.push_back(data);

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
            const auto& seg = element->rect.get()->content;

            const glm::mat4 model = MakeTransformModel(
            glm::vec3{seg.x, seg.y, 0.0f},
            glm::quat{1.0f, 0.0f, 0.0f, 0.0f},
            glm::vec3{seg.width, seg.height, 1.0f});

            const ResourceID imageID{element->style.backgroundImage};
            auto [texWidth, texHeight] = textureManager.getTextureSize<float>(imageID);

            data->imageIndex = textureManager.getTextureIndex(imageID);
            data->imageColor = element->style.imageColor;
            data->backgroundColor = element->style.backgroundColor;
            data->uvRect = ComputeImageUVRect(element->rect->content.width, element->rect->content.height,texWidth, texHeight,element->style.imageScaleMode);
            data->model = model;
            data->imageRepeatMode = static_cast<uint32_t>(element->style.imageRepeatMode);
        }
    }

    class Viewpanel {
        public:
        Viewpanel() = default;
        explicit Viewpanel(const TextureManager& _textureManager) : textureManager(&_textureManager) {}

        discadelta::RectSegmentContextHandler   rect{nullptr, discadelta::DestroySegmentContext<discadelta::RectSegmentContext>};
        VisualElementHandler                    rootElement{nullptr, DestroyVisualElement};

        void makeUniformData() {
            std::map<const ResourceID, const uint32_t> textureMap{};
            uniformData.clear();
            uniformData.reserve(rootElement->rect->branchCount);
            bindings.clear();
            bindings.reserve(rootElement->rect->branchCount);
            AccumulateVisualElementUniformData(uniformData, bindings, *rootElement.get(), *textureManager);
        }

        void onResize(const float& width, const float& height) {
            discadelta::UpdateSegments(*rootElement->rect, width, height, true);
            UpdateVisualElementUniformData(bindings, *textureManager);
        }

        void updateUniformBuffer(const uint32_t& imageIndex) const {
            memcpy(uniformMemory[imageIndex], uniformData.data(), sizeof(UniformData) * uniformData.size());
        }

        void makeSSBO(const gpu::GPUResources& gpu, uint32_t imageCount) {
            const size_t branchCount = rootElement->rect->branchCount;

            uniformBuffer.clear();
            uniformMemory.clear();
            uniformBuffer.reserve(imageCount);
            uniformMemory.reserve(imageCount);
            const vk::DeviceSize size = sizeof(UniformData) * branchCount;
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
                .setRange(sizeof(UniformData) * rootElement->rect->branchCount);
                bufferInfo.push_back(info);
            }

            return std::move(bufferInfo);
        }

        const TextureManager*                                       textureManager{nullptr};
        std::vector<gpu::Buffer>                                    uniformBuffer{};
        std::vector<void*>                                          uniformMemory{};
        std::vector<UniformData>                                    uniformData{};
        std::vector<std::pair<const VisualElement*, UniformData*>>  bindings{};
    };

    void MakeDescriptorSetLayout(const gpu::GPUResources& gpu, RenderResource& renderResource, const uint32_t maxCombinedImageSampler = 1) {
        auto descriptorLayout = gpu::MakeDescriptorSetLayout(gpu,{
            gpu::MakeStorageBufferLayoutBinding(1),
            gpu::MakeUniformBufferLayoutBinding(1),
            gpu::MakeSampledImageLayoutBinding(maxCombinedImageSampler),
            gpu::MakeSamplerLayoutBinding(REPEAT_SAMPLER_CONFIGS.size())
        });

        renderResource.descriptorSetLayout.emplace(std::move(descriptorLayout));
    }

    void MakePipelineLayout(const gpu::GPUResources& gpu, RenderResource& renderResource) {
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo
            .setSetLayoutCount(1)
            .setPSetLayouts(&*renderResource.descriptorSetLayout.value())
            .setPushConstantRangeCount(0);

        renderResource.pipelineLayout.emplace(*gpu.device, pipelineLayoutInfo);
    }

    void MakePipeline(const gpu::GPUResources& gpu, const vk::Format& depthFormat, const gpu::SwapchainResource& swapchain, const std::vector<char>& shaderCode, RenderResource& renderResource) {
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
            .setDepthTestEnable(true)
            .setDepthWriteEnable(true)
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
            .setLayout(*renderResource.pipelineLayout.value())
            .setRenderPass(nullptr)
            .setSubpass(0)
            .setPNext(&renderingInfo);

        renderResource.pipelineCache.emplace(*gpu.device, vk::PipelineCacheCreateInfo());
        renderResource.pipeline.emplace(*gpu.device, *renderResource.pipelineCache, pipelineInfo);
    }

    void MakeDescriptorPool(const gpu::GPUResources& gpu, const gpu::SwapchainResource& swapchain, RenderResource& renderResource) {
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
        renderResource.descriptorPool.emplace(*gpu.device, poolInfo);
    }

    void MakeDescriptorSets(const UFoxWindow& window, const uint32_t& imageCount, const std::optional<Viewpanel>& panel, RenderResource& renderResource, const TextureManager & textureManager, const uint32_t& samplerImageCount) {
        std::vector<vk::DescriptorSetLayout> layouts(imageCount, *renderResource.descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo
            .setDescriptorPool( *renderResource.descriptorPool )
            .setDescriptorSetCount( static_cast<uint32_t>( layouts.size() ) )
            .setPSetLayouts( layouts.data() );

        renderResource.descriptorSets.clear();

        auto sets = window.gpuResource.device->allocateDescriptorSets(allocInfo);
        const auto ssboInfo = panel->makeDescriptorBufferInfo();
        const auto imageInfo = textureManager.makeSampledImageDescriptorImageInfos();
        const uint32_t descriptorCount = std::min<uint32_t>(samplerImageCount, static_cast<uint32_t>(imageInfo.size()));

        std::vector<vk::DescriptorImageInfo> samplerInfo;
        samplerInfo.reserve(renderResource.repeatSamplers.size());

        for (const auto& sampler : renderResource.repeatSamplers) {
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
            renderResource.descriptorSets.push_back(std::move(sets[i]));
            window.gpuResource.device->updateDescriptorSets(write, {});
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

            if (_textureManager != nullptr) {
                rootPanel.emplace(*textureManager);
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
            renderResource.repeatSamplers.reserve(REPEAT_SAMPLER_CONFIGS.size());

            for (auto [addressModeU, addressModeV, addressModeW, borderColor] : REPEAT_SAMPLER_CONFIGS) {
                gpu::MakeSampler(
                    gpuResources,
                    renderResource.repeatSamplers,
                    addressModeU,
                    addressModeV,
                    addressModeW,
                    borderColor
                );
            }

            rootPanel->rect = discadelta::CreateSegmentContext<discadelta::RectSegmentContext, discadelta::RectSegmentCreateInfo>({
                            .name          = "gui-root-rect",
                            .width         = 0.0f,
                            .widthMin      = 0.0f,
                            .widthMax      = 0.0f,
                            .height        = 0.0f,
                            .heightMin     = 0.0f,
                            .heightMax     = std::numeric_limits<float>::max(),
                            .direction     = discadelta::FlexDirection::Row,
                            .flexCompress  = 1.0f,
                            .flexExpand    = 1.0f,
                            .order         = 0
                            });

            rootPanel->rootElement.reset();
            rootPanel->rootElement = CreateVisualElement("root-panel-element");

            discadelta::Link(*rootPanel->rect.get(), *rootPanel->rootElement->rect.get());

            v1 = CreateVisualElement("v1");
            v2 = CreateVisualElement("v2");
            v3 = CreateVisualElement("v3");

            v1->style.backgroundColor = glm::vec4{1.0f, 1.0f, 0.0f, 1.0f};
            v1->style.backgroundImage = TESTER_TEXTURE_ID.data;
            //v2->style.backgroundColor = glm::vec4{0.0f, 1.0f, 0.0f, 0.0f};
            //v2->style.imageColor = glm::vec4{0.0f, 0.0f, 1.0f, 1.0f};
            v2->style.backgroundImage = "shaderIcon.png";
            v2->style.imageScaleMode = ImageScaleMode::eScaleToFit;
            v2->style.imageRepeatMode = ImageRepeatMode::eNone;
            v3->style.backgroundImage = "NotoSans-Regular_latin_mtsdf";

            rootPanel->rootElement->link(*v1.get());
            rootPanel->rootElement->link(*v2.get());
            rootPanel->rootElement->link(*v3.get());

            rootPanel->rootElement->style.backgroundImage = "default";


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
            rootPanel->makeSSBO(window->gpuResource, window->getImageCount());
            MakeDescriptorSetLayout(gpu, renderResource, samplerImageCount);
            MakePipelineLayout(gpu, renderResource);
            MakePipeline(gpu, window->depthResource->format, *window->windowResource->swapchainResource, shaderCode, renderResource);
            MakeDescriptorPool(gpu, *window->windowResource->swapchainResource, renderResource);
            MakeDescriptorSets(*window, window->getImageCount(), rootPanel, renderResource, *textureManager, samplerImageCount);
            updateViewportSize(width, height);

            debug::log(debug::LogLevel::eInfo, "GUI Document InitResource : Susses");
        }

        void onPostGainsFocus() {
            updateTextureDescriptorSets();
        }

        std::optional<Viewpanel>                        rootPanel{};
        VisualElementHandler                            v1 {nullptr, DestroyVisualElement};
        VisualElementHandler                            v2 {nullptr, DestroyVisualElement};
        VisualElementHandler                            v3 {nullptr, DestroyVisualElement};

    private:
        UFoxWindow*                                     window = nullptr;
        MeshManager*                                    meshManager = nullptr;
        TextureManager*                                 textureManager = nullptr;
        const ResourceID*                               meshID{nullptr};
        const Mesh*                                     mesh{nullptr};
        const Texture*                                  texture{nullptr};
        RenderResource                                  renderResource{};
        uint32_t                                        samplerImageCount = MAX_GUI_TEXTURES;

        void updateViewportSize(const float& width, const float& height) {
            rootPanel->onResize(width,height);
        }

        void updateUniformBuffer(const uint32_t& currentImage) const {
            rootPanel->updateUniformBuffer(currentImage);
        }

        void updateTextureDescriptorSets() {
            if (renderResource.descriptorSets.empty()) return;

            const std::vector<vk::DescriptorImageInfo> imageInfo = textureManager->makeSampledImageDescriptorImageInfos();
            const uint32_t descriptorCount = std::min<uint32_t>(samplerImageCount, static_cast<uint32_t>(imageInfo.size()));

            for (size_t i = 0; i < window->getImageCount(); i++) {
                vk::WriteDescriptorSet write{};
                write.setDstSet(*renderResource.descriptorSets[i])
                    .setDstBinding(2)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eSampledImage)
                    .setDescriptorCount(descriptorCount)
                    .setPImageInfo(imageInfo.data());
                window->gpuResource.device->updateDescriptorSets({write}, {});
            }

            rootPanel->makeUniformData();
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
                         .setPColorAttachments(&colorAttachment)
                         .setPDepthAttachment(&depthAttachment);
            cmb.beginRendering(renderingInfo);

            cmb.bindPipeline(vk::PipelineBindPoint::eGraphics, *renderResource.pipeline);

            cmb.setViewport(0, vk::Viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
            cmb.setScissor(0, rect);
            cmb.setCullMode(vk::CullModeFlagBits::eFront);
            cmb.setFrontFace(vk::FrontFace::eCounterClockwise);
            cmb.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

            vk::Buffer vertexBuffers[] = {*mesh->vertexBuffer->data};
            vk::DeviceSize offsets[] = {0};
            cmb.bindVertexBuffers(0, vertexBuffers, offsets);
            cmb.bindIndexBuffer(*mesh->indexBuffer->data, 0, vk::IndexType::eUint16);

            cmb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderResource.pipelineLayout, 0, *renderResource.descriptorSets[imageIndex], nullptr);
            cmb.drawIndexed(std::size(RectIndices), rootPanel->uniformData.size(), 0, 0, 0);

            cmb.endRendering();
        }
    };
}
