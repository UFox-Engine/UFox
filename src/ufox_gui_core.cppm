module;


#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_raii.hpp>
#include <iostream>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_surface.h>
#include <SDL3_image/SDL_image.h>


export module ufox_gui_core;

import ufox_lib;
import ufox_engine_lib;
import ufox_engine_core;
import ufox_graphic_device;
import ufox_geometry_core;
import ufox_geometry_lib;
import ufox_gui_lib;
import ufox_discadelta_lib;
import ufox_discadelta_core;
import ufox_render_lib;
import ufox_render_core;


using namespace ufox::geometry;

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

        void accumulateRects(std::vector<glm::mat4>& container, const VisualElement& element) {
            if (!element.rect || !element.rect.get()) {
                return;  // safety - invalid rect context
            }

            const auto& seg = element.rect.get()->content;

            // Build model matrix – position + non-uniform scale
            // Note: we're using scale for width/height — very common for 2D UI
            const glm::mat4 model = engine::MakeTransformModel(
                glm::vec3{seg.x, seg.y, 0.0f},
                glm::quat{1.0f, 0.0f, 0.0f, 0.0f},     // identity quaternion (no rotation for now)
                glm::vec3{seg.width, seg.height, 1.0f}
            );

            container.push_back(model);

            // Recurse into THIS element's children — not root!
            for (VisualElement* child : element.hierarchy) {
                if (child) {
                    accumulateRects(container, *child);
                }
            }
        }

        void updateSSBOBuffer(const uint32_t& imageIndex) const {

        }

        void makeSSBO(const gpu::vulkan::GPUResources& gpu, uint32_t imageCount) {
            const size_t branchCount = rootElement->rect->branchCount;


            rectSSBO.clear();
            rectSSBOMemory.clear();
            rectSSBO.reserve(imageCount);
            rectSSBOMemory.reserve(imageCount);
            const vk::DeviceSize size = sizeof(glm::mat4) * branchCount;
            for (size_t i = 0; i < imageCount; i++) {
                gpu::vulkan::Buffer buffer;
                gpu::vulkan::MakeBuffer(buffer, gpu, size, vk::BufferUsageFlagBits::eStorageBuffer,vk::MemoryPropertyFlagBits::eHostVisible |vk::MemoryPropertyFlagBits::eHostCoherent);
                rectSSBO.push_back(std::move(buffer));
                rectSSBOMemory.push_back(rectSSBO.back().memory->mapMemory(0, size));
            }
        }

        [[nodiscard]] vk::DescriptorBufferInfo MakeDescriptorBufferInfo(const size_t index) const {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo
                .setBuffer(*rectSSBO[index].data)
                .setOffset(0)
                .setRange(sizeof(glm::mat4) * rootElement->rect->branchCount);
            return bufferInfo;
        }

        std::vector<gpu::vulkan::Buffer>      rectSSBO{};
        std::vector<void*>                    rectSSBOMemory{};
        std::vector<glm::mat4>                rectSSBOData{};

        private:


    };



    void MakeDescriptorSetLayout(const gpu::vulkan::GPUResources& gpu, RenderResource& guiResource) {
        auto descriptorLayout = gpu::vulkan::MakeDescriptorSetLayout(gpu,
        {
            {vk::DescriptorType::eStorageBuffer,1,vk::ShaderStageFlagBits::eVertex,nullptr},
            {vk::DescriptorType::eUniformBuffer,1,vk::ShaderStageFlagBits::eVertex,nullptr},
            {vk::DescriptorType::eCombinedImageSampler,1,vk::ShaderStageFlagBits::eFragment,nullptr}
        });

        guiResource.descriptorSetLayout.emplace(std::move(descriptorLayout));
    }

    void MakePipelineLayout(const gpu::vulkan::GPUResources& gpu, RenderResource& renderResource) {
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo
            .setSetLayoutCount(1)
            .setPSetLayouts(&*renderResource.descriptorSetLayout.value())
            .setPushConstantRangeCount(0);

        renderResource.pipelineLayout.emplace(*gpu.device, pipelineLayoutInfo);
    }

    void MakePipeline(const gpu::vulkan::GPUResources& gpu, const gpu::vulkan::SwapchainResource& swapchain, const std::vector<char>& shaderCode, RenderResource& renderResource) {
        vk::raii::ShaderModule shaderModule = gpu::vulkan::CreateShaderModule(gpu ,shaderCode);
        std::array stages = {
            vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, *shaderModule, "vertMain" },
            vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, *shaderModule, "fragMain" }
        };

        std::array bindingDescription{Vertex::getBindingDescription()};
        auto attributeDescriptions = Vertex::getAttributeDescriptions();

        vk::PipelineVertexInputStateCreateInfo vertexInput = gpu::vulkan::MakePipeVertexInputState(bindingDescription, attributeDescriptions);

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
            .setDepthCompareOp(vk::CompareOp::eNever)
            .setDepthBoundsTestEnable(false)
            .setStencilTestEnable(false);

        vk::PipelineRenderingCreateInfo renderingInfo{};
        renderingInfo
        .setColorAttachmentCount(1)
        .setPColorAttachmentFormats(&swapchain.colorFormat);

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

    void MakeDescriptorPool(const gpu::vulkan::GPUResources& gpu, const gpu::vulkan::SwapchainResource& swapchain, RenderResource& renderResource) {
        uint32_t imageCount = swapchain.getImageCount();
        std::array poolSizes {
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, imageCount),
            vk::DescriptorPoolSize(engine::SCREEN_VIEW_PROJECTION_BUFFER_TYPE, imageCount),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, imageCount)
            };
        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            .setMaxSets(imageCount)
            .setPoolSizes(poolSizes);
        renderResource.descriptorPool.emplace(*gpu.device, poolInfo);
    }

    void MakeDescriptorSets(const engine::UFoxWindow& window, const uint32_t& imageCount, const Viewpanel& panel, RenderResource& renderResource, const render::Texture2D & texture) {
        std::vector<vk::DescriptorSetLayout> layouts(imageCount, *renderResource.descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo
            .setDescriptorPool( *renderResource.descriptorPool )
            .setDescriptorSetCount( static_cast<uint32_t>( layouts.size() ) )
            .setPSetLayouts( layouts.data() );
        renderResource.descriptorSets.clear();

        auto sets = window.gpuResource.device->allocateDescriptorSets(allocInfo);

        for (size_t i =0; i < imageCount; i++) {
            vk::DescriptorBufferInfo ssboInfo = panel.MakeDescriptorBufferInfo(i);

            vk::DescriptorImageInfo imageInfo{};
            imageInfo
                .setSampler(*texture.sampler)
                .setImageView(*texture.view)
                .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            std::array<vk::WriteDescriptorSet, 3> write{};
            write[0].setDstSet(*sets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                    .setDescriptorCount(1)
                    .setPBufferInfo(&ssboInfo);
            write[1] = gpu::vulkan::MakeWriteDescriptorSet(sets[i], window.MakeScreenViewProjectionBufferInfo(), 1, engine::SCREEN_VIEW_PROJECTION_BUFFER_TYPE);
            write[2].setDstSet(*sets[i])
                    .setDstBinding(2)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setPImageInfo(&imageInfo);
            renderResource.descriptorSets.push_back(std::move(sets[i]));
            window.gpuResource.device->updateDescriptorSets(write, {});
        }
    }

    class  Document {
    public:
        explicit Document(engine::UFoxWindow* window_ = nullptr, MeshManager* meshManager_ = nullptr) : window(window_) , meshManager(meshManager_) {
            if (window_ != nullptr) {
                window->addSystemInitEventHandlers([](void* user) { static_cast<Document*>(user)->init();}, this);
                window->addResourceInitEventHandlers([](const float& w, const float& h, void* user) { static_cast<Document*>(user)->initResource(w,h);}, this);
                window->addResizeEventHandlers([](const float& w, const float& h, void* user) {  static_cast<Document*>(user)->updateViewportSize(w,h);}, this);
                window->addUpdateBufferEventHandlers([](const uint32_t& currentImage, void* user){ static_cast<Document*>(user)->updateUniformBuffer(currentImage);}, this);
                window->addDrawCanvasEventHandlers([](const vk::raii::CommandBuffer& cmb,const uint32_t& imageIndex,
                        const windowing::WindowResource& winResource, void* user ) { static_cast<Document*>(user)->drawCanvas(cmb, imageIndex, winResource);}, this);
            }
        }

        ~Document() {
            if (window != nullptr) {
                window->removeSystemInitEventHandlers(this);
                window->removeResourceInitEventHandlers(this);
                window->removeOnResizeEventHandlers(this);
                window->removeUpdateBufferEventHandlers(this);
                window->removeDrawCanvasEventHandlers(this);
                window = nullptr;
            }

            if (meshManager != nullptr) {
                meshManager->unuseMesh(meshUser);
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


            rootPanel.rootElement->link(*v1.get());
            rootPanel.rootElement->link(*v2.get());
            rootPanel.rootElement->link(*v3.get());

            debug::log(debug::LogLevel::eInfo, "elements count: {}", rootPanel.rootElement->rect->branchCount);


            debug::log(debug::LogLevel::eInfo, "GUI Document Init : Susses");
        }

        void initResource(const float& width, const float& height) {
            if (meshManager != nullptr){
                engine::ResourceContextCreateInfo info{};
                info.setName(RECT_MESH_NAME)
                    .setSourceType(engine::SourceType::eBuiltIn)
                    .setCategory("GUI");
                meshUser.id = CreateMeshContent(*meshManager, RectVertices, RectIndices, info);
                if (meshUser.id != nullptr) meshManager->useMesh(meshUser);
            }

            const gpu::vulkan::GPUResources& gpu = window->gpuResource;
            rootPanel.makeSSBO(gpu, window->getImageCount());
            MakeUniformResource(gpu);
            const std::vector<char> shaderCode = engine::UFoxWindow::ReadFileChar("res/shaders/test.slang.spv");
            MakeDescriptorSetLayout(gpu, renderResource);
            MakePipelineLayout(gpu, renderResource);
            MakePipeline(gpu, *window->windowResource->swapchainResource, shaderCode, renderResource);

            textureImage = render::CreateTexture(window->gpuResource, "res/textures/652234-statue-1275469_1280.jpg");
            render::CreateTextureImageView(gpu,*textureImage);
            render::createTextureSampler(gpu, *textureImage);

            MakeDescriptorPool(gpu, *window->windowResource->swapchainResource, renderResource);
            MakeDescriptorSets(*window, window->getImageCount(), rootPanel, renderResource, *textureImage);

            updateViewportSize(width, height);

            debug::log(debug::LogLevel::eInfo, "GUI Document InitResource : Susses");
        }

        Viewpanel rootPanel{};
        VisualElementHandler v1 {nullptr, DestroyVisualElement};
        VisualElementHandler v2 {nullptr, DestroyVisualElement};
        VisualElementHandler v3 {nullptr, DestroyVisualElement};

    private:
        engine::UFoxWindow* window = nullptr;
        MeshManager* meshManager = nullptr;
        MeshUser meshUser{nullptr,nullptr};

        RenderResource renderResource{};
        std::unique_ptr<render::Texture2D> textureImage{};

        void MakeUniformResource(const gpu::vulkan::GPUResources& gpu) {
            const uint32_t imageCount = window->getImageCount();

            renderResource.rectBuffers.clear();
            renderResource.rectBuffersMapped.clear();
            renderResource.rectBuffers.reserve(imageCount);
            renderResource.rectBuffersMapped.reserve(imageCount);


            for (uint32_t i = 0; i < imageCount; i++) {
                gpu::vulkan::Buffer buffer{};
                gpu::vulkan::MakeBuffer(buffer,gpu, engine::MAT4_BUFFER_SIZE , vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
                renderResource.rectBuffers.push_back(std::move(buffer));
                renderResource.rectBuffersMapped.push_back(renderResource.rectBuffers.back().memory->mapMemory(0, engine::MAT4_BUFFER_SIZE));
            }
        }

        void updateViewportSize(const float& width, const float& height) {
            discadelta::RectSegmentContext* rect = rootPanel.rect.get();
            if (rect == nullptr) return;

            discadelta::UpdateSegments(*rect, width, height, true);

            rootPanel.rectSSBOData.clear();
            rootPanel.rectSSBOData.reserve(rootPanel.rootElement->rect->branchCount);
            rootPanel.accumulateRects(rootPanel.rectSSBOData, *rootPanel.rootElement.get());

        }

        void updateUniformBuffer(const uint32_t& currentImage) const {
            memcpy(rootPanel.rectSSBOMemory[currentImage], rootPanel.rectSSBOData.data(), sizeof(glm::mat4) * rootPanel.rectSSBOData.size());
        }

        void drawCanvas(const vk::raii::CommandBuffer &cmb, const uint32_t& imageIndex, const windowing::WindowResource& winResource) const {
            int height, width;

            if (winResource.getExtent(width, height)) return;

            const vk::Rect2D rect{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};
            vk::ClearColorValue clearColor{0.0f, 0.0f, 0.0f, 1.0f};

            vk::RenderingAttachmentInfo colorAttachment{};
            colorAttachment.setImageView(winResource.swapchainResource->getCurrentImageView())
                           .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                           .setLoadOp(vk::AttachmentLoadOp::eClear)
                           .setStoreOp(vk::AttachmentStoreOp::eStore)
                           .setClearValue(clearColor);

            vk::RenderingInfo renderingInfo{};
            renderingInfo.setRenderArea(rect)
                         .setLayerCount(1)
                         .setColorAttachmentCount(1)
                         .setPColorAttachments(&colorAttachment);
            cmb.beginRendering(renderingInfo);

            cmb.bindPipeline(vk::PipelineBindPoint::eGraphics, *renderResource.pipeline);

            cmb.setViewport(0, vk::Viewport{ 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f });
            cmb.setScissor(0, rect);
            cmb.setCullMode(vk::CullModeFlagBits::eNone);
            cmb.setFrontFace(vk::FrontFace::eClockwise);
            cmb.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);

            vk::Buffer vertexBuffers[] = {*meshUser.mesh->vertexBuffer->data};
            vk::DeviceSize offsets[] = {0};
            cmb.bindVertexBuffers(0, vertexBuffers, offsets);
            cmb.bindIndexBuffer(*meshUser.mesh->indexBuffer->data, 0, vk::IndexType::eUint16);

            cmb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderResource.pipelineLayout, 0, *renderResource.descriptorSets[imageIndex], nullptr);
            cmb.drawIndexed(std::size(RectIndices), rootPanel.rectSSBOData.size(), 0, 0, 0);

            cmb.endRendering();
        }
    };
}
