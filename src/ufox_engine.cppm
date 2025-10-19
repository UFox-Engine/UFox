//
// Created by Puwiwad on 05.10.2025.
//
module;
#include <iostream>
#include <vulkan/vulkan_raii.hpp>
#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif
export module ufox_engine;
import ufox_lib;
import ufox_windowing;
import ufox_graphic;

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

    class UFoxWindow {
    public:
        UFoxWindow(const GPUResources& gpu_,std::string const& windowName, vk::Extent2D const& extent_)
            : gpu(gpu_), windowResource{windowing::CreateWindow(windowName, extent_)} {
            surfaceResource.emplace(*gpu.instance, *windowResource);
#ifdef USE_SDL
            SDL_AddEventWatch(FramebufferResizeCallbackSDL, this);
#else
            glfwSetWindowUserPointer(windowResource->getHandle(), this);
            glfwSetFramebufferSizeCallback(windowResource->getHandle(), FramebufferResizeCallbackGLFW);
            glfwSetWindowIconifyCallback(windowResource->getHandle(), WindowIconifyCallbackGLFW);
#endif
        }
        // Resources should be destroyed in reverse order of creation
        ~UFoxWindow() {
            enableDraw = false;
            if (swapchainResource) {
                swapchainResource->Clear();
            }
#ifdef USE_SDL
            SDL_RemoveEventWatch(FramebufferResizeCallbackSDL, this);
#endif

        }
        void InitResources() {
            swapchainResource.emplace(gpu::vulkan::MakeSwapchainResource(gpu, *surfaceResource, vk::ImageUsageFlagBits::eColorAttachment));
            frameResource.emplace(gpu::vulkan::MakeFrameResource(gpu, vk::FenceCreateFlagBits::eSignaled));

        }

        void Draw() {
#ifdef USE_SDL
            SDL_Event event;
            bool running = true;
            while (running) {
                while (SDL_PollEvent(&event)) {
                    switch (event.type) {
                        case SDL_EVENT_QUIT: {
                            enableDraw = false;
                            running = false;
                            break;
                        }

                        case SDL_EVENT_WINDOW_MINIMIZED: {
                            enableDraw = false;
                            break;
                        }
                        case SDL_EVENT_WINDOW_RESTORED: {
                            enableDraw = true;
                            break;
                        }
                        default: {}
                    }

                }
                DrawFrame();
            }
#else
            while (!glfwWindowShouldClose(windowResource->getHandle())) {
                DrawFrame();
                glfwPollEvents();
            }
#endif
        }

        [[nodiscard]] gpu::vulkan::QueueFamilyIndices GetQueueFamilyIndices() const {
            return gpu::vulkan::FindGraphicsAndPresentQueueFamilyIndex(gpu, *surfaceResource);
        }

    private:
        void RecreateSwapchain() {
            // Handle window minimization
            int width = 0, height = 0;
#ifdef USE_SDL
            SDL_GetWindowSize(windowResource->getHandle(), &width, &height);
#else
            glfwGetFramebufferSize(windowResource->getHandle(), &width, &height);

#endif
            enableDraw = width != 0 && height != 0;

            if (!enableDraw) return;

            gpu.device->waitIdle();
            surfaceResource->extent = vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            swapchainResource->Clear();
            gpu::vulkan::ReMakeSwapchainResource(*swapchainResource, gpu, *surfaceResource, vk::ImageUsageFlagBits::eColorAttachment);
        }

        void RecreateSwapchain(int width, int height) {

            enableDraw = width != 0 && height != 0;

            if (!enableDraw) return;

            gpu.device->waitIdle();
            surfaceResource->extent = vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
            swapchainResource->Clear();
            gpu::vulkan::ReMakeSwapchainResource(*swapchainResource, gpu, *surfaceResource, vk::ImageUsageFlagBits::eColorAttachment);
        }

        void DrawFrame() {
            while ( vk::Result::eTimeout == gpu.device->waitForFences( *frameResource->getCurrentDrawFence(), vk::True, UINT64_MAX ) )
                ;

            if (!enableDraw) return;

            auto [result, imageIndex] = swapchainResource->swapChain->acquireNextImage(UINT64_MAX, frameResource->getCurrentPresentCompleteSemaphore(), nullptr);
            swapchainResource->currentImageIndex = imageIndex;
            gpu.device->resetFences(*frameResource->getCurrentDrawFence());
            if (result == vk::Result::eErrorOutOfDateKHR) {
                ufox::debug::log(debug::LogLevel::eInfo, "Recreating swapchain due to acquireNextImage failure");
                RecreateSwapchain();
                return;
            }
            if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
                throw std::runtime_error("Failed to acquire swapchain image");
            }
            const vk::raii::CommandBuffer& cmb = frameResource->getCurrentCommandBuffer();
            cmb.reset();
            RecordCommandBuffer(cmb);


            vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
            vk::SubmitInfo submitInfo{};
            submitInfo.setWaitSemaphoreCount(1)
                      .setPWaitSemaphores(&*frameResource->getCurrentPresentCompleteSemaphore())
                      .setPWaitDstStageMask(&waitDestinationStageMask)
                      .setCommandBufferCount(1)
                      .setPCommandBuffers(&*cmb)
                      .setSignalSemaphoreCount(1)
                      .setPSignalSemaphores(&*swapchainResource->getCurrentRenderFinishedSemaphore());
            gpu.graphicsQueue->submit(submitInfo, frameResource->getCurrentDrawFence());
            vk::PresentInfoKHR presentInfo{};
            presentInfo.setWaitSemaphoreCount(1)
                       .setPWaitSemaphores(&*swapchainResource->getCurrentRenderFinishedSemaphore())
                       .setSwapchainCount(1)
                       .setPSwapchains(&**swapchainResource->swapChain)
                       .setPImageIndices(&swapchainResource->currentImageIndex);

            result = gpu.presentQueue->presentKHR(presentInfo);
            if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized) {
                framebufferResized = false;
                RecreateSwapchain();
                return;
            }
            if (result != vk::Result::eSuccess) {
                throw std::runtime_error("Failed to present swapchain image");
            }
            frameResource->ContinueNextFrame();
        }


        void RecordCommandBuffer(const vk::raii::CommandBuffer& cmb) const {
            std::array<vk::ClearValue, 2> clearValues{};
            clearValues[0].color        = vk::ClearColorValue{0.2f, 0.2f, 0.2f,1.0f};
            clearValues[1].depthStencil = vk::ClearDepthStencilValue{0.0f, 0};
            cmb.begin({});
            vk::ImageSubresourceRange range{};
            range.aspectMask     = vk::ImageAspectFlagBits::eColor;
            range.baseMipLevel   = 0;
            range.levelCount     = VK_REMAINING_MIP_LEVELS;
            range.baseArrayLayer = 0;
            range.layerCount     = VK_REMAINING_ARRAY_LAYERS;
            gpu::vulkan::TransitionImageLayout(
                cmb, swapchainResource->getCurrentImage(),
                vk::PipelineStageFlagBits2::eTopOfPipe,
                vk::PipelineStageFlagBits2::eColorAttachmentOutput ,
                {},
                vk::AccessFlagBits2::eColorAttachmentWrite,
                vk::ImageLayout::eUndefined,
                vk::ImageLayout::eColorAttachmentOptimal,range
                );
            vk::RenderingAttachmentInfo colorAttachment{};
            colorAttachment.setImageView(swapchainResource->getCurrentImageView())
                           .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                           .setLoadOp(vk::AttachmentLoadOp::eClear)
                           .setStoreOp(vk::AttachmentStoreOp::eStore)
                           .setClearValue(clearValues[0]);
            vk::Rect2D renderArea{};
            renderArea.offset = vk::Offset2D{0, 0};
            renderArea.extent = swapchainResource->extent;
            vk::RenderingInfo renderingInfo{};
            renderingInfo.setRenderArea(renderArea)
                         .setLayerCount(1)
                         .setColorAttachmentCount(1)
                         .setPColorAttachments(&colorAttachment);
            cmb.beginRendering(renderingInfo);

            cmb.endRendering();
            gpu::vulkan::TransitionImageLayout(cmb, swapchainResource->getCurrentImage(),
                vk::ImageLayout::eColorAttachmentOptimal, vk::ImageLayout::ePresentSrcKHR, range);
            cmb.end();
        }

