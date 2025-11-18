module;

#include <vulkan/vulkan_raii.hpp>
#include <functional>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <ktx.h>

export module ufox_gui;

import ufox_lib;
import ufox_graphic_device;
import ufox_geometry;

export namespace ufox::gui {
    constexpr void MakeDescriptorPool(const gpu::vulkan::GPUResources& gpu, const uint32_t& minImageCount, GUIResource& guiResource) {
        auto pool = gpu::vulkan::MakeDescriptorPool(gpu, {{vk::DescriptorType::eUniformBuffer, minImageCount}});

        guiResource.descriptorPool.emplace(std::move(pool));
    }

    constexpr void MakeDescriptorSetLayout(const gpu::vulkan::GPUResources& gpu, GUIResource& guiResource) {
        auto descriptorLayout = gpu::vulkan::MakeDescriptorSetLayout(gpu,
        {{
            vk::DescriptorType::eUniformBuffer,
            1,
            vk::ShaderStageFlagBits::eVertex,
            nullptr
        }});

        guiResource.descriptorSetLayout.emplace(std::move(descriptorLayout));
    }

    constexpr void MakePipelineLayout(const gpu::vulkan::GPUResources& gpu, GUIResource& guiResource) {
        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo
          .setSetLayoutCount(1)
          .setPSetLayouts(&*guiResource.descriptorSetLayout.value());

        guiResource.pipelineLayout.emplace(*gpu.device, pipelineLayoutInfo);
    }

    void MakePipeline(const gpu::vulkan::GPUResources& gpu, const gpu::vulkan::SwapchainResource& swapchain, const std::vector<char>& shaderCode, GUIResource& guiResource) {
        vk::raii::ShaderModule shaderModule = gpu::vulkan::CreateShaderModule(gpu ,shaderCode);
        std::array stages = {
            vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, *shaderModule, "vertMain" },
            vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, *shaderModule, "fragMain" }
        };

        std::array bindingDescription{geometry::Vertex::getBindingDescription(0)};
        auto attributeDescriptions = geometry::Vertex::getAttributeDescriptions(0);

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
            .setDstColorBlendFactor(vk::BlendFactor::eOneMinusSrcAlpha) // 1 - alpha for background
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
            .setLayout(*guiResource.pipelineLayout.value())
            .setRenderPass(nullptr)
            .setSubpass(0)
            .setPNext(&renderingInfo);

        guiResource.pipelineCache.emplace(*gpu.device, vk::PipelineCacheCreateInfo());
        guiResource.pipeline.emplace(*gpu.device, *guiResource.pipelineCache, pipelineInfo);
    }

    constexpr GUIElement MakeGUIElement(const gpu::vulkan::GPUResources& gpu, const GUIResource& guiResource, const uint32_t& minImageCount) {
        GUIElement guiElement{};

        guiElement.uniformBuffers.clear();
        guiElement.uniformBuffersMapped.clear();

        guiElement.uniformBuffers.reserve(minImageCount);
        guiElement.uniformBuffersMapped.reserve(minImageCount);

        guiElement.descriptorSets.clear();

        const std::vector<vk::DescriptorSetLayout> layouts(minImageCount, *guiResource.descriptorSetLayout);
        vk::DescriptorSetAllocateInfo allocInfo{};
        allocInfo
            .setDescriptorPool( *guiResource.descriptorPool )
            .setDescriptorSetCount( static_cast<uint32_t>( layouts.size() ) )
            .setPSetLayouts( layouts.data() );

        guiElement.descriptorSets = gpu.device->allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < minImageCount; i++) {
            guiElement.uniformBuffers.push_back(gpu::vulkan::MakeBuffer(gpu, gpu::vulkan::UBO_BUFFER_SIZE, vk::BufferUsageFlagBits::eUniformBuffer
                                                        , vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent));
            guiElement.uniformBuffersMapped.push_back( guiElement.uniformBuffers[i].memory->mapMemory(0, gpu::vulkan::UBO_BUFFER_SIZE));

            vk::DescriptorBufferInfo bufferInfo{};
            bufferInfo
                .setBuffer(*guiElement.uniformBuffers[i].data)
                .setOffset(0)
                .setRange(gpu::vulkan::UBO_BUFFER_SIZE);

            vk::WriteDescriptorSet write{};
            write.setDstSet(guiElement.descriptorSets[i])
                    .setDstBinding(0)
                    .setDstArrayElement(0)
                    .setDescriptorType(vk::DescriptorType::eUniformBuffer)
                    .setDescriptorCount(1)
                    .setPBufferInfo(&bufferInfo);

            gpu.device->updateDescriptorSets(write, nullptr);
        }

        return guiElement;
    }

    constexpr GUIResource MakeGuiResource(const gpu::vulkan::GPUResources& gpu, const gpu::vulkan::SwapchainResource& swapchain, const std::vector<char>& shaderCode) {
        GUIResource guiResource{};

        MakeDescriptorSetLayout(gpu, guiResource);
        MakePipelineLayout(gpu, guiResource);
        MakePipeline(gpu, swapchain, shaderCode, guiResource);
        guiResource.meshResource.emplace(geometry::CreateDefaultQuadMesh(gpu));
        MakeDescriptorPool(gpu, swapchain.getImageCount(), guiResource);
        guiResource.elements.emplace(MakeGUIElement(gpu, guiResource, swapchain.getImageCount()));


        return guiResource;
    }
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
//         // void CreateTextureImage(const vk::raii::CommandBuffer& cmb) {
//         //     // Load KTX2 texture instead of using stb_image
//         //     ktxTexture* kTexture;
//         //     KTX_error_code result = ktxTexture_CreateFromNamedFile(
//         //         "res/textures/rgb.png",
//         //         KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
//         //         &kTexture);
//         //
//         //     if (result != KTX_SUCCESS) {
//         //         throw std::runtime_error("failed to load ktx texture image!");
//         //     }
//         //
//         //     // Get texture dimensions and data
//         //     uint32_t texWidth = kTexture->baseWidth;
//         //     uint32_t texHeight = kTexture->baseHeight;
//         //     ktx_size_t imageSize = ktxTexture_GetImageSize(kTexture, 0);
//         //     ktx_uint8_t* ktxTextureData = ktxTexture_GetData(kTexture);
//         //
//         //     // Create staging buffer
//         //     gpu::vulkan::Buffer stagingBuffer(gpu, imageSize, vk::BufferUsageFlagBits::eTransferSrc);
//         //
//         //     // Copy texture data to staging buffer
//         //     auto pData = static_cast<uint8_t *>( stagingBuffer.memory->mapMemory( 0, imageSize ) );
//         //     memcpy( pData, ktxTextureData, imageSize );
//         //     stagingBuffer.memory->unmapMemory();
//         //
//         //     // Determine the Vulkan format from KTX format
//         //     vk::Format textureFormat = vk::Format::eR8G8B8A8Srgb; // Default format, should be determined from KTX metadata
//         //
//         //     // Create the texture image
//         //     textureImage.emplace(gpu, vk::Extent3D{texWidth,texHeight,1}, textureFormat, vk::ImageTiling::eOptimal,
//         //         vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, vk::MemoryPropertyFlagBits::eDeviceLocal);
//         //
//         //     // Copy data from staging buffer to texture image
//         //     gpu::vulkan::SetImageLayout(cmb, *textureImage, textureFormat, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
//         //     gpu::vulkan::CopyBufferToImage(gpu,stagingBuffer, *textureImage, {texWidth, texHeight, 1});
//         //     gpu::vulkan::SetImageLayout(cmb, *textureImage, textureFormat, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
//         //
//         //
//         //
//         //     // Cleanup KTX resources
//         //     ktxTexture_Destroy(kTexture);
//         // }
//
//         void CreateTextureImageView() {
//             gpu::vulkan::CreateImageView(gpu, *textureImage);
//         }
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
