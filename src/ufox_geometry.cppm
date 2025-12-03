module;
#include <vulkan/vulkan_raii.hpp>
#ifdef USE_SDL
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#else
#include <GLFW/glfw3.h>
#endif

#include <chrono>
#include <glm/glm.hpp>
#include <optional>
#include <set>
#include <vector>

export module ufox_geometry;

import ufox_lib;  // GPUResources, SwapchainResource, FrameResource
import ufox_input;
import ufox_graphic_device;

export namespace ufox::geometry {

    template<typename T>
        constexpr T ReadLengthValue(const Length& length,const T&  min, const T&  max, const T& autoValue) noexcept requires std::is_arithmetic_v<T> {
        const T range = max - min;

        if (!length.hasUnit() || range <= T(0)) {
            return autoValue;
        }

        if (length.isPixel()) {
            return std::max(T(0), static_cast<T>(length.value));
        }

        if (length.isPercent()) {
            const float percent = mathf::Clamp(length.value, 0.0f, 100.0f) * 0.01f;
            return min + static_cast<T>(percent * static_cast<float>(range));
        }

        return max;
    }

    template<typename T>
    constexpr T ReadLengthValue(const Length& length, const T& availableSize, const T& autoValue) noexcept requires std::is_arithmetic_v<T> {
        return ReadLengthValue(length, T(0), availableSize, autoValue);
    }

    constexpr std::pair<int,int> GetPanelMin(const Viewpanel& panel) {
        const int pW = static_cast<int>(panel.hasParent()? panel.parent->rect.extent.width : panel.rect.extent.width);
        const int pH = static_cast<int>(panel.hasParent()? panel.parent->rect.extent.height : panel.rect.extent.height);
        const int minWidth = ReadLengthValue(panel.minWidth, pW, 0);
        const int minHeight = ReadLengthValue(panel.minHeight, pH, 0);
        const int baseWidth = ReadLengthValue(panel.width, pW, 0);
        const int baseHeight = ReadLengthValue(panel.height, pH, 0);
        const float invertFlex = mathf::InvertRatio(panel.flexShrink);
        const int shrinkBaseWidth = mathf::MulToInt(baseWidth, invertFlex);
        const int shrinkBaseHeight = mathf::MulToInt(baseHeight, invertFlex);

        const int& w = shrinkBaseWidth > minWidth? shrinkBaseWidth: minWidth;
        const int& h = shrinkBaseHeight > minHeight? shrinkBaseHeight: minHeight;

        return { w, h };
    }

    constexpr std::pair<int,int> GetPanelSumsMinSize2D(const Viewpanel& panel) noexcept {

        const auto [w, h] = GetPanelMin(panel);

        if (panel.isChildrenEmpty()) {
            return { w, h };
        }

        int sumsWidth = 0;
        int sumsHeight = 0;

        if (panel.isRow()) {
            for (const auto& child : panel.children) {
                const auto size = GetPanelSumsMinSize2D(*child);
                sumsWidth += size.first;
                if (size.second > sumsHeight) sumsHeight = size.second;
            }
        }
        else {
            for (const auto& child : panel.children) {
                const auto size = GetPanelSumsMinSize2D(*child);
                if (size.first > sumsWidth) sumsWidth = size.first;
                sumsHeight += size.second;
            }
        }

        int finalWidth = sumsWidth > w ? sumsWidth : w;
        int finalHeight = sumsHeight > h ? sumsHeight : h;

        return std::make_pair(finalWidth, finalHeight);
    }

    constexpr int GetPanelSumsMinSize1D(const bool& isRow,const Viewpanel& panel) noexcept {
        auto [w, h] = GetPanelSumsMinSize2D(panel);
        return isRow ? w : h;
    }

    constexpr int GetPanelPosition1D(const bool& row, const Viewpanel& panel) noexcept {
        return row ? panel.rect.offset.x : panel.rect.offset.y;
    }

    constexpr int GetPanelExtent1D(const bool& row, const Viewpanel& panel) noexcept {
        return static_cast<int>(row ? panel.rect.extent.width : panel.rect.extent.height);
    }

    constexpr int GetParentPanelPosition1D(const bool& row, const Viewpanel& panel) noexcept {
        return !panel.parent? 0 : row ? panel.parent->rect.offset.x : panel.parent->rect.offset.y;
    }

    constexpr std::pair<int, int> GetParentPanelPosition2D(const Viewpanel& panel) noexcept {
        if (!panel.parent) return std::make_pair(0, 0);
        return std::make_pair(panel.parent->rect.offset.x, panel.parent->rect.offset.y);
    }

    constexpr int GetPanelGlobalLength(const bool& isRow, const Viewpanel& panel) noexcept {
        return isRow ? panel.rect.offset.x + static_cast<int>(panel.rect.extent.width) : panel.rect.offset.y + static_cast<int>(panel.rect.extent.height);
    }

    constexpr void SetLocalPanelRect(Viewpanel& panel, const int& localX, const int& localY, const int& width, const int& height) noexcept {
        int globalX = localX;
        int globalY = localY;
        const auto finalWidth = static_cast<uint32_t>(width);
        const auto finalHeight = static_cast<uint32_t>(height);

        const auto [pWidth, pHeight] = GetParentPanelPosition2D(panel);
        globalX += pWidth;
        globalY += pHeight;

        panel.rect = vk::Rect2D{ vk::Offset2D{localX, globalY}, vk::Extent2D{finalWidth, finalHeight}};
    }

    constexpr void SetPanelResizerRect(Viewpanel& panel, const bool& enable, const bool& isRow, const int& x, const int& y, const int& w, const int& h) noexcept {
        const int resizerX = enable ? isRow?  x - RESIZER_OFFSET: x:0;
        const int resizerY = enable? isRow? y:  y - RESIZER_OFFSET:0;
        const uint32_t resizerWidth = enable ? isRow? RESIZER_THICKNESS: static_cast<uint32_t>(w):0;
        const uint32_t resizerHeight = enable ? isRow? static_cast<uint32_t>(h): RESIZER_THICKNESS:0;

        panel.resizerZone = vk::Rect2D{vk::Offset2D{resizerX, resizerY}, vk::Extent2D{resizerWidth, resizerHeight}};
    }

    constexpr int GetPanelAlignLength(const Viewpanel& panel) noexcept {
        return static_cast<int>(panel.isRow()? panel.rect.extent.width: panel.rect.extent.height);
    }

    constexpr int GetPanelAlignOffset(const Viewpanel& panel) noexcept {
        return panel.isRow()? panel.rect.offset.x: panel.rect.offset.y;
    }

    constexpr ViewpanelFlexContext MakePanelFlexContext(const Viewpanel& viewpanel) {
        const int   relativeLength = GetPanelAlignLength(viewpanel);
        const int   relativeOffset = GetPanelAlignOffset(viewpanel);
        int         absoluteBaseLength{0};
        int         shrinkCapacityLength{0};


        float   remainSumsFlexGlow{0.0f};

        for (const auto &child : viewpanel.children) {
          if (!child)
            continue;

          const int childLength =
              viewpanel.isRow()
                  ? ReadLengthValue(
                        child->width,
                        static_cast<int>(viewpanel.rect.extent.width), 0)
                  : ReadLengthValue(
                        child->height,
                        static_cast<int>(viewpanel.rect.extent.height), 0);

          absoluteBaseLength += childLength;
          shrinkCapacityLength += static_cast<int>(
              std::round(static_cast<float>(childLength) * child->flexShrink));
          remainSumsFlexGlow += child->flexGlow;
        }

        const int targetShrinkLength = relativeLength - (absoluteBaseLength - shrinkCapacityLength);
        const int remainFlexLength = std::max(0, relativeLength - absoluteBaseLength);

        return ViewpanelFlexContext{viewpanel, relativeLength, absoluteBaseLength, shrinkCapacityLength, targetShrinkLength, relativeOffset, remainFlexLength, remainSumsFlexGlow};
    }

    constexpr int ComputePanelFlexShrinkLength(const float& baseLength, const float& flexShrink, const ViewpanelFlexContext& ctx) noexcept {
        const float shrinkBaseLength = mathf::MulToFloat(baseLength, flexShrink);
        const float shrinkRatio = mathf::Normalize(shrinkBaseLength, 0.0f, static_cast<float>(ctx.shrinkCapacityLength));
        const float projectedShrink = shrinkRatio * static_cast<float>(ctx.targetShrinkLength) + (baseLength - shrinkBaseLength);

        return std::min(0, static_cast<int>(std::lroundf(projectedShrink - baseLength)));
    }

    constexpr int ComputePanelFlexGlowLength(const float& flexGlow, ViewpanelFlexContext& ctx) noexcept {
        const int flexLength = ctx.remainFlexLength == 0? 0: mathf::MulToInt(static_cast<float>(ctx.remainFlexLength) / ctx.remainSumsFlexGlow, flexGlow);
        ctx.remainFlexLength = std::max(0, ctx.remainFlexLength - flexLength);
        ctx.remainSumsFlexGlow -= flexGlow;
        return flexLength;
    }

    constexpr void MakePanelsLayout(Viewpanel& viewpanel, const int& posX, const int& posY, const int& width, const int& height) {
        SetLocalPanelRect(viewpanel, posX, posY, width, height);

        if (viewpanel.isChildrenEmpty()) return;

        ViewpanelFlexContext flexCtx = MakePanelFlexContext(viewpanel);


        for (size_t i = 0; i < viewpanel.childCount(); ++i) {
            Viewpanel *child = viewpanel.getChild(i);
            if (!child) continue;

            const Length& length = flexCtx.isRow? child->width: child->height;
            const Length& min = flexCtx.isRow? child->minWidth: child->minHeight;
            const float baseLength = static_cast<float>(ReadLengthValue(length, 0, flexCtx.relativeLength, 0));
            const int minLength = ReadLengthValue(min, 0, flexCtx.relativeLength, 0);

            const int shrinkDelta = ComputePanelFlexShrinkLength(baseLength, child->flexShrink, flexCtx);
            const int flexDelta = ComputePanelFlexGlowLength(child->flexGlow, flexCtx);

            const int currentLength = std::max(0, static_cast<int>(baseLength) + shrinkDelta + flexDelta);



            const int targetWidthLength = flexCtx.isRow? currentLength: width;
            const int targetHeightLength = flexCtx.isRow? height: currentLength;

            const int targetXOffset = flexCtx.isRow? flexCtx.accumulateOffset : viewpanel.rect.offset.x;
            const int targetYOffset = flexCtx.isRow? viewpanel.rect.offset.y : flexCtx.accumulateOffset;

            MakePanelsLayout(*child, targetXOffset, targetYOffset, targetWidthLength,targetHeightLength);

            flexCtx.accumulateOffset += currentLength;

            SetPanelResizerRect(*child,i < flexCtx.lastIndex, flexCtx.isRow, targetXOffset, targetYOffset, width, height);
        }
    }

    constexpr int GetResizerPushLeftThreshold(const bool& row, const Viewpanel& panel, const int& minExtent, const int& offset = 0) noexcept {
        return GetPanelPosition1D(row, panel) + minExtent + offset;
    }

    constexpr int GetResizerPushRightThreshold(const bool& row, const Viewpanel& panel, const int& min, const int& offset = 0 )noexcept {
        return GetPanelGlobalLength(row, panel) - offset - min;
    }

    constexpr void ResizingViewport(Viewport& viewport, const int& width, const int& height) {
        viewport.extent.width  = static_cast<uint32_t>(width);
        viewport.extent.height = static_cast<uint32_t>(height);

        if (viewport.panel){
            MakePanelsLayout(*viewport.panel, 0, 0, static_cast<int>(viewport.extent.width), static_cast<int>(viewport.extent.height));
        }
    }

    constexpr void EnabledViewportResizer(Viewport& viewport, const input::InputResource& input, const bool& state) {
        if (state) {
            ViewpanelResizerContext& ctx    = viewport.resizerContext;

            if (!ctx.targetPanel)           return;
            Viewpanel* target               = ctx.targetPanel;

            if (!target->parent)            return;

            Viewpanel* parent               = target->parent;
            const bool isRow                = parent->isRow();
            const int parentPos             = GetPanelPosition1D(isRow, *parent);
            const int parentExtent          = GetPanelExtent1D(isRow, *parent);
            const size_t panelsCount        = parent->childCount();
            const size_t index              = utilities::Index_Of(std::span{parent->children}, target).value_or(static_cast<size_t>(-1));
            const int clickOffset           = (isRow? input.mousePosition.x - target->resizerZone.offset.x : input.mousePosition.y - target->resizerZone.offset.y) - RESIZER_OFFSET;

            ctx.index                       = index;
            ctx.isRow                       = isRow;
            ctx.panelsCount                 = panelsCount;
            ctx.pushIndex                   = index;
            ctx.parentExtent                = parentExtent;
            ctx.min                         = parentPos;
            ctx.max                         = parentPos + parentExtent;
            ctx.clickOffset                 = clickOffset;
            ctx.isActive                    = true;
        }
        else {
            viewport.resizerContext.isActive = false;
        }
    }

    constexpr void DisableViewportResizer(Viewport& viewport) {
        viewport.resizerContext.isActive = false;
    }

