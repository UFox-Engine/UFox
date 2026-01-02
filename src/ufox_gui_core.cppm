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

    VisualElement MakeVisualElement(const std::string& name) {
        VisualElement visualElement{};
        visualElement.rect = discadelta::CreateSegmentContext<discadelta::RectSegmentContext, discadelta::RectSegmentCreateInfo>({
            .name          = name,
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
        return visualElement;
    }

    void MakeDescriptorSetLayout(const gpu::vulkan::GPUResources& gpu, RenderResource& guiResource) {
        auto descriptorLayout = gpu::vulkan::MakeDescriptorSetLayout(gpu,
        {
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
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, imageCount),
            vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, imageCount)
            };
        vk::DescriptorPoolCreateInfo poolInfo{};
        poolInfo
            .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
            .setMaxSets(imageCount)
            .setPoolSizes(poolSizes);
        renderResource.descriptorPool.emplace(*gpu.device, poolInfo);
    }

    void MakeDescriptorSets(const gpu::vulkan::GPUResources& gpu, RenderResource& renderResource, const render::Texture2D & texture) {
        std::vector<vk::DescriptorSetLayout> layouts(renderResource.uniformBuffers.size(), *renderResource.descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo
            .setDescriptorPool( *renderResource.descriptorPool )
            .setDescriptorSetCount( static_cast<uint32_t>( layouts.size() ) )
            .setPSetLayouts( layouts.data() );
        renderResource.descriptorSets.clear();

        auto sets = gpu.device->allocateDescriptorSets(allocInfo);

        for (size_t i =0; i < renderResource.uniformBuffers.size(); i++) {
            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo
                .setBuffer(*renderResource.uniformBuffers[i].data)
                .setOffset(0)
                .setRange(sizeof(engine::UniformBufferObject));

            vk::DescriptorImageInfo imageInfo{};
            imageInfo
            .setSampler(*texture.sampler)
            .setImageView(*texture.view)
            .setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

            std::array<vk::WriteDescriptorSet, 2> write{};
            write[0].setDstSet(*sets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setPBufferInfo(&bufferInfo);
            write[1].setDstSet(*sets[i])
                    .setDstBinding(1)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
                    .setDescriptorCount(1)
                    .setPImageInfo(&imageInfo);
            renderResource.descriptorSets.push_back(std::move(sets[i]));
            gpu.device->updateDescriptorSets(write, {});
        }
    }

    class  Document {
    public:
        explicit Document(engine::UFoxWindow* window_ = nullptr, MeshManager* meshManager_ = nullptr) : window(window_) , meshManager(meshManager_) {
            if (window_ != nullptr) {
                window->addSystemInitEventHandlers([](void* user) { static_cast<Document*>(user)->init();}, this);
                window->addSystemInitEventHandlers([](void* user) { static_cast<Document*>(user)->initResource();}, this);
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
                meshManager->unuseMesh(meshUser, true);
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

            rootPanel.rootElement = MakeVisualElement("root-panel-element");

            discadelta::Link(*rootPanel.rect.get(), *rootPanel.rootElement.rect.get());

            int w,h = 0;
            window->windowResource->getExtent(w,h);

            updateViewportSize(static_cast<float>(w), static_cast<float>(h));


            debug::log(debug::LogLevel::eInfo, "GUI Document Init : Susses");
        }

        void initResource() {
            if (meshManager != nullptr){
                meshUser.id = CreateMeshContent(*meshManager, RECT_MESH_NAME, RectVertices, RectIndices, engine::ContentSourceType::eBuiltIn, "GUI");
                if (meshUser.id != nullptr) meshManager->useMesh(meshUser);
            }

            const gpu::vulkan::GPUResources& gpu = window->gpuResource;
            remakeUniformResource(gpu);
            const std::vector<char> shaderCode = engine::UFoxWindow::ReadFile("res/shaders/test.slang.spv");
            MakeDescriptorSetLayout(gpu, renderResource);
            MakePipelineLayout(gpu, renderResource);
            MakePipeline(gpu, *window->windowResource->swapchainResource, shaderCode, renderResource);

            textureImage = render::CreateTexture(window->gpuResource, "res/textures/rgb.png");
            render::CreateTextureImageView(gpu,*textureImage);
            render::createTextureSampler(gpu, *textureImage);

            MakeDescriptorPool(gpu, *window->windowResource->swapchainResource, renderResource);
            MakeDescriptorSets(gpu, renderResource, *textureImage);

            debug::log(debug::LogLevel::eInfo, "GUI Document InitResource : Susses");
        }


        Viewpanel rootPanel{};

    private:
        engine::UFoxWindow* window = nullptr;
        MeshManager* meshManager = nullptr;
        MeshUser meshUser{};

        RenderResource renderResource{};
        std::unique_ptr<render::Texture2D> textureImage{};

        void remakeUniformResource(const gpu::vulkan::GPUResources& gpu) {
            const uint32_t imageCount = window->getImageCount();

            renderResource.uniformBuffers.clear();
            renderResource.uniformBuffersMapped.clear();
            renderResource.uniformBuffers.reserve(imageCount);
            renderResource.uniformBuffersMapped.reserve(imageCount);

            for (uint32_t i = 0; i < imageCount; i++) {
                vk::DeviceSize bufferSize = sizeof(engine::UniformBufferObject);
                gpu::vulkan::Buffer buffer{};
                gpu::vulkan::MakeBuffer(buffer,gpu, bufferSize , vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
                renderResource.uniformBuffers.push_back(std::move(buffer));
                renderResource.uniformBuffersMapped.push_back(renderResource.uniformBuffers.back().memory->mapMemory(0, bufferSize));
            }
        }

        void updateViewportSize(const float& width, const float& height) const {
            discadelta::RectSegmentContext* rect = rootPanel.rect.get();
            if (rect == nullptr) return;
            discadelta::UpdateSegments(*rect, width, height, true);
        }

        void updateUniformBuffer(const uint32_t& currentImage) const {
            engine::UniformBufferObject ubo{};
            const discadelta::RectSegment& segment = rootPanel.rootElement.rect->content;
            int w,h = 0;
            window->windowResource->getExtent(w,h);
            ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(00.0f, 00.0f, 0.0f)) * glm::scale(glm::mat4(1.0f), glm::vec3(segment.width, segment.height, 1.0f));
            ubo.view = glm::mat4(1.0f);
            ubo.proj = glm::ortho(0.0f, static_cast<float>(w),0.0f, static_cast<float>(h),-1.0f, 1.0f);

            memcpy(renderResource.uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
        }

        void drawCanvas(const vk::raii::CommandBuffer &cmb, const uint32_t& imageIndex, const windowing::WindowResource& winResource) const {
            int height, width;

            if (winResource.getExtent(width, height)) return;

            const vk::Rect2D rect{{0, 0}, {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}};
            vk::ClearColorValue clearColor{1.0f, 0.5f, 0.3f, 1.0f};

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

            vk::Buffer vertexBuffers[] = {*meshUser.mesh->vertexBuffer.data};
            vk::DeviceSize offsets[] = {0};
            cmb.bindVertexBuffers(0, vertexBuffers, offsets);
            cmb.bindIndexBuffer(*meshUser.mesh->indexBuffer.data, 0, vk::IndexType::eUint16);

            cmb.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *renderResource.pipelineLayout, 0, *renderResource.descriptorSets[imageIndex], nullptr);
            cmb.drawIndexed(std::size(RectIndices), 1, 0, 0, 0);

            cmb.endRendering();
        }
    };
}

//legacy
// export namespace ufox::gui {
//
//
//     class Renderer {
//
//     public:
//         Renderer(const gpu::vulkan::GPUResources& gpu_, const std::vector<char>& shaderCode, const uint32_t& minImageCount_, const vk::PipelineRenderingCreateInfo& renderingInfo ) : gpu(gpu_), minImageCount(minImageCount_) {
//             createDescriptorSetLayout();
//             createPipelineLayout();
//             createPipeline(shaderCode, renderingInfo);
//             createVertexBuffer();
//             createIndexBuffer();
//             createUniformBuffers(minImageCount);
//             createDescriptorPool(minImageCount);
//             createDescriptorSets(minImageCount);
//         }
//         ~Renderer() = default;
//
//         void UpdateRect(const vk::Extent2D& extent, const uint32_t currentImage) const {
//             updateUniformBuffer(extent, currentImage);
//         }
//
//         void DrawRect(const vk::raii::CommandBuffer &cmd, const vk::Extent2D& extent, const uint32_t currentImage) const {
//             cmd.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
//
//             cmd.setViewport(0, vk::Viewport{ 0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f });
//             cmd.setScissor(0, vk::Rect2D{ {0, 0}, extent });
//             cmd.setCullMode(vk::CullModeFlagBits::eNone);
//             cmd.setFrontFace(vk::FrontFace::eClockwise);
//             cmd.setPrimitiveTopology(vk::PrimitiveTopology::eTriangleList);
//
//
//             // Bind shared vertex and index buffers
//             vk::Buffer vertexBuffers[] = {*vertexBuffer->data};
//             vk::DeviceSize offsets[] = {0};
//             cmd.bindVertexBuffers(0, vertexBuffers, offsets);
//             cmd.bindIndexBuffer(*indexBuffer->data, 0, vk::IndexType::eUint16);
//
//
//             cmd.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, *pipelineLayout, 0, *descriptorSets[currentImage], nullptr);
//             cmd.drawIndexed(std::size(Indices), 1, 0, 0, 0);
//         }
//
//     private:
//
//         void updateUniformBuffer(const vk::Extent2D& extent, const uint32_t currentImage) const {
//
//             UniformBufferObject ubo{};
//
//             ubo.model = glm::translate(glm::mat4(1.0f), glm::vec3(50, 50, 0.0f)) *
//                    glm::scale(glm::mat4(1.0f), glm::vec3(500, 500, 1.0f));
//             ubo.view = glm::mat4(1.0f);
//             ubo.proj = glm::ortho(0.0f, static_cast<float>(extent.width),0.0f, static_cast<float>(extent.height), -1.0f, 1.0f);
//
//
//             memcpy(*uniformBuffers[currentImage].mapped, &ubo, sizeof(ubo));
//         }
//
//
//         void createVertexBuffer() {
//             gpu::vulkan::Buffer stagingBuffer(gpu, GUI_RECT_MESH_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferSrc);
//
//             auto pData = static_cast<uint8_t *>( stagingBuffer.memory->mapMemory( 0, GUI_RECT_MESH_BUFFER_SIZE ) );
//             memcpy( pData, Geometries, GUI_RECT_MESH_BUFFER_SIZE );
//             stagingBuffer.memory->unmapMemory();
//
//             vertexBuffer.emplace(gpu, GUI_RECT_MESH_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferDst|vk::BufferUsageFlagBits::eVertexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
//             gpu::vulkan::CopyBuffer(gpu, stagingBuffer, *vertexBuffer, GUI_RECT_MESH_BUFFER_SIZE);
//         }
//
//         void createDescriptorSets(const uint32_t& minImageCount) {
//             std::vector<vk::DescriptorSetLayout> layouts(minImageCount, *descriptorSetLayout);
//             vk::DescriptorSetAllocateInfo allocInfo{};
//             allocInfo
//                 .setDescriptorPool( *descriptorPool )
//                 .setDescriptorSetCount( static_cast<uint32_t>( layouts.size() ) )
//                 .setPSetLayouts( layouts.data() );
//
//             descriptorSets.clear();
//             descriptorSets = gpu.device->allocateDescriptorSets(allocInfo);
//
//             for (size_t i = 0; i < minImageCount; i++) {
//                 vk::DescriptorBufferInfo bufferInfo{};
//                 bufferInfo
//                     .setBuffer(*uniformBuffers[i].buffer->data)
//                     .setOffset(0)
//                     .setRange(sizeof(UniformBufferObject));
//
//                 vk::WriteDescriptorSet write{};
//                 write.setDstSet(descriptorSets[i])
//                         .setDstBinding(0)
//                         .setDstArrayElement(0)
//                         .setDescriptorType(vk::DescriptorType::eUniformBuffer)
//                         .setDescriptorCount(1)
//                         .setPBufferInfo(&bufferInfo);
//
//                 gpu.device->updateDescriptorSets(write, nullptr);
//             }
//         }
//
//         void createDescriptorPool(const uint32_t& minImageCount) {
//             vk::DescriptorPoolSize poolSize{};
//             poolSize
//                 .setType(vk::DescriptorType::eUniformBuffer)
//                 .setDescriptorCount(minImageCount);
//
//             vk::DescriptorPoolCreateInfo poolInfo{};
//             poolInfo
//                 .setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
//                 .setMaxSets(minImageCount)
//                 .setPoolSizeCount(1)
//                 .setPPoolSizes(&poolSize);
//
//             descriptorPool.emplace(*gpu.device, poolInfo);
//         }
//
//         void createIndexBuffer() {
//             gpu::vulkan::Buffer stagingBuffer(gpu,GUI_INDEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferSrc);
//
//             auto pData = static_cast<uint8_t *>( stagingBuffer.memory->mapMemory( 0, GUI_INDEX_BUFFER_SIZE ) );
//             memcpy( pData, Indices, GUI_INDEX_BUFFER_SIZE );
//             stagingBuffer.memory->unmapMemory();
//
//             indexBuffer.emplace(gpu, GUI_INDEX_BUFFER_SIZE, vk::BufferUsageFlagBits::eTransferDst|vk::BufferUsageFlagBits::eIndexBuffer, vk::MemoryPropertyFlagBits::eDeviceLocal);
//             gpu::vulkan::CopyBuffer(gpu, stagingBuffer, *indexBuffer, GUI_INDEX_BUFFER_SIZE);
//         }
//
//         void createDescriptorSetLayout() {
//             vk::DescriptorSetLayoutBinding vertexLayoutBinding{};
//             vertexLayoutBinding
//                 .setBinding(0)
//                 .setDescriptorType(vk::DescriptorType::eUniformBuffer)
//                 .setDescriptorCount(1)
//                 .setStageFlags(vk::ShaderStageFlagBits::eVertex)
//                 .setPImmutableSamplers(nullptr);
//
//             // vk::DescriptorSetLayoutBinding samplerLayoutBinding{};
//             // samplerLayoutBinding.setBinding(1)
//             //                     .setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
//             //                     .setDescriptorCount(1)
//             //                     .setStageFlags(vk::ShaderStageFlagBits::eFragment)
//             //                     .setPImmutableSamplers(nullptr);
//             //
//             // vk::DescriptorSetLayoutBinding guiStyleLayoutBinding{};
//             // guiStyleLayoutBinding.setBinding(2)
//             //                           .setDescriptorType(vk::DescriptorType::eUniformBuffer)
//             //                           .setDescriptorCount(1)
//             //                           .setStageFlags(vk::ShaderStageFlagBits::eFragment)
//             //                           .setPImmutableSamplers(nullptr);
//
//             std::array bindings = { vertexLayoutBinding };
//
//             vk::DescriptorSetLayoutCreateInfo layoutInfo{};
//             layoutInfo.setBindingCount( bindings.size())
//                       .setPBindings(bindings.data());
//
//             descriptorSetLayout.emplace(*gpu.device, layoutInfo);
//         }
//
//         void createPipelineLayout() {
//             vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
//             pipelineLayoutInfo.setSetLayoutCount(1)
//             .setPSetLayouts(&**descriptorSetLayout);
//
//             pipelineLayout.emplace(*gpu.device, pipelineLayoutInfo);
//         }
//
//         void createPipeline(const std::vector<char>& shaderCode,const vk::PipelineRenderingCreateInfo& renderingInfo) {
//
//             vk::raii::ShaderModule shaderModule = gpu::vulkan::CreateShaderModule(gpu ,shaderCode);
//             std::array stages = {
//                 vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, *shaderModule, "vertMain" },
//                 vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, *shaderModule, "fragMain" }
//             };
//
//
//             std::array bindingDescription{ Vertex::getBindingDescription(0)};
//             auto attributeDescriptions = Vertex::getAttributeDescriptions(0);
//
//             vk::PipelineVertexInputStateCreateInfo vertexInput = gpu::vulkan::MakePipeVertexInputState(bindingDescription, attributeDescriptions);
//
//
//             vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
//             inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList)
//                          .setPrimitiveRestartEnable(false);
//
//             vk::PipelineViewportStateCreateInfo viewportState{};
//             viewportState.setViewportCount(1).setScissorCount(1);
//
//             vk::PipelineRasterizationStateCreateInfo rasterizer{};
//             rasterizer.setPolygonMode(vk::PolygonMode::eFill)
//                       .setDepthBiasEnable(false)
//                       .setDepthClampEnable(false)
//                       .setRasterizerDiscardEnable(false)
//                       .setLineWidth(1.0f);
//
//             vk::PipelineMultisampleStateCreateInfo multisample{};
//             multisample.setRasterizationSamples(vk::SampleCountFlagBits::e1);
//
//             vk::PipelineColorBlendAttachmentState blendAttachment{};
//             blendAttachment.setBlendEnable(true)
//                            .setSrcColorBlendFactor(vk::BlendFactor::eSrcAlpha) // Use alpha for color
//                            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha) // 1 - alpha for background
//                            .setColorBlendOp(vk::BlendOp::eAdd) // Add blended colors
//                            .setSrcAlphaBlendFactor(vk::BlendFactor::eOne) // Preserve alpha
//                            .setDstAlphaBlendFactor(vk::BlendFactor::eZero)
//                            .setAlphaBlendOp(vk::BlendOp::eAdd)
//                            .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
//                                               vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
//
//             vk::PipelineColorBlendStateCreateInfo blendState{};
//             blendState.setAttachmentCount(1).setPAttachments(&blendAttachment);
//
//             std::array dynamicStates = { vk::DynamicState::eViewport, vk::DynamicState::eScissor, vk::DynamicState::eCullMode,
//                                          vk::DynamicState::eFrontFace, vk::DynamicState::ePrimitiveTopology };
//             vk::PipelineDynamicStateCreateInfo dynamicState{};
//             dynamicState.setDynamicStateCount(dynamicStates.size()).setPDynamicStates(dynamicStates.data());
//
//
//             vk::PipelineDepthStencilStateCreateInfo depthStencil{};
//             depthStencil.setDepthTestEnable(false)
//                    .setDepthWriteEnable(false)
//                    .setDepthCompareOp(vk::CompareOp::eNever)
//                    .setDepthBoundsTestEnable(false)
//                    .setStencilTestEnable(false);
//
//             vk::GraphicsPipelineCreateInfo pipelineInfo{};
//             pipelineInfo.setStageCount(stages.size())
//                         .setPStages(stages.data())
//                         .setPVertexInputState(&vertexInput)
//                         .setPInputAssemblyState(&inputAssembly)
//                         .setPViewportState(&viewportState)
//                         .setPRasterizationState(&rasterizer)
//                         .setPMultisampleState(&multisample)
//                         .setPColorBlendState(&blendState)
//                         .setPDynamicState(&dynamicState)
//                         .setPDepthStencilState(&depthStencil)
//                         .setLayout(**pipelineLayout)
//                         .setRenderPass(nullptr)
//                         .setSubpass(0)
//                         .setPNext(&renderingInfo);
//
//
//             pipelineCache.emplace(*gpu.device, vk::PipelineCacheCreateInfo());
//             pipeline.emplace(*gpu.device, *pipelineCache, pipelineInfo);
//         }
//
//         void createUniformBuffers(const uint32_t& minImageCount) {
//             uniformBuffers.clear();
//             uniformBuffers.reserve(minImageCount);
//
//             for (size_t i = 0; i < minImageCount; i++) {
//                 gpu::vulkan::RemappableBuffer ubo{};
//
//                 vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
//                 ubo.buffer.emplace(gpu, bufferSize, vk::BufferUsageFlagBits::eUniformBuffer);
//                 ubo.mapped.emplace(ubo.buffer->memory->mapMemory(0, bufferSize));
//                 uniformBuffers.emplace_back(std::move(ubo));
//             }
//         }
//
//
//

//
//         const gpu::vulkan::GPUResources&                gpu;
//         const uint32_t                                  minImageCount;
//         std::optional<vk::raii::PipelineCache>          pipelineCache{};
//         std::optional<vk::raii::Pipeline>               pipeline{};
//         std::optional<vk::raii::DescriptorSetLayout>    descriptorSetLayout{};
//         std::optional<vk::raii::PipelineLayout>         pipelineLayout{};
//         std::optional<gpu::vulkan::Buffer>              vertexBuffer{};
//         std::optional<gpu::vulkan::Buffer>              indexBuffer{};
//         std::vector<StyleBuffer>                        styleBuffers{};
//         std::vector<gpu::vulkan::RemappableBuffer>      uniformBuffers{};
//         std::optional<vk::raii::DescriptorPool>         descriptorPool{};
//         std::vector<vk::raii::DescriptorSet>            descriptorSets{};
//
//         std::optional<gpu::vulkan::TextureImage>        textureImage{};
//     };
// }
