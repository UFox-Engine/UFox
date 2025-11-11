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

export module ufox_geometry;

import ufox_lib;  // GPUResources, SwapchainResource, FrameResource

export namespace ufox::geometry {

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

    void ResizingViewport(Viewport& viewport, int width, int height)
    {
        viewport.extent.width  = static_cast<uint32_t>(width);
        viewport.extent.height = static_cast<uint32_t>(height);

        if (viewport.panel)
        {
            BuildPanelLayout(*viewport.panel, 0, 0, viewport.extent.width, viewport.extent.height);
        }
    }

    void PollInputForPanel(
        Viewport& viewport,
        Viewpanel& viewpanel,
        input::InputResource& input)
    {
        if (viewpanel.pickingMode == PickingMode::eIgnore)
            return;

        const auto mx = input.mousePosition.x;
        const auto my = input.mousePosition.y;

        const bool isHovered =
            mx >= viewpanel.rect.offset.x &&
            my >= viewpanel.rect.offset.y &&
            mx <  viewpanel.rect.offset.x + static_cast<int32_t>(viewpanel.rect.extent.width) &&
            my <  viewpanel.rect.offset.y + static_cast<int32_t>(viewpanel.rect.extent.height);


        if (isHovered) {
            if (viewport.hoveredPanel != &viewpanel)
                viewport.hoveredPanel = &viewpanel;
        }
        else if (viewport.hoveredPanel == &viewpanel) {
            viewport.hoveredPanel = nullptr;
        }

        if (!viewpanel.parent) return;

        const bool overResizer =
            mx > viewpanel.scalingZone.offset.x &&
            my > viewpanel.scalingZone.offset.y &&
            mx < viewpanel.scalingZone.offset.x + static_cast<int32_t>(viewpanel.scalingZone.extent.width) &&
            my < viewpanel.scalingZone.offset.y + static_cast<int32_t>(viewpanel.scalingZone.extent.height);

        if (overResizer)
        {
            if (viewport.scaler != &viewpanel) {
                viewport.scaler = &viewpanel;
                input.setCursor(viewpanel.parent->isColumn() ?
                input::CursorType::eNSResize :
                input::CursorType::eEWResize);
            }
        }
        else if (viewport.scaler == &viewpanel)
        {
            input.setCursor(input::CursorType::eDefault);
            viewport.scaler = nullptr;
        }
    }

    void BindEvents(Viewport& viewport, input::InputResource& input)
    {
        if (!viewport.panel) return;

        viewport.handleMouseMove.emplace(input.onMouseMoveCallbackPool.bind([&viewport](input::InputResource& i) {

                            auto allPanels = viewport.panel->GetAllPanels();

                            for (Viewpanel* panel : allPanels){
                                if (panel->pickingMode == PickingMode::ePosition){
                                    PollInputForPanel(viewport, *panel, i);
                                }
                            }
        }));
    }

    void UnbindEvents(Viewport& viewport, input::InputResource& input)
    {
        if (!viewport.handleMouseMove.has_value()) return;
        input.onMouseMoveCallbackPool.unbind(viewport.handleMouseMove->index);
        viewport.handleMouseMove.reset();
    }


}