    constexpr void HandleTargetPanelResizerPush(ViewpanelResizerContext& ctx);
    constexpr void HandlePanelResizerRightPush(ViewpanelResizerContext& ctx);
    constexpr void HandlePanelResizerLeftPush(ViewpanelResizerContext& ctx);

    constexpr void TranslatePanelResizer(ViewpanelResizerContext& ctx) {
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

    constexpr void UpdatePanelResizerValue(Viewpanel& panel, const int& value, const int& valueOffset, const int& minValue, const int& parentExtent) {
        debug::log(debug::LogLevel::eInfo, "panel:{} value {} offset {} min {} extent {}", panel.name, value, valueOffset, minValue, parentExtent);
        panel.resizerValue = static_cast<float>(value - valueOffset - minValue ) / static_cast<float>(parentExtent);
        debug::log(debug::LogLevel::eInfo, "panel:{} value {}", panel.name, panel.resizerValue);
    }

    constexpr void ResetViewResizerPushContext(ViewpanelResizerContext& ctx) {
        ctx.valueOffset = 0;
        ctx.pushIndex = ctx.index;
    }

    constexpr void HandleTargetPanelResizerPush(ViewpanelResizerContext& ctx) {
        const int minLeftValue = GetPanelSumsMinSize1D(ctx.isRow, *ctx.targetPanel);
        const int32_t pushMinThreshold = GetResizerPushLeftThreshold(ctx.isRow, *ctx.targetPanel, minLeftValue, ctx.valueOffset);

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
        const int minRightValue = GetPanelSumsMinSize1D(ctx.isRow, *nextPanel);
        const int32_t pushMaxThreshold = GetResizerPushRightThreshold(ctx.isRow, *nextPanel, minRightValue, ctx.valueOffset);
        debug::log(debug::LogLevel::eInfo, "1 p p {}", GetPanelPosition1D(ctx.isRow, *nextPanel));
        debug::log(debug::LogLevel::eInfo, "1global length {}", GetPanelGlobalLength(ctx.isRow, *nextPanel));
        debug::log(debug::LogLevel::eInfo, "1offset {}", ctx.valueOffset);
        debug::log(debug::LogLevel::eInfo, "1ushMaxThreshold {}", pushMaxThreshold);
        if (ctx.currentValue > pushMaxThreshold) {
            debug::log(debug::LogLevel::eInfo, "1here");
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

    constexpr void HandlePanelResizerRightPush(ViewpanelResizerContext& ctx) {
        Viewpanel* pushPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex);
        const Viewpanel* nextPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex + 1);
        const int minValue = GetPanelSumsMinSize1D(ctx.isRow, *nextPanel);
        const int pushMaxThreshold = GetResizerPushRightThreshold(ctx.isRow, *nextPanel, minValue, ctx.valueOffset);
        debug::log(debug::LogLevel::eInfo, "2push p p {}", GetPanelPosition1D(ctx.isRow, *pushPanel));
        debug::log(debug::LogLevel::eInfo, "2global length {}", GetPanelGlobalLength(ctx.isRow, *nextPanel));
        debug::log(debug::LogLevel::eInfo, "2offset {}", ctx.valueOffset);
        debug::log(debug::LogLevel::eInfo, "2ushMaxThreshold {}", pushMaxThreshold);
        debug::log(debug::LogLevel::eInfo, "2here");
        if (ctx.currentValue > pushMaxThreshold) {
            debug::log(debug::LogLevel::eInfo, "2here");
            if (ctx.pushIndex < ctx.panelsCount - 2) {
                ctx.pushIndex++;
                ctx.valueOffset += minValue;
                TranslatePanelResizer(ctx);
            } else {
                debug::log(debug::LogLevel::eInfo, "here4");
                ctx.currentValue = pushMaxThreshold;
                UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.min, 0, ctx.parentExtent);
                ResetViewResizerPushContext(ctx);
            }
        } else {
            debug::log(debug::LogLevel::eInfo, "here5");
            UpdatePanelResizerValue(*pushPanel, ctx.currentValue, -ctx.valueOffset, ctx.min, ctx.parentExtent);
            ResetViewResizerPushContext(ctx);
        }
    }

    constexpr void HandlePanelResizerLeftPush(ViewpanelResizerContext& ctx) {
        Viewpanel* pushPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex);
        const int minValue = GetPanelSumsMinSize1D(ctx.isRow, *pushPanel);
        const int32_t pushMinThreshold = GetResizerPushLeftThreshold(ctx.isRow, *pushPanel, minValue, ctx.valueOffset);

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

    constexpr void ViewpanelPollEvent(Viewport& viewport, Viewpanel& viewpanel, input::InputResource& input, input::StandardCursorResource& cursor)
    {
        // auto start = std::chrono::high_resolution_clock::now();
        if (!viewpanel.parent) return;

        const auto mx = input.mousePosition.x;
        const auto my = input.mousePosition.y;

        if (viewport.resizerContext.isActive) {
            ViewpanelResizerContext& ctx = viewport.resizerContext;
            const int mouse = ctx.isRow ? mx : my;
            ctx.currentValue = mouse - ctx.clickOffset;
            ctx.currentValue = std::clamp(ctx.currentValue, ctx.min, ctx.max);
            ctx.currentValue = static_cast<int>(std::round(ctx.currentValue / RESIZER_SNAP_GRID) * RESIZER_SNAP_GRID);

            TranslatePanelResizer(ctx);
            MakePanelsLayout(*viewport.panel, 0, 0, static_cast<int>(viewport.extent.width), static_cast<int>(viewport.extent.height));

            // auto end = std::chrono::high_resolution_clock::now();
            // float ms = std::chrono::duration<float, std::milli>(end - start).count();
            //
            // debug::log(debug::LogLevel::eInfo, "UI Frame: {:.3f} ms", ms);
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
            if (viewport.resizerContext.targetPanel != &viewpanel) {
                viewport.resizerContext.targetPanel = &viewpanel;
    #ifdef USE_SDL
                input::SetStandardCursor(cursor, viewpanel.parent->isColumn() ?input::CursorType::eNSResize :input::CursorType::eEWResize);
    #else
                input::SetStandardCursor(cursor, viewpanel.parent->isColumn() ?input::CursorType::eNSResize :input::CursorType::eEWResize, viewport.window);
    #endif

            }
        }
        else if (viewport.resizerContext.targetPanel == &viewpanel)
        {
    #ifdef USE_SDL
            input::SetStandardCursor(cursor, input::CursorType::eDefault);
    #else
            input::SetStandardCursor(cursor, input::CursorType::eDefault, viewport.window);
    #endif
            viewport.resizerContext.targetPanel = nullptr;
        }

        if (isHovered) {
            if (viewport.hoveredPanel != &viewpanel)
                viewport.hoveredPanel = &viewpanel;
        }
        else if (viewport.hoveredPanel == &viewpanel) {
            viewport.hoveredPanel = nullptr;
        }
    }

    constexpr void BindEvents(Viewport& viewport, input::InputResource& input, input::StandardCursorResource& cursor) {
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
            if (inputResource.leftMouseButtonAction.phase == input::ActionPhase::eStart && viewport.resizerContext.targetPanel){
                EnabledViewportResizer(viewport, inputResource, true);
            }else if (inputResource.leftMouseButtonAction.phase == input::ActionPhase::eEnd){
                EnabledViewportResizer(viewport, inputResource, false);
            }
        }));
    }

    constexpr void UnbindEvents(Viewport& viewport, input::InputResource& input)
    {
        if (!viewport.mouseMoveEventHandle.has_value()) return;
        input.onMouseMoveCallbackPool.unbind(viewport.mouseMoveEventHandle->index);
        viewport.mouseMoveEventHandle.reset();
        if (!viewport.leftClickEventHandle.has_value()) return;
        input.onLeftMouseButtonCallbackPool.unbind(viewport.leftClickEventHandle->index);
        viewport.leftClickEventHandle.reset();
    }

    constexpr MeshResource CreateDefaultQuadMesh(const gpu::vulkan::GPUResources& gpu) {
        MeshResource quadMesh{DEFAULT_QUAD_MESH_NAME};
        quadMesh.vertices.reserve(QUAD_VERTEX_COUNT);
        quadMesh.indices.reserve(QUAD_INDEX_COUNT);

        quadMesh.vertices.assign(std::begin(QuadVertices), std::end(QuadVertices));
        quadMesh.indices.assign(std::begin(QuadIndices), std::end(QuadIndices));

        gpu::vulkan::CreateAndCopyBuffer(gpu, QuadVertices, QUAD_VERTICES_BUFFER_SIZE, quadMesh.vertexBuffer);
        gpu::vulkan::CreateAndCopyBuffer(gpu, QuadIndices, QUAD_INDICES_BUFFER_SIZE, quadMesh.indexBuffer);

        return quadMesh;
    }
}
