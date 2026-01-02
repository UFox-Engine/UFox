//
// Created by Puwiwad on 05.10.2025.
//
module;
#include <iostream>
#include <vulkan/vulkan_raii.hpp>
#include <fstream>
#include <glm/glm.hpp>
#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif
export module ufox_engine;
import ufox_lib;
import ufox_graphic_device;
import ufox_input;
import ufox_geometry_core;
import ufox_render_core;
import ufox_gui_lib;
import ufox_gui_core;
import ufox_resource_manager;
import ufox_discadelta_lib;
import ufox_discadelta_core;

using namespace ufox::geometry;

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

export namespace ufox {

    class UFoxEngine {
    public:
        UFoxEngine() = default;

        ~UFoxEngine() {
            // Wait for all GPU operations to complete before destroying resources
            if (gpu.device) {
                gpu.device->waitIdle();
            }

            pauseRendering = true;

            if (windowResource->swapchainResource) {
                windowResource->swapchainResource->Clear();
            }
            // if (viewport && inputResource) {
            //     geometry::UnbindEvents(*viewport,*inputResource);
            // }

        }



        void Init() {
            InitializeGPU();



        }

        void Run() {
#ifdef USE_SDL
            bool running = true;
            SDL_Event event;
            while (running) {
                sdlPollEvents(event, running);
                beginUpdate();
                update();
                lateUpdate();
                render();
            }
#else
            while (!glfwWindowShouldClose(windowResource->getHandle())) {
                glfwPollEvents();
                beginUpdate();
                update();
                lateUpdate();
                render();
            }
#endif
        }

    private:
        void beginUpdate() {
            input::RefreshResources(*inputResource);
            input::UpdateGlobalMousePosition(*windowResource, *inputResource);
            input::UpdateMouseButtonAction(*inputResource);
        }

        void update() {


        }

        void lateUpdate() {

        }

        void render() {
            drawFrame();
        }


        void IniInitializeWindow() {
            windowResource.emplace(windowing::CreateWindow(*gpu.instance, "UFox", vk::Extent2D{800, 200}));
#ifdef USE_SDL
            SDL_AddEventWatch(framebufferResizeCallback, this);

#else
            glfwSetWindowUserPointer(windowResource->getHandle(), this);
            glfwSetFramebufferSizeCallback(windowResource->getHandle(), framebufferResizeCallback);
            glfwSetWindowIconifyCallback(windowResource->getHandle(), windowIconifyCallback);
            glfwSetWindowPosCallback(windowResource->getHandle(), windowMoveCallback);
            glfwSetMouseButtonCallback(windowResource->getHandle(), mouse_button_callback);

#endif
        }

        void InitializeGPU() {
            gpuCreateInfo = gpu::vulkan::CreateGraphicDeviceInfo();

            gpu.context.emplace();
            gpu.instance.emplace(gpu::vulkan::MakeInstance(gpu, gpuCreateInfo));
            gpu.physicalDevice.emplace(gpu::vulkan::PickBestPhysicalDevice(gpu));

            IniInitializeWindow();

            gpu.queueFamilyIndices.emplace(getQueueFamilyIndices());
            gpu.device.emplace(gpu::vulkan::MakeDevice(gpu, gpuCreateInfo));
            gpu.commandPool.emplace(gpu::vulkan::MakeCommandPool(gpu));

            gpu.graphicsQueue.emplace(gpu::vulkan::MakeGraphicsQueue(gpu));
            gpu.presentQueue.emplace(gpu::vulkan::MakePresentQueue(gpu));

            windowResource->swapchainResource.emplace(gpu::vulkan::MakeSwapchainResource(gpu, *windowResource, vk::ImageUsageFlagBits::eColorAttachment));
            frameResource.emplace(gpu::vulkan::MakeFrameResource(gpu, vk::FenceCreateFlagBits::eSignaled));



            inputResource.emplace();
            standardCursorResource.emplace(input::CreateStandardMouseCursor());

            std::vector<char> shaderCode = ReadFile("res/shaders/test.slang.spv");
            //guiResource.emplace(gui::MakeGuiResource(gpu, *windowResource->swapchainResource, shaderCode));

            resourceManager.emplace(gpu);
            resourceManager->SetRootPath("res/"); // Sets rootPath to "res/textures/"
            // Scan for PNG and JPEG files
            std::vector<std::string> extensions = {"png", "jpg", "jpeg"};
            auto imageFiles = resourceManager->ScanForImageFiles(extensions);
            for (const auto& file : imageFiles) {
                std::cout << "Found Image: " << file << std::endl; // e.g., "res/textures/image1.png", "res/textures/subfolder/image2.jpg"
                TextureImage texture_image {file};
            }
        }

        [[nodiscard]] gpu::vulkan::QueueFamilyIndices getQueueFamilyIndices() const {
            return gpu::vulkan::FindGraphicsAndPresentQueueFamilyIndex(gpu, *windowResource);
        }

        [[nodiscard]] vk::PipelineRenderingCreateInfo getPipelineRenderingCreateInfo() const {
            vk::PipelineRenderingCreateInfo renderingInfo{};
            renderingInfo
            .setColorAttachmentCount(1)
            .setPColorAttachmentFormats(&windowResource->swapchainResource->colorFormat);

            return renderingInfo;
        }


        static std::vector<char> ReadFile(const std::string& filename) {
            std::ifstream file(filename, std::ios::ate | std::ios::binary);

            if (!file.is_open()) {
                throw std::runtime_error("failed to open file!");
            }

            std::vector<char> buffer(file.tellg());

            file.seekg(0, std::ios::beg);
            file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));

            file.close();

            return buffer;
        }

        void recreateSwapchain() {
            // Handle window minimization
            int width = 0, height = 0;
            pauseRendering = windowResource->getExtent(width, height);

            if (pauseRendering) return;

            gpu.device->waitIdle();
            windowResource->extent = vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            windowResource->swapchainResource->Clear();
            gpu::vulkan::ReMakeSwapchainResource(*windowResource->swapchainResource, gpu, *windowResource, vk::ImageUsageFlagBits::eColorAttachment);
        }

        void recreateSwapchain(int width, int height) {

            pauseRendering = width == 0 || height == 0;

            if (pauseRendering) return;

            gpu.device->waitIdle();
            windowResource->extent = vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            windowResource->swapchainResource->Clear();
            gpu::vulkan::ReMakeSwapchainResource(*windowResource->swapchainResource, gpu, *windowResource, vk::ImageUsageFlagBits::eColorAttachment);
        }

        void drawFrame() {
            while ( vk::Result::eTimeout == gpu.device->waitForFences( *frameResource->getCurrentDrawFence(), vk::True, UINT64_MAX ) )
                ;

            if (pauseRendering) {return;}

            auto [result, imageIndex] = windowResource->swapchainResource->swapChain->acquireNextImage(UINT64_MAX, frameResource->getCurrentPresentCompleteSemaphore(), nullptr);
            windowResource->swapchainResource->currentImageIndex = imageIndex;
            gpu.device->resetFences(*frameResource->getCurrentDrawFence());
            if (result == vk::Result::eErrorOutOfDateKHR) {
                debug::log(debug::LogLevel::eInfo, "Recreating swapchain due to acquireNextImage failure");
                recreateSwapchain();
                return;
            }
            if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
                throw std::runtime_error("Failed to acquire swapchain image");
            }
            const vk::raii::CommandBuffer& cmb = frameResource->getCurrentCommandBuffer();
            cmb.reset();
            recordCommandBuffer(cmb);


            vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
            vk::SubmitInfo submitInfo{};
            submitInfo.setWaitSemaphoreCount(1)
                      .setPWaitSemaphores(&*frameResource->getCurrentPresentCompleteSemaphore())
                      .setPWaitDstStageMask(&waitDestinationStageMask)
                      .setCommandBufferCount(1)
                      .setPCommandBuffers(&*cmb)
                      .setSignalSemaphoreCount(1)
                      .setPSignalSemaphores(&*windowResource->swapchainResource->getCurrentRenderFinishedSemaphore());
            gpu.graphicsQueue->submit(submitInfo, frameResource->getCurrentDrawFence());
            vk::PresentInfoKHR presentInfo{};
            presentInfo.setWaitSemaphoreCount(1)
                       .setPWaitSemaphores(&*windowResource->swapchainResource->getCurrentRenderFinishedSemaphore())
                       .setSwapchainCount(1)
                       .setPSwapchains(&**windowResource->swapchainResource->swapChain)
                       .setPImageIndices(&windowResource->swapchainResource->currentImageIndex);

            result = gpu.presentQueue->presentKHR(presentInfo);
            if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
                framebufferResized = false;
                recreateSwapchain();
                return;
            }
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("Failed to present swapchain image");
            }
            frameResource->ContinueNextFrame();
        }

        static vk::Rect2D ConvertDiscadeltaToVkRect(const discadelta::RectSegmentContext& context) {
            auto x = static_cast<int>(context.content.x);
            auto y = static_cast<int>(context.content.y);
            auto width = static_cast<uint32_t>(context.content.width);
            auto height = static_cast<uint32_t>(context.content.height);
            return vk::Rect2D{{x,y}, {width, height}};
        }


        void recordCommandBuffer(const vk::raii::CommandBuffer& cmb) const {
            std::array<vk::ClearValue, 2> clearValues{};
            clearValues[0].color        = vk::ClearColorValue{0.0f, 0.0f, 0.0f,1.0f};
            clearValues[1].depthStencil = vk::ClearDepthStencilValue{0.0f, 0};
            vk::Extent2D extent = windowResource->extent;
            cmb.begin({});
            vk::ImageSubresourceRange range{};
            range.aspectMask     = vk::ImageAspectFlagBits::eColor;
            range.baseMipLevel   = 0;
            range.levelCount     = VK_REMAINING_MIP_LEVELS;
            range.baseArrayLayer = 0;
            range.layerCount     = VK_REMAINING_ARRAY_LAYERS;
            gpu::vulkan::TransitionImageLayout(
                cmb, windowResource->swapchainResource->getCurrentImage(),
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput ,
                {},
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,range
                );
            vk::RenderingAttachmentInfo colorAttachment{};
            colorAttachment.setImageView(windowResource->swapchainResource->getCurrentImageView())
                           .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                           .setLoadOp(vk::AttachmentLoadOp::eClear)
                           .setStoreOp(vk::AttachmentStoreOp::eStore)
                           .setClearValue(clearValues[0]);
            vk::Rect2D renderArea{};
            renderArea.offset = vk::Offset2D{0, 0};
            renderArea.extent = extent;
            vk::RenderingInfo renderingInfo{};
            renderingInfo.setRenderArea(renderArea)
                         .setLayerCount(1)
                         .setColorAttachmentCount(1)
                         .setPColorAttachments(&colorAttachment);
            cmb.beginRendering(renderingInfo);


            cmb.endRendering();

            // vk::ClearColorValue c4 = viewport->hoveredPanel == &viewpanel4.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel4->clearColor;
            // vk::ClearColorValue c2 = viewport->hoveredPanel == &viewpanel2.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel2->clearColor;
            // vk::ClearColorValue c6 = viewport->hoveredPanel == &viewpanel6.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel6->clearColor;
            // vk::ClearColorValue c7 = viewport->hoveredPanel == &viewpanel7.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel7->clearColor;
            // vk::ClearColorValue c8 = viewport->hoveredPanel == &viewpanel8.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel8->clearColor;
            // vk::ClearColorValue c9 = viewport->hoveredPanel == &viewpanel9.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel9->clearColor;
            // vk::ClearColorValue c10 =viewport->hoveredPanel == &viewpanel10.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel10->clearColor;

            // render::RenderArea(cmb, *windowResource, viewpanel2->rect, c2);
            // render::RenderArea(cmb, *windowResource, viewpanel4->rect, c4);
            // render::RenderArea(cmb, *windowResource, viewpanel6->rect, c6);
            // render::RenderArea(cmb, *windowResource, viewpanel7->rect, c7);
            // render::RenderArea(cmb, *windowResource, viewpanel8->rect, c8);
            // render::RenderArea(cmb, *windowResource, viewpanel9->rect, c9);
            // render::RenderArea(cmb, *windowResource, viewpanel10->rect, c10);



            gpu::vulkan::TransitionImageLayout(cmb, windowResource->swapchainResource->getCurrentImage(),
                vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, range);
            cmb.end();
        }

