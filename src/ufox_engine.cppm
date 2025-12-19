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
import ufox_geometry;
import ufox_render;
import ufox_gui;
import ufox_resource_manager;



export namespace ufox {
    gpu::vulkan::GraphicDeviceCreateInfo CreateGraphicDeviceInfo() {
        gpu::vulkan::GraphicDeviceCreateInfo createInfo{};
        createInfo.appInfo.pApplicationName = "UFox";
        createInfo.appInfo.applicationVersion = vk::makeVersion(1, 0, 0);
        createInfo.appInfo.pEngineName = "UFox Engine";
        createInfo.appInfo.engineVersion = vk::makeVersion(1, 0, 0);
        createInfo.appInfo.apiVersion = vk::ApiVersion14;
        createInfo.instanceExtensions = gpu::vulkan::GetInstanceExtensions();
        createInfo.deviceExtensions = gpu::vulkan::GetDeviceExtensions();
        createInfo.enableDynamicRendering = true;
        createInfo.enableExtendedDynamicState = true;
        createInfo.enableSynchronization2 = true;
        createInfo.commandPoolCreateFlags = vk::CommandPoolCreateFlags{
            vk::CommandPoolCreateFlagBits::eResetCommandBuffer |
            vk::CommandPoolCreateFlagBits::eTransient
        };

        return createInfo;
    }

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
            if (viewport && inputResource) {
                geometry::UnbindEvents(*viewport,*inputResource);
            }

        }

        void Init() {
            InitializeGPU();
            viewport.emplace(*windowResource);
            viewpanel1.emplace(geometry::PanelAlignment::eRow,geometry::PickingMode::eIgnore);
            viewpanel1->name = "root";
            viewpanel2.emplace();
            viewpanel2->name = "child [1]";
            //viewpanel2->width = geometry::Length::Pixels(100);
            viewpanel2->flexGlow = 0.0f;
            viewpanel2->flexShrink = 0.0f;
            //viewpanel2->resizerValue = 0.25f;
            //viewpanel2->minWidth = geometry::Length::Pixels(100);
            viewpanel2->setBackgroundColor(vk::ClearColorValue{1.0f, 0.0f, 0.0f, 1.0f});
            viewpanel3.emplace(geometry::PanelAlignment::eColumn);
            viewpanel3->name = "child [2]";
            viewpanel3->resizerValue = 0.50f;
            viewpanel3->flexGlow = 1.0f;
            viewpanel3->flexShrink = 1.0f;
            //viewpanel3->minWidth = geometry::Length::Pixels(100);
            //viewpanel3->width = geometry::Length::Pixels(100);
            viewpanel4.emplace();
            viewpanel4->name = "child [3]";
            viewpanel4->resizerValue = 0.75f;
            viewpanel4->flexGlow = 0.0f;
            viewpanel4->flexShrink = 0.0f;
            //viewpanel4->width = geometry::Length::Pixels(100);
            //viewpanel4->maxWidth = geometry::Length::Pixels(150);
            viewpanel4->setBackgroundColor(vk::ClearColorValue{0.0f, 0.0f, 1.0f, 1.0f});
            viewpanel5.emplace(geometry::PanelAlignment::eRow);
            viewpanel5->name = "child [2]-[1]";
            viewpanel5->resizerValue = 0.5f;
            viewpanel5->setBackgroundColor(vk::ClearColorValue{0.0f, 1.0f, 0.0f, 1.0f});
            viewpanel6.emplace();
            viewpanel6->name = "child [2]-[2]";
            viewpanel6->resizerValue = 0.5f;
            viewpanel6->setBackgroundColor(vk::ClearColorValue{1.0f, 1.0f, 0.0f, 1.0f});
            viewpanel7.emplace();
            viewpanel7->name = "child [2]-[1]-[1]";
            viewpanel7->resizerValue = 0.3f;
            //viewpanel7->width = geometry::Length::Pixels(200);
            //viewpanel7->minWidth = geometry::Length::Pixels(100);
            viewpanel7->setBackgroundColor(vk::ClearColorValue{1.0f, 0.0f, 1.0f, 1.0f});
            viewpanel8.emplace();
            viewpanel8->name = "child [2]-[1]-[2]";
            //viewpanel8->width = geometry::Length::Pixels(100);
            viewpanel8->resizerValue = 0.6f;
            viewpanel8->setBackgroundColor(vk::ClearColorValue{0.5f, 0.5f, 0.5f, 1.0f});
            viewpanel9.emplace();
            viewpanel9->name = "child [2]-[1]-[3]";
            viewpanel9->resizerValue = 0.5f;
            //viewpanel9->minWidth = geometry::Length::Pixels(20);
            viewpanel9->setBackgroundColor(vk::ClearColorValue{0.5f, 0.7f, 0.5f, 1.0f});
            viewpanel10.emplace();
            viewpanel10->name = "child [4]";
            viewpanel10->resizerValue = 0.1f;
            viewpanel10->flexGlow = 1.0f;
            viewpanel10->flexShrink = 1.0f;
            //viewpanel10->width = geometry::Length::Pixels(100);
            //viewpanel10->minWidth = geometry::Length::Pixels(200);
            viewpanel10->setBackgroundColor(vk::ClearColorValue{0.5f, 0.5f, 0.7f, 1.0f});


            viewport->panel = &*viewpanel1;
            viewpanel1->resizerValue =0;
            viewpanel1->add(&*viewpanel2);
            viewpanel1->add(&*viewpanel3);
            viewpanel1->add(&*viewpanel4);

            viewpanel3->add(&*viewpanel6);
            viewpanel3->add(&*viewpanel5);
            viewpanel5->add(&*viewpanel7);
            viewpanel5->add(&*viewpanel8);
            viewpanel5->add(&*viewpanel9);
            viewpanel5->add(&*viewpanel10);



            int width = 0, height = 0;
            windowResource->getExtent(width, height);
            geometry::ResizingViewport(*viewport, width, height);
            geometry::BindEvents(*viewport,*inputResource, *standardCursorResource);

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
            gpuCreateInfo = CreateGraphicDeviceInfo();

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
            guiResource.emplace(gui::MakeGuiResource(gpu, *windowResource->swapchainResource, shaderCode));

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


        void recordCommandBuffer(const vk::raii::CommandBuffer& cmb) const {
            std::array<vk::ClearValue, 2> clearValues{};
            clearValues[0].color        = vk::ClearColorValue{0.2f, 0.2f, 0.2f,1.0f};
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

            vk::ClearColorValue c4 = viewport->hoveredPanel == &viewpanel4.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel4->clearColor;
            vk::ClearColorValue c2 = viewport->hoveredPanel == &viewpanel2.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel2->clearColor;
            vk::ClearColorValue c6 = viewport->hoveredPanel == &viewpanel6.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel6->clearColor;
            vk::ClearColorValue c7 = viewport->hoveredPanel == &viewpanel7.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel7->clearColor;
            vk::ClearColorValue c8 = viewport->hoveredPanel == &viewpanel8.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel8->clearColor;
            vk::ClearColorValue c9 = viewport->hoveredPanel == &viewpanel9.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel9->clearColor;
            vk::ClearColorValue c10 = viewport->hoveredPanel == &viewpanel10.value()? vk::ClearColorValue{0.8f, 0.8f, 0.8f, 1.0f}: viewpanel10->clearColor;
            render::RenderArea(cmb, *windowResource, viewpanel2->rect, c2);
            render::RenderArea(cmb, *windowResource, viewpanel4->rect, c4);
            render::RenderArea(cmb, *windowResource, viewpanel5->rect, vk::ClearColorValue{1.0f, 1.0f, 1.0f, 1.0f});
            render::RenderArea(cmb, *windowResource, viewpanel6->rect, c6);
             render::RenderArea(cmb, *windowResource, viewpanel7->rect, c7);
             render::RenderArea(cmb, *windowResource, viewpanel8->rect, c8);
             render::RenderArea(cmb, *windowResource, viewpanel9->rect, c9);
             render::RenderArea(cmb, *windowResource, viewpanel10->rect, c10);


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
                geometry::ResizingViewport(*app->viewport, width, height);

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
                        if (viewport && inputResource) {
                            geometry::UnbindEvents(*viewport,*inputResource);
                        }
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
            geometry::ResizingViewport(*app->viewport, width, height);
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
        std::optional<gui::GUIResource>                 guiResource{};
        std::optional<ResourceManager>                  resourceManager{};
        std::optional<geometry::Viewport>               viewport{};
        std::optional<geometry::Viewpanel>              viewpanel1{};
        std::optional<geometry::Viewpanel>              viewpanel2{};
        std::optional<geometry::Viewpanel>              viewpanel3{};
        std::optional<geometry::Viewpanel>              viewpanel4{};
        std::optional<geometry::Viewpanel>              viewpanel5{};
        std::optional<geometry::Viewpanel>              viewpanel6{};
        std::optional<geometry::Viewpanel>              viewpanel7{};
        std::optional<geometry::Viewpanel>              viewpanel8{};
        std::optional<geometry::Viewpanel>              viewpanel9{};
        std::optional<geometry::Viewpanel>              viewpanel10{};


        bool framebufferResized = false;
        bool pauseRendering = false;
    };
}