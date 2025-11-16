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

    [[nodiscard]] int GetPanelMinExtent1D(const bool& row, const Viewpanel& panel) noexcept {
        auto [w, h] = panel.getTotalMinExtent();
        return static_cast<int>(row? w:h);
    }

    [[nodiscard]] int GetPanelPosition1D(const bool& row, const Viewpanel& panel) noexcept {
        return row ? panel.getPositionX() : panel.getPositionY();
    }

    [[nodiscard]] int GetResizerPushLeftThreshold(const bool& row, const Viewpanel& panel, const int& minExtent) noexcept {
        return GetPanelPosition1D(row, panel) + minExtent;
    }

    [[nodiscard]] int GetResizerPushRightThreshold(const bool& row, const Viewpanel& panel ) noexcept {
        return panel.getExtentToViewportSpace(row) - GetPanelMinExtent1D(row, panel);
    }

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
            float ratio = glm::clamp(child->resizerValue, 0.0f, 1.0f);
            uint32_t sizeValue;

            if (viewpanel.isRow()) {
                sizeValue = i < lastChildIndex ?
                    static_cast<uint32_t>(static_cast<float>(width) * ratio) :
                    width;
                BuildPanelLayout(*child, x, y, sizeValue - x + posX, height);
                x = static_cast<int32_t>(sizeValue) + posX;
                child->resizerZone = i < lastChildIndex ?
                    vk::Rect2D{vk::Offset2D{x - RESIZER_OFFSET, y}, vk::Extent2D{RESIZER_THICKNESS, height}} :
                    vk::Rect2D{{0,0}, {0,0}};
            } else {
                sizeValue = i < lastChildIndex ?
                    static_cast<uint32_t>(static_cast<float>(height) * ratio) :
                    height;
                BuildPanelLayout(*child, x, y, width, sizeValue - y + posY);
                y = static_cast<int32_t>(sizeValue) + posY;
                child->resizerZone = i < lastChildIndex ?
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

    void EnabledViewportResizer(Viewport& viewport, const input::InputResource& input, const bool& state){
        if (state) {
            SplitterContext& ctx            = viewport.splitterContext;

            if (!ctx.targetPanel)           return;
            Viewpanel* target               = ctx.targetPanel;

            if (!target->parent)            return;

            Viewpanel* parent               = target->parent;
            const int parentPos             = parent->getAlignmentOffset();
            const int parentExtent          = parent->getAlignmentExtent();
            const size_t panelsCount        = parent->getChildrenSize();
            const size_t index              = utilities::Index_Of(std::span{parent->children}, target).value_or(static_cast<size_t>(-1));
            const bool isRow                = parent->isRow();
            const int mousePos              = isRow? input.mousePosition.x : input.mousePosition.y;
            const int zonePos               = (isRow? target->resizerZone.offset.x  : target->resizerZone.offset.y) + RESIZER_OFFSET;
            const int clickOffset           = mousePos - zonePos;

            ctx.index                       = index;
            ctx.isRow                       = isRow;
            ctx.panelsCount                 = panelsCount;
            ctx.pushIndex                   = index;
            ctx.parentExtent                = parentExtent;
            ctx.min                         = parentPos;
            ctx.max                         = parentPos + parentExtent;
            ctx.clickOffset                 = clickOffset;
            ctx.currentValue                = std::clamp(mousePos - clickOffset, ctx.min, ctx.max);
            ctx.isActive                    = true;
        }
        else {
            viewport.splitterContext.isActive = false;
        }
    }

    void DisableViewportResizer(Viewport& viewport) {
        viewport.splitterContext.isActive = false;
    }

    void HandleTargetPanelResizerPush(SplitterContext& ctx);
    void HandlePanelResizerRightPush(SplitterContext& ctx);
    void HandlePanelResizerLeftPush(SplitterContext& ctx);

    void TranslatePanelResizer(SplitterContext& ctx) {
        if (ctx.pushIndex >= ctx.panelsCount) {
            ctx.isActive = false;
            return;
        }

        if (ctx.pushIndex == ctx.index) {
            HandleTargetPanelResizerPush(ctx);
        } else if (ctx.pushIndex < ctx.index) {
            HandlePanelResizerLeftPush(ctx);
        } else if (ctx.pushIndex > ctx.index) {
            HandlePanelResizerRightPush(ctx);
        }
    }

    void UpdatePanelResizerValue(Viewpanel& panel, const int& value, const int& valueOffset, const int& minValue, const int& parentExtent) {
        panel.resizerValue = static_cast<float>(value - valueOffset - minValue) / static_cast<float>(parentExtent);
    }

    void ResetViewResizerPushContext(SplitterContext& ctx) {
        ctx.valueOffset = 0;
        ctx.pushIndex = ctx.index;
    }

    void HandleTargetPanelResizerPush(SplitterContext& ctx) {
        const int minLeftValue = GetPanelMinExtent1D(ctx.isRow, *ctx.targetPanel);
        const int32_t pushMinThreshold = GetResizerPushLeftThreshold(ctx.isRow, *ctx.targetPanel, minLeftValue) + ctx.valueOffset;

        if (ctx.currentValue < pushMinThreshold) {
            if (ctx.pushIndex > 0) {
                ctx.pushIndex--;
                ctx.valueOffset = minLeftValue;
                TranslatePanelResizer(ctx);
            } else {
                ctx.currentValue = pushMinThreshold;
                UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
            }
            return;
        }

        const Viewpanel* nextPanel = ctx.targetPanel->parent->getChild(ctx.index + 1);
        const int minRightValue = GetPanelMinExtent1D(ctx.isRow, *nextPanel);
        const int32_t pushMaxThreshold = GetResizerPushRightThreshold(ctx.isRow, *nextPanel) - ctx.valueOffset;

        if (ctx.currentValue > pushMaxThreshold) {
            if (ctx.pushIndex < ctx.panelsCount - 2) {
                ctx.pushIndex++;
                ctx.valueOffset = minRightValue;
                TranslatePanelResizer(ctx);
            } else {
                ctx.currentValue = pushMaxThreshold;
                UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
            }
            return;
        }

        UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
    }

    void HandlePanelResizerRightPush(SplitterContext& ctx) {
        Viewpanel* pushPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex);
        const Viewpanel* nextPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex + 1);
        const int minValue = GetPanelMinExtent1D(ctx.isRow, *nextPanel);
        const int32_t pushMaxThreshold = GetResizerPushRightThreshold(ctx.isRow, *nextPanel) - ctx.valueOffset;

        if (ctx.currentValue > pushMaxThreshold) {
            if (ctx.pushIndex < ctx.panelsCount - 2) {
                ctx.pushIndex++;
                ctx.valueOffset += minValue;
                TranslatePanelResizer(ctx);
            } else {
                ctx.currentValue = pushMaxThreshold;
                UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.min, 0, ctx.parentExtent);
                ResetViewResizerPushContext(ctx);
            }
        } else {
            UpdatePanelResizerValue(*pushPanel, ctx.currentValue, -ctx.valueOffset, ctx.min, ctx.parentExtent);
            ResetViewResizerPushContext(ctx);
        }
    }

    void HandlePanelResizerLeftPush(SplitterContext& ctx) {
        Viewpanel* pushPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex);
        const int minValue = GetPanelMinExtent1D(ctx.isRow, *pushPanel);
        const int32_t pushMinThreshold = GetResizerPushLeftThreshold(ctx.isRow, *pushPanel, minValue) + ctx.valueOffset;

        if (ctx.currentValue < pushMinThreshold) {
            if (ctx.pushIndex > 0) {
                ctx.pushIndex--;
                ctx.valueOffset += minValue;
                TranslatePanelResizer(ctx);
            } else {
                ctx.currentValue = pushMinThreshold;
                UpdatePanelResizerValue(*pushPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
                ResetViewResizerPushContext(ctx);
            }
        } else {
            UpdatePanelResizerValue(*pushPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
            ResetViewResizerPushContext(ctx);
        }
    }

    void ViewpanelPollEvent(Viewport& viewport, Viewpanel& viewpanel, input::InputResource& input, input::StandardCursorResource& cursor)
    {
        if (!viewpanel.parent) return;

        const auto mx = input.mousePosition.x;
        const auto my = input.mousePosition.y;

        if (viewport.splitterContext.isActive) {
            SplitterContext& ctx = viewport.splitterContext;
            const int mouse = ctx.isRow ? mx : my;
            ctx.currentValue = mouse - ctx.clickOffset;
            ctx.currentValue = std::clamp(ctx.currentValue, ctx.min, ctx.max);

            TranslatePanelResizer(ctx);
            BuildPanelLayout(*viewport.panel, 0, 0, viewport.extent.width, viewport.extent.height);

            return;
        }

        const bool overSplitter =
            mx > viewpanel.resizerZone.offset.x &&
            my > viewpanel.resizerZone.offset.y &&
            mx < viewpanel.resizerZone.offset.x + static_cast<int32_t>(viewpanel.resizerZone.extent.width) &&
            my < viewpanel.resizerZone.offset.y + static_cast<int32_t>(viewpanel.resizerZone.extent.height);

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
            const auto allPanels = viewport.panel->getAllPanels();
            for (const auto panel : allPanels) {
                if (panel->pickingMode == PickingMode::ePosition) {
                    ViewpanelPollEvent(viewport, *panel, i, cursor);
                }
            }
        }));

        viewport.leftClickEventHandle.emplace(input.onLeftMouseButtonCallbackPool.bind([&viewport](const input::InputResource& inputResource){
            if (inputResource.leftMouseButtonAction.phase == input::ActionPhase::eStart && viewport.splitterContext.targetPanel){
                EnabledViewportResizer(viewport, inputResource, true);
            }else if (inputResource.leftMouseButtonAction.phase == input::ActionPhase::eEnd){
                EnabledViewportResizer(viewport, inputResource, false);
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