#ifdef USE_SDL
        static bool framebufferResizeCallback(void* userData, SDL_Event* event) {
            if (event->type == SDL_EVENT_WINDOW_RESIZED) {
                SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
                auto* app = static_cast<UFoxEngine*>(userData);
                int width, height;
                SDL_GetWindowSize(win, &width, &height);
                app->framebufferResized = true;
                app->recreateSwapchain(width, height);

                app->drawFrame();
            }
            return true;
        }

        void sdlPollEvents(SDL_Event& event , bool& running) {
            while (SDL_PollEvent(&event)) {
                switch (event.type) {
                    case SDL_EVENT_QUIT: {
                        if (gpu.device) {
                            gpu.device->waitIdle();
                        }
                        pauseRendering = true;
                        if (windowResource->swapchainResource) {
                            windowResource->swapchainResource->Clear();
                        }
                        // if (viewport && inputResource) {
                        //     geometry::UnbindEvents(*viewport,*inputResource);
                        // }
                        running = false;
                        SDL_RemoveEventWatch(framebufferResizeCallback, this);
                        break;
                    }
                    case SDL_EVENT_WINDOW_MOVED: {
                        windowResource->position = vk::Offset2D{event.window.data1,event.window.data2};
                        break;
                    }
                    case SDL_EVENT_WINDOW_MINIMIZED: {
                        pauseRendering = true;

                        break;
                    }
                    case SDL_EVENT_WINDOW_RESTORED: {
                        if (pauseRendering)
                            pauseRendering = false;

                        break;
                    }
                    case SDL_EVENT_MOUSE_BUTTON_DOWN:{
                        input::MouseButton button = input::MouseButton::eNone;
                        input::ActionPhase phase = input::ActionPhase::eStart;

                        if (event.button.button == SDL_BUTTON_LEFT) {button = input::MouseButton::eLeft;}
                        else if (event.button.button == SDL_BUTTON_RIGHT) {button = input::MouseButton::eRight;}
                        else if (event.button.button == SDL_BUTTON_MIDDLE) {button = input::MouseButton::eMiddle;}

                        input::CatchMouseButton(*inputResource, button, phase, 1,0);

                        break;
                    }
                    case SDL_EVENT_MOUSE_BUTTON_UP: {
                        input::MouseButton button = input::MouseButton::eNone;
                        input::ActionPhase phase = input::ActionPhase::eEnd;

                        if (event.button.button == SDL_BUTTON_LEFT) {button = input::MouseButton::eLeft;}
                        else if (event.button.button == SDL_BUTTON_RIGHT) {button = input::MouseButton::eRight;}
                        else if (event.button.button == SDL_BUTTON_MIDDLE) {button = input::MouseButton::eMiddle;}

                        input::CatchMouseButton(*inputResource, button, phase,0,0);

                        break;
                    }

                    default: {}
                }
            }
        }

#else
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
            auto app = static_cast<UFoxEngine*>(glfwGetWindowUserPointer(window));
            app->framebufferResized = true;
            app->recreateSwapchain(width, height);
            gui::UpdateViewportSize(app->vp, static_cast<float>(width), static_cast<float>(height));
            app->drawFrame();
        }

        static void windowIconifyCallback(GLFWwindow* window, int iconified) {
            auto app = static_cast<UFoxEngine*>(glfwGetWindowUserPointer(window));
            app->pauseRendering = iconified != GLFW_FALSE;
        }

        static void windowMoveCallback(GLFWwindow* window, int x, int y) {
            auto app = static_cast<UFoxEngine*>(glfwGetWindowUserPointer(window));
            app->windowResource->position = vk::Offset2D{x,y};

        }


        static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
        {
            auto app = static_cast<UFoxEngine*>(glfwGetWindowUserPointer(window));

            input::MouseButton buttonType = input::MouseButton::eNone;
            input::ActionPhase phase = input::ActionPhase::eSleep;

            if (button == GLFW_MOUSE_BUTTON_LEFT) {buttonType = input::MouseButton::eLeft;}
            else if (button == GLFW_MOUSE_BUTTON_RIGHT) {buttonType = input::MouseButton::eRight;}
            else if (button == GLFW_MOUSE_BUTTON_MIDDLE) {buttonType = input::MouseButton::eMiddle;}

            if (action == GLFW_PRESS) {phase = input::ActionPhase::eStart;}
            else if (action == GLFW_RELEASE) {phase = input::ActionPhase::eEnd;}


            input::CatchMouseButton(app->inputResource.value(), buttonType, phase, 1,0);

        }
#endif

        gpu::vulkan::GraphicDeviceCreateInfo            gpuCreateInfo{};
        gpu::vulkan::GPUResources                       gpu{};
        std::optional<windowing::WindowResource>        windowResource{};
        std::optional<gpu::vulkan::FrameResource>       frameResource{};
        std::optional<input::InputResource>             inputResource{};
        std::optional<input::StandardCursorResource>    standardCursorResource{};
        std::optional<gui::RenderResource>                 guiResource{};
        std::optional<ResourceManager>                  resourceManager{};



        discadelta::RectSegmentContextHandler rect1{nullptr, &discadelta::DestroySegmentContext<discadelta::RectSegmentContext>};
        discadelta::RectSegmentContextHandler rect2{nullptr, &discadelta::DestroySegmentContext<discadelta::RectSegmentContext>};


        bool framebufferResized = false;
        bool pauseRendering = false;
    };
}