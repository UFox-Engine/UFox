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
import ufox_input;

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
            float ratio = glm::clamp(child->scaleValue, 0.0f, 1.0f);
            uint32_t sizeValue;

            if (viewpanel.isRow()) {
                sizeValue = i < lastChildIndex ?
                    static_cast<uint32_t>(static_cast<float>(width) * ratio) :
                    width;
                BuildPanelLayout(*child, x, y, sizeValue - x + posX, height);
                x = static_cast<int32_t>(sizeValue) + posX;
                child->scalerZone = i < lastChildIndex ?
                    vk::Rect2D{vk::Offset2D{x - RESIZER_OFFSET, y}, vk::Extent2D{RESIZER_THICKNESS, height}} :
                    vk::Rect2D{{0,0}, {0,0}};
            } else {
                sizeValue = i < lastChildIndex ?
                    static_cast<uint32_t>(static_cast<float>(height) * ratio) :
                    height;
                BuildPanelLayout(*child, x, y, width, sizeValue - y + posY);
                y = static_cast<int32_t>(sizeValue) + posY;
                child->scalerZone = i < lastChildIndex ?
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

    void PollInputForPanel(Viewport& viewport, Viewpanel& viewpanel, input::InputResource& input, input::StandardCursorResource& cursor, const size_t childIndex)
    {
        if (!viewpanel.parent) return;

        const auto mx = input.mousePosition.x;
        const auto my = input.mousePosition.y;

        if (viewport.splitterContext.isActive) {
            return;
        }

        const bool overSplitter =
            mx > viewpanel.scalerZone.offset.x &&
            my > viewpanel.scalerZone.offset.y &&
            mx < viewpanel.scalerZone.offset.x + static_cast<int32_t>(viewpanel.scalerZone.extent.width) &&
            my < viewpanel.scalerZone.offset.y + static_cast<int32_t>(viewpanel.scalerZone.extent.height);

        const bool isHovered =
            mx >= viewpanel.rect.offset.x &&
            my >= viewpanel.rect.offset.y &&
            mx <  viewpanel.rect.offset.x + static_cast<int32_t>(viewpanel.rect.extent.width) &&
            my <  viewpanel.rect.offset.y + static_cast<int32_t>(viewpanel.rect.extent.height);

        if (overSplitter)
        {
            if (viewport.splitterContext.targetPanel != &viewpanel) {
                viewport.splitterContext.targetPanel = &viewpanel;
#ifdef USE_SDL
                input::SetStandardCursor(cursor, viewpanel.parent->isColumn() ?input::CursorType::eNSResize :input::CursorType::eEWResize);
#else
                input::SetStandardCursor(cursor, viewpanel.parent->isColumn() ?input::CursorType::eNSResize :input::CursorType::eEWResize, viewport.window);
#endif

                viewport.splitterContext.index = childIndex;
            }
        }
        else if (viewport.splitterContext.targetPanel == &viewpanel)
        {
#ifdef USE_SDL
            input::SetStandardCursor(cursor, input::CursorType::eDefault);
#else
            input::SetStandardCursor(cursor, input::CursorType::eDefault, viewport.window);
#endif
            viewport.splitterContext.targetPanel = nullptr;
            viewport.splitterContext.index = 0;
        }

        if (isHovered) {
            if (viewport.hoveredPanel != &viewpanel)
                viewport.hoveredPanel = &viewpanel;
        }
        else if (viewport.hoveredPanel == &viewpanel) {
            viewport.hoveredPanel = nullptr;
        }
    }

    void BindEvents(Viewport& viewport, input::InputResource& input, input::StandardCursorResource& cursor) {
        if (!viewport.panel) return;

        viewport.mouseMoveEventHandle.emplace(input.onMouseMoveCallbackPool.bind([&viewport, &cursor](input::InputResource& i) {
            auto allPanels = viewport.panel->GetAllPanels();
            for (size_t idx = 0; idx < allPanels.size(); ++idx) {
                    Viewpanel* panel = allPanels[idx];
                    if (panel->pickingMode == PickingMode::ePosition) {
                    PollInputForPanel(viewport, *panel, i, cursor, idx);
                }
            }
        }));

        viewport.leftClickEventHandle.emplace(input.onLeftMouseButtonCallbackPool.bind([&viewport](input::InputResource& inputResource){
                if (inputResource.leftMouseButtonAction.phase == input::ActionPhase::eStart && viewport.splitterContext.targetPanel) {
                const Viewpanel* target = viewport.splitterContext.targetPanel;
                const Viewpanel* parent = target->parent;
                if (!parent) return;

                const size_t splitterIndex = viewport.splitterContext.index;

                const bool isRow = parent->isRow();
                uint32_t rawMinValue = 0;
                uint32_t rawMaxValue = 0;

                for (size_t i = 0; i < parent->getChildrenSize(); ++i) {
                    auto [minW, minH] = parent->getChild(i)->getMinSize();
                    const uint32_t childMin = isRow ? minW : minH;
                    if (i < splitterIndex) rawMinValue += childMin;
                    else if (i > splitterIndex) rawMaxValue += childMin;
                }

                const uint32_t parentTotal = isRow ? parent->rect.extent.width : parent->rect.extent.height;
                if (parentTotal == 0) return;

                viewport.splitterContext.minScale = static_cast<float>(rawMinValue) / static_cast<float>(parentTotal);
                viewport.splitterContext.maxScale = 1.0f - static_cast<float>(rawMaxValue) / static_cast<float>(parentTotal);

                viewport.splitterContext.minScale = std::max(0.0f, viewport.splitterContext.minScale);
                viewport.splitterContext.maxScale = std::min(1.0f, viewport.splitterContext.maxScale);

                if (viewport.splitterContext.minScale > viewport.splitterContext.maxScale) {
                    viewport.splitterContext.minScale = viewport.splitterContext.maxScale = 0.5f;
        }

        viewport.splitterContext.startScale = target->scaleValue;
        viewport.splitterContext.isActive = true;
            }else if (inputResource.leftMouseButtonAction.phase == input::ActionPhase::eEnd) {
                viewport.splitterContext.isActive = false;
                viewport.splitterContext.targetPanel = nullptr;
                viewport.splitterContext.index = 0;
            }
        }));
    }

    void UnbindEvents(Viewport& viewport, input::InputResource& input)
    {
        if (!viewport.mouseMoveEventHandle.has_value()) return;
        input.onMouseMoveCallbackPool.unbind(viewport.mouseMoveEventHandle->index);
        viewport.mouseMoveEventHandle.reset();
        if (!viewport.leftClickEventHandle.has_value()) return;
        input.onLeftMouseButtonCallbackPool.unbind(viewport.leftClickEventHandle->index);
        viewport.leftClickEventHandle.reset();
    }


}
