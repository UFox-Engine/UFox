module;
#include <vulkan/vulkan_raii.hpp>
#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif
#include <vector>
#include <optional>
#include <set>
#include <glm/glm.hpp>


export module ufox_renderer;

import ufox_lib;  // GPUResources, SwapchainResource, FrameResource

export namespace ufox::renderer {
    struct Viewport;
    struct Viewpanel;

    enum class PanelAlignment { eRow, eColumn };
    enum class PickingMode    { ePosition, eIgnore };
    enum class ScalingMode    { eFlex, eFixed };

    constexpr uint32_t RESIZER_THICKNESS = 12;
    constexpr int32_t RESIZER_OFFSET = 6;




    struct Viewpanel {
        enum class AlignDirection {
            Column,
            Row
        };

        Viewpanel(Viewport& viewport_, input::InputResource& input_,
                  PanelAlignment align = PanelAlignment::eColumn,PickingMode picking = PickingMode::ePosition,ScalingMode scaling = ScalingMode::eFlex)
            : alignment(align), pickingMode(picking), scalingMode(scaling), viewport(viewport_), input(input_){
            if (pickingMode == PickingMode::ePosition)
                bindMouseEvents();
        }

        ~Viewpanel() {
            if (handleMouseMove.has_value()) {
                input.onMouseMoveCallbackPool.unbind(handleMouseMove->index);
                handleMouseMove.reset();
            }
        }

        vk::Rect2D                          rect{{0,0},{100,100}};
        vk::Rect2D                          scalingZone{{0,0},{0,0}};
        uint32_t                            minWidth{0};
        uint32_t                            minHeight{0};
        Viewpanel*                          parent{nullptr};


        PanelAlignment                      alignment{PanelAlignment::eColumn};
        PickingMode                         pickingMode{PickingMode::ePosition};
        ScalingMode                         scalingMode{ScalingMode::eFlex};

        vk::ClearColorValue                 clearColor{0.5f, 0.5f, 0.5f, 1.0f};
        vk::ClearColorValue                 clearColor2{0.8f, 0.8f, 0.8f, 1.0f};

        float                               scaler{0};

        void add(Viewpanel* child) {
            if (!child || child->parent == this) return;
            if (child->parent)
                child->parent->remove(child);

            child->parent = this;
            children.push_back(child);
            // NO input meddling — child's picking mode is its own business
        }

        void remove(Viewpanel* child) {
            if (!child) return;
            auto it = std::find(children.begin(), children.end(), child);
            if (it == children.end()) return;

            child->parent = nullptr;
            children.erase(it);
            // NO unbind — child keeps its input state
        }

        void clear() {
            for (auto* child : children)
                child->parent = nullptr;
            children.clear();
            // NO input cleanup — children manage themselves
        }

        void setPickingMode(PickingMode m) noexcept {
            if (m == pickingMode) return;
            pickingMode = m;
            if (m == PickingMode::ePosition) bindMouseEvents();
            else unbindMouseEvents();
        }

        void setAlignment(PanelAlignment a) noexcept { alignment = a; }
        void setScalingMode(ScalingMode s) noexcept   { scalingMode = s; }

        [[nodiscard]] bool isRow()                              const noexcept { return alignment   == PanelAlignment::eRow; }
        [[nodiscard]] bool isColumn()                           const noexcept { return alignment   == PanelAlignment::eColumn; }
        [[nodiscard]] bool isPickingIgnored()                   const noexcept { return pickingMode == PickingMode::eIgnore; }
        [[nodiscard]] bool isFlexScaled()                       const noexcept { return scalingMode == ScalingMode::eFlex; }
        [[nodiscard]] bool isFixedScaled()                      const noexcept { return scalingMode == ScalingMode::eFixed; }
        [[nodiscard]] bool isChildrenEmpty()                    const noexcept { return children.empty(); }
        [[nodiscard]] size_t getChildrenSize()                  const noexcept { return children.size(); }
        [[nodiscard]] size_t getLastChildIndex()                const noexcept { return children.size() - 1; }
        [[nodiscard]] Viewpanel* getChild(const size_t index)   const noexcept { if (index < 0 || index >= static_cast<int>(children.size())) return nullptr;return children[index];}


        [[nodiscard]] std::pair<uint32_t, uint32_t> getTotalMinRect() const {
            uint32_t totalMinWidth = minWidth;
            uint32_t totalMinHeight = minHeight;

            for (const auto& child : children) {
                const auto [childMinWidth, childMinHeight] = child->getTotalMinRect();
                if (isRow()) {
                    totalMinWidth += childMinWidth;
                    totalMinHeight = std::max(totalMinHeight, childMinHeight);
                }
                else {
                    totalMinWidth = std::max(totalMinWidth, childMinWidth);
                    totalMinHeight += childMinHeight;
                }
            }

            return std::make_pair(totalMinWidth, totalMinHeight);
        }

        void SetBackgroundColor(const vk::ClearColorValue& color) {
            clearColor = color;
        }



        void onProcessingRendering(const vk::raii::CommandBuffer& cmb, const windowing::WindowResource& window) const {

            if (rect.extent.width < 1 || rect.extent.height < 1) return;

            vk::RenderingAttachmentInfo colorAttachment{};
            colorAttachment.setImageView(window.swapchainResource->getCurrentImageView())
                           .setImageLayout(vk::ImageLayout::eColorAttachmentOptimal)
                           .setLoadOp(vk::AttachmentLoadOp::eClear)
                           .setStoreOp(vk::AttachmentStoreOp::eStore)
                           .setClearValue(isHovered ? clearColor2 : clearColor);

            vk::RenderingInfo renderingInfo{};
            renderingInfo.setRenderArea(rect)
                         .setLayerCount(1)
                         .setColorAttachmentCount(1)
                         .setPColorAttachments(&colorAttachment);
            cmb.beginRendering(renderingInfo);





            cmb.endRendering();
        }




    private:
        Viewport&                                           viewport;
        input::InputResource&                               input;

        bool                                                isHovered{false};
        bool                                                isOverResizer{false};

        vk::Offset2D                                        clickOffset{0,0};
        std::vector<Viewpanel*>                             children;
        std::optional<input::EventCallbackPool::Handler>    handleMouseMove;

        void bindMouseEvents() {
            if (handleMouseMove.has_value()) return;
            handleMouseMove.emplace(input.onMouseMoveCallbackPool.bind([this](input::InputResource& i) {
                onMouseMove(i);
            }));
        }

        void unbindMouseEvents() {
            if (!handleMouseMove.has_value()) return;
            input.onMouseMoveCallbackPool.unbind(handleMouseMove->index);
            handleMouseMove.reset();
        }

        void onMouseMove(input::InputResource& input) {
            const auto mx = input.mousePosition.x;
            const auto my = input.mousePosition.y;

            // Panel hover
            isHovered = mx >= rect.offset.x &&
                        my >= rect.offset.y &&
                        mx <  rect.offset.x + static_cast<int32_t>(rect.extent.width) &&
                        my <  rect.offset.y + static_cast<int32_t>(rect.extent.height);

            // ROOT PANEL → NO RESIZER → NO CURSOR → EXIT
            if (parent == nullptr) return;



            if (mx > scalingZone.offset.x &&
                my > scalingZone.offset.y &&
                mx <  scalingZone.offset.x + static_cast<int32_t>(scalingZone.extent.width) &&
                my <  scalingZone.offset.y + static_cast<int32_t>(scalingZone.extent.height)) {

                isOverResizer = true;


                if (parent->isColumn()) {
                    input.setCursor(input::CursorType::eNSResize);
                }
                else {
                    input.setCursor(input::CursorType::eEWResize);
                }
            }
            else {
                if (isOverResizer) {
                    input.setCursor(input::CursorType::eDefault);
                }
                isOverResizer = false;
            }


        }



    };



    void BuildPanelLayout(Viewpanel& viewpanel,const int32_t& posX, const int32_t& posY, const uint32_t& width, const uint32_t& height) {
        // Update current panel dimensions
        viewpanel.rect.offset.x = posX;
        viewpanel.rect.offset.y = posY;
        viewpanel.rect.extent.width = width;
        viewpanel.rect.extent.height = height;

        if (viewpanel.isChildrenEmpty()) return;

        int32_t x = posX;
        int32_t y = posY;

        const size_t lastChildIndex = viewpanel.getLastChildIndex();

        for (size_t i = 0; i < viewpanel.getChildrenSize(); ++i) {
            Viewpanel* child = viewpanel.getChild(i);
            float ratio = glm::clamp(child->scaler, 0.0f, 1.0f);
            uint32_t sizeValue;

            if (viewpanel.isRow()) {
                sizeValue = i < lastChildIndex ?
                    static_cast<uint32_t>(static_cast<float>(width) * ratio) :
                    width;
                BuildPanelLayout(*child, x, y, sizeValue - x + posX, height);
                x = static_cast<int32_t>(sizeValue) + posX;
                child->scalingZone = i < lastChildIndex ?
                    vk::Rect2D{vk::Offset2D{x - RESIZER_OFFSET, y}, vk::Extent2D{RESIZER_THICKNESS, height}} :
                    vk::Rect2D{{0,0}, {0,0}};
            } else {
                sizeValue = i < lastChildIndex ?
                    static_cast<uint32_t>(static_cast<float>(height) * ratio) :
                    height;
                BuildPanelLayout(*child, x, y, width, sizeValue - y + posY);
                y = static_cast<int32_t>(sizeValue) + posY;
                child->scalingZone = i < lastChildIndex ?
                    vk::Rect2D{vk::Offset2D{x, y - RESIZER_OFFSET}, vk::Extent2D{width, RESIZER_THICKNESS}} :
                    vk::Rect2D{{0,0}, {0,0}};
            }
        }
    }


    struct Viewport {
        explicit Viewport(const windowing::WindowResource& window_): window(window_) {}
        ~Viewport() = default;

        vk::Extent2D        extent{};
        Viewpanel*          panel = nullptr;
        Viewpanel*          hoveredPanel = nullptr;
        Viewpanel*          resizerPanel = nullptr;

        void onResize(const int width, const int height) const {

            BuildPanelLayout(*panel, 0, 0, width, height);
        }

        void SetWindowMinSize() const {
            auto size = panel->getTotalMinRect();

#ifdef USE_SDL
            SDL_SetWindowMinimumSize(window.getHandle(), static_cast<int>(size.first), static_cast<int>(size.second));
#else

#endif
        }

    private:
        const windowing::WindowResource&    window;
    };

}
