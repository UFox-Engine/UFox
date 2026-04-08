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

    class  Viewpanel {
        public:
        discadelta::RectSegmentContextHandler   rect{nullptr, discadelta::DestroySegmentContext<discadelta::RectSegmentContext>};
        VisualElementHandler                    rootElement{nullptr, DestroyVisualElement};

        void accumulateUniformData(std::vector<UniformData>& uniformDatas, const VisualElement& element, const std::map<const ResourceID, const uint32_t>& textureMap) {
            if (!element.rect || !element.rect.get()) {
                return;  // safety - invalid rect context
            }

            const auto& seg = element.rect.get()->content;

            uint32_t imageIndex {0};
            if (const auto it = textureMap.find(
              ResourceID{element.style.backgroundImage});
              it != textureMap.end()) {imageIndex = it->second;}

            // Build model matrix – position + non-uniform scale
            // Note: we're using scale for width/height — very common for 2D UI
            const glm::mat4 model = MakeTransformModel(
                glm::vec3{seg.x, seg.y, 0.0f},
                glm::quat{1.0f, 0.0f, 0.0f, 0.0f},     // identity quaternion (no rotation for now)
                glm::vec3{seg.width, seg.height, 1.0f}
            );

            UniformData data{};
            data.imageIndex = imageIndex;
            data.model = model;
            data.imageColor = element.style.imageColor;
            data.backgroundColor = element.style.backgroundColor;

            uniformDatas.push_back(data);

            // Recurse into THIS element's children — not root!
            for (const VisualElement* child : element.hierarchy) {
                if (child) {
                    accumulateUniformData(uniformDatas, *child, textureMap);
                }
            }
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

        [[nodiscard]] vk::DescriptorBufferInfo MakeDescriptorBufferInfo(const size_t index) const {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo
                .setBuffer(*uniformBuffer[index].data)
                .setOffset(0)
                .setRange(sizeof(UniformData) * rootElement->rect->branchCount);
            return bufferInfo;
        }

        std::vector<gpu::Buffer>                uniformBuffer{};
        std::vector<void*>                      uniformMemory{};
        std::vector<UniformData>                uniformData{};
    };

    void MakeDescriptorSetLayout(const gpu::GPUResources& gpu, RenderResource& guiResource, const uint32_t maxCombinedImageSampler = 1) {
        auto descriptorLayout = gpu::MakeDescriptorSetLayout(gpu,{
            gpu::MakeStorageBufferLayoutBinding(1),
            gpu::MakeUniformBufferLayoutBinding(1),
            gpu::MakeCombinedImageSamplerLayoutBinding(maxCombinedImageSampler)
        });

        guiResource.descriptorSetLayout.emplace(std::move(descriptorLayout));
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
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, imageCount)
            };
        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            .setMaxSets(imageCount)
            .setPoolSizes(poolSizes);
        renderResource.descriptorPool.emplace(*gpu.device, poolInfo);
    }

    void MakeDescriptorSets(const UFoxWindow& window, const uint32_t& imageCount, const Viewpanel& panel, RenderResource& renderResource, const TextureManager & manager, const uint32_t& maxImageSize) {
        std::vector<vk::DescriptorSetLayout> layouts(imageCount, *renderResource.descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo
            .setDescriptorPool( *renderResource.descriptorPool )
            .setDescriptorSetCount( static_cast<uint32_t>( layouts.size() ) )
            .setPSetLayouts( layouts.data() );
        renderResource.descriptorSets.clear();

        auto sets = window.gpuResource.device->allocateDescriptorSets(allocInfo);

        std::vector<vk::DescriptorImageInfo> imageInfo = manager.makeImageDescriptorImageInfos();
        debug::log(debug::LogLevel::eInfo, "GUI Document MakeDescriptorSets : Susses {}", imageInfo.size());

        for (size_t i =0; i < imageCount; i++) {
            vk::DescriptorBufferInfo ssboInfo = panel.MakeDescriptorBufferInfo(i);

            std::array<vk::WriteDescriptorSet, 3> write{};
            write[0].setDstSet(*sets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setPBufferInfo(&ssboInfo);
            write[1] = gpu::MakeBufferWriteDescriptorSet(sets[i], window.MakeScreenViewProjectionBufferInfo(), 1, SCREEN_VIEW_PROJECTION_BUFFER_TYPE);
            write[2].setDstSet(*sets[i])
            .setDstBinding(2)
            .setDstArrayElement(0)
            .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
            .setDescriptorCount(imageInfo.size())
            .setPImageInfo(imageInfo.data());
            renderResource.descriptorSets.push_back(std::move(sets[i]));
            window.gpuResource.device->updateDescriptorSets(write, {});
        }
    }

    inline ResourceID TESTER_TEXTURE_ID{"grok-image-b9472c20-0cfc-4f3d-b558-d5c3066aa081.jpg"};

    class  Document {
    public:
        explicit Document(UFoxWindow* _window = nullptr, MeshManager* _meshManager = nullptr, TextureManager* _textureManager = nullptr) : window(_window) , meshManager(_meshManager), textureManager(_textureManager) {
            if (_window != nullptr) {
                window->registerSystemInitEventHandlers([](void* user) { static_cast<Document*>(user)->init();}, this);
                window->registerResourceInitEventHandlers([](const float& w, const float& h, void* user) { static_cast<Document*>(user)->initResource(w,h);}, this);
                window->registerResizeEventHandlers([](const float& w, const float& h, void* user) {  static_cast<Document*>(user)->updateViewportSize(w,h);}, this);
                window->registerUpdateBufferEventHandlers([](const uint32_t& currentImage, void* user){ static_cast<Document*>(user)->updateUniformBuffer(currentImage);}, this);
                window->registerDrawCanvasEventHandlers([](const vk::raii::CommandBuffer& cmb,const uint32_t& imageIndex, const gpu::WindowResource& winResource,
                    const vk::RenderingAttachmentInfo& colorAttachment, const vk::RenderingAttachmentInfo& depthAttachment, void* user ) { static_cast<Document*>(user)->drawCanvas(cmb, imageIndex, winResource, colorAttachment, depthAttachment);}, this);
            }
        }

        ~Document() {
            if (window != nullptr) {
                window->unregisterSystemInitEventHandlers(this);
                window->unregisterResourceInitEventHandlers(this);
                window->unregisterResizeEventHandlers(this);
                window->unregisterUpdateBufferEventHandlers(this);
                window->unregisterDrawCanvasEventHandlers(this);
                window = nullptr;
            }

            if (meshManager != nullptr) {
                meshManager = nullptr;
            }

        }

        void init() {
            rootPanel.rect = discadelta::CreateSegmentContext<discadelta::RectSegmentContext, discadelta::RectSegmentCreateInfo>({
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

            rootPanel.rootElement.reset();
            rootPanel.rootElement = CreateVisualElement("root-panel-element");

            discadelta::Link(*rootPanel.rect.get(), *rootPanel.rootElement->rect.get());

            v1 = CreateVisualElement("v1");
            v2 = CreateVisualElement("v2");
            v3 = CreateVisualElement("v3");

            v1->style.backgroundColor = glm::vec4{1.0f, 1.0f, 0.0f, 1.0f};
            v1->style.backgroundImage = TESTER_TEXTURE_ID.data;
            v2->style.backgroundColor = glm::vec4{0.0f, 1.0f, 0.0f, 0.0f};
            v2->style.imageColor = glm::vec4{0.0f, 0.0f, 1.0f, 1.0f};
            v2->style.backgroundImage = "shaderIcon.png";
            v3->style.backgroundImage = "rgb.png";

            rootPanel.rootElement->link(*v1.get());
            rootPanel.rootElement->link(*v2.get());
            rootPanel.rootElement->link(*v3.get());

            rootPanel.rootElement->style.backgroundImage = "default";


            debug::log(debug::LogLevel::eInfo, "GUI Document Init : Susses");
        }

        void initResource(const float& width, const float& height) {
            if (meshManager != nullptr){
                ResourceContextCreateInfo info{};
                info.setName(RECT_MESH_NAME)
                    .setSourceType(SourceType::eBuiltIn)
                    .setCategory("GUI");
                meshID = MakeMeshContent(*meshManager, RectVertices, RectIndices, info);
                mesh = meshManager->getMesh(*meshID);
                if (mesh) {
                    debug::log(debug::LogLevel::eInfo, "GUI Document InitMesh ({}): Susses", mesh->name);
                }
            }

            uint32_t samplerImageCount = MAX_GUI_TEXTURES;

            samplerImageCount = textureManager->getMaxDescriptorSetSamplerImages(samplerImageCount);
            textureManager->buildTextureMap(&textureMap);

            const gpu::GPUResources& gpu = window->gpuResource;
            makeUniformResource();
            const std::vector<char> shaderCode = UFoxWindow::ReadFileChar("res/shaders/test.slang.spv");

            MakeDescriptorSetLayout(gpu, renderResource, samplerImageCount);
            MakePipelineLayout(gpu, renderResource);
            MakePipeline(gpu, window->depthResource->format, *window->windowResource->swapchainResource, shaderCode, renderResource);
            MakeDescriptorPool(gpu, *window->windowResource->swapchainResource, renderResource);
            MakeDescriptorSets(*window, window->getImageCount(), rootPanel, renderResource, *textureManager, samplerImageCount);
            updateViewportSize(width, height);

            debug::log(debug::LogLevel::eInfo, "GUI Document InitResource : Susses");
        }

        Viewpanel rootPanel{};
        VisualElementHandler v1 {nullptr, DestroyVisualElement};
        VisualElementHandler v2 {nullptr, DestroyVisualElement};
        VisualElementHandler v3 {nullptr, DestroyVisualElement};

    private:
        UFoxWindow* window = nullptr;
        MeshManager* meshManager = nullptr;
        TextureManager* textureManager = nullptr;
        const ResourceID* meshID{nullptr};
        const Mesh* mesh{nullptr};
        const Texture* texture{nullptr};
        RenderResource renderResource{};
        std::map<const ResourceID, const uint32_t> textureMap{};

        void makeUniformResource() {
            rootPanel.makeSSBO(window->gpuResource, window->getImageCount());
        }

        void updateViewportSize(const float& width, const float& height) {
            discadelta::RectSegmentContext* rect = rootPanel.rect.get();
            if (rect == nullptr) return;

            discadelta::UpdateSegments(*rect, width, height, true);

            rootPanel.uniformData.clear();
            rootPanel.uniformData.reserve(rootPanel.rootElement->rect->branchCount);
            rootPanel.accumulateUniformData(rootPanel.uniformData, *rootPanel.rootElement.get(), textureMap);

        }

        void updateUniformBuffer(const uint32_t& currentImage) const {
            rootPanel.updateUniformBuffer(currentImage);
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
            cmb.drawIndexed(std::size(RectIndices), rootPanel.uniformData.size(), 0, 0, 0);

            cmb.endRendering();
        }
    };
}