#ifdef USE_SDL
        static bool FramebufferResizeCallbackSDL(void* userData, SDL_Event* event) {
            if (event->type == SDL_EVENT_WINDOW_RESIZED) {
                SDL_Window* win = SDL_GetWindowFromID(event->window.windowID);
                auto* app = static_cast<UFoxWindow*>(userData);
                int width, height;
                SDL_GetWindowSize(win, &width, &height);
                app->framebufferResized = true;
                app->RecreateSwapchain(width, height);
                app->DrawFrame();
            }
            return true;
        }
#else
        static void FramebufferResizeCallbackGLFW(GLFWwindow* window, int width, int height) {
            auto app = static_cast<UFoxWindow*>(glfwGetWindowUserPointer(window));
            app->framebufferResized = true;
            app->RecreateSwapchain(width, height);
            app->DrawFrame();
        }

        static void WindowIconifyCallbackGLFW(GLFWwindow* window, int iconified) {
            auto app = static_cast<UFoxWindow*>(glfwGetWindowUserPointer(window));
            app->enableDraw = (iconified == GLFW_FALSE);
        }
#endif

        const GPUResources&                             gpu;
        std::optional<windowing::WindowResource>        windowResource;
        std::optional<gpu::vulkan::SurfaceResource>     surfaceResource{};
        std::optional<gpu::vulkan::SwapchainResource>   swapchainResource{};
        std::optional<gpu::vulkan::FrameResource>       frameResource{};
        bool framebufferResized = false;
        bool enableDraw = true;
    };



class Engine {
public:
    Engine() = default;

    ~Engine() {
        // Wait for all GPU operations to complete before destroying resources
        if (gpuResources.device) {
            gpuResources.device->waitIdle();
        }
    }

    void Init() {
        InitializeGPU();
    }

    void Run() {
        window->Draw();
    }

private:
    void InitializeGPU() {
        gpuCreateInfo = CreateGraphicDeviceInfo();

        gpuResources.context.emplace();
        gpuResources.instance.emplace(gpu::vulkan::MakeInstance(gpuResources, gpuCreateInfo));
        gpuResources.physicalDevice.emplace(gpu::vulkan::PickBestPhysicalDevice(gpuResources));

        window.emplace(gpuResources, "UFox", vk::Extent2D{800, 800});

        gpuResources.queueFamilyIndices.emplace(window->GetQueueFamilyIndices());
        gpuResources.device.emplace(gpu::vulkan::MakeDevice(gpuResources, gpuCreateInfo));
        gpuResources.commandPool.emplace(gpu::vulkan::MakeCommandPool(gpuResources));

        gpuResources.graphicsQueue.emplace(gpu::vulkan::MakeGraphicsQueue(gpuResources));
        gpuResources.presentQueue.emplace(gpu::vulkan::MakePresentQueue(gpuResources));

        window->InitResources();
        ufox::debug::log(debug::LogLevel::eInfo, "Initialize success");
    }

    gpu::vulkan::GraphicDeviceCreateInfo gpuCreateInfo{};
    GPUResources gpuResources{};
    std::optional<UFoxWindow> window{};
};

}