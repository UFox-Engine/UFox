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

        if (!length.hasUnit()) {
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

    constexpr std::pair<int,int> GetPanelAutoBaseLength2D(const Viewpanel& panel) noexcept {
        if (panel.isChildrenEmpty()) return std::make_pair(0,0);
        const int range = GetPanelAlignLength(panel);
        int baseWidthLength = 0;
        int baseHeightLength = 0;

        for (const auto& child : panel.children) {
            if (!child) continue;
            auto [w,h] = GetPanelAutoBaseLength2D(*child);
            if (!child->width.hasUnit()) {
                baseWidthLength += w;
            }
            else {
                if (child->width.isPixel()) {
                    baseWidthLength += std::max(0, static_cast<int>(child->width.value));
                }else if (child->width.isPercent()) {
                    const float percent = mathf::Clamp(child->width.value, 0.0f, 100.0f) * 0.01f;
                    baseWidthLength += mathf::MulToInt(percent, range);
                }
            }

            if (!child->height.hasUnit()) {
                baseHeightLength += h;
            }
            else {
                if (child->height.isPixel()) {
                    baseHeightLength += std::max(0, static_cast<int>(child->height.value));
                }else if (child->height.isPercent()) {
                    const float percent = mathf::Clamp(child->height.value, 0.0f, 100.0f) * 0.01f;
                    baseHeightLength += mathf::MulToInt(percent, range);
                }
            }
        }

        return std::make_pair(baseWidthLength, baseHeightLength);
    }

    constexpr int CalculateSegmentLength(const Length& length, int childAccumulated, int range) noexcept {
    if (!length.hasUnit()) {
        return childAccumulated;
    }
    if (length.isPixel()) {
        return std::max(0, static_cast<int>(length.value));
    }
    if (length.isPercent()) {
        const float percent = mathf::Clamp(length.value, 0.0f, 100.0f) * 0.01f;
        return mathf::MulToInt(percent, range);
    }
    return 0;
}

    constexpr int CalculateConstrainedSize(int baseLength, const Length& minLength, int parentRef, float invFlexShrink) noexcept {
        const int minVal = ReadLengthValue(minLength, parentRef, 0);
        const int shrunkVal = mathf::MulToInt(baseLength, invFlexShrink);
        return std::max(shrunkVal, minVal);
    }

    constexpr void ChooseRectLayoutGreaterMin(Viewpanel& panel) noexcept {
        if (panel.isChildrenEmpty()) {
            panel.layout.greaterMinWidth = panel.layout.minWidth;
            panel.layout.greaterMinHeight = panel.layout.minHeight;
            return;
        }

        const bool isRow = panel.isRow();
        int accumulatedWidth = 0;
        int accumulatedHeight = 0;

        for (const auto& child : panel.children) {
            ChooseRectLayoutGreaterMin(*child);
            const auto& childLayout = child->layout;

            if (isRow) {
                accumulatedWidth += childLayout.greaterMinWidth;
                accumulatedHeight = std::max(accumulatedHeight, childLayout.greaterMinHeight);
            } else {
                accumulatedWidth = std::max(accumulatedWidth, childLayout.greaterMinWidth);
                accumulatedHeight += childLayout.greaterMinHeight;
            }
        }

        panel.layout.greaterMinWidth = std::max(accumulatedWidth, panel.layout.minWidth);
        panel.layout.greaterMinHeight = std::max(accumulatedHeight, panel.layout.minHeight);

    }

    constexpr std::pair<int, int> AccumulateRectLayoutStepBaseLength(Viewpanel& panel) noexcept {
        const int pW = static_cast<int>(panel.hasParent() ? panel.parent->rect.extent.width : panel.rect.extent.width);
        const int pH = static_cast<int>(panel.hasParent() ? panel.parent->rect.extent.height : panel.rect.extent.height);
        const int alignRange = GetPanelAlignLength(panel);

        int baseWidthLength = 0;
        int baseHeightLength = 0;

        for (const auto& child : panel.children) {
            if (!child) continue;
            const auto [childW, childH] = AccumulateRectLayoutStepBaseLength(*child);

            baseWidthLength += CalculateSegmentLength(child->width, childW, alignRange);
            baseHeightLength += CalculateSegmentLength(child->height, childH, alignRange);
        }

        panel.layout.baseWidth = ReadLengthValue(panel.width, pW, baseWidthLength);
        panel.layout.baseHeight = ReadLengthValue(panel.height, pH, baseHeightLength);

        const float invFlexShrink = mathf::InvertRatio(panel.flexShrink);

        panel.layout.minWidth = CalculateConstrainedSize(baseWidthLength, panel.minWidth, pW, invFlexShrink);
        panel.layout.minHeight = CalculateConstrainedSize(baseHeightLength, panel.minHeight, pH, invFlexShrink);

        ChooseRectLayoutGreaterMin(panel);
        return {baseWidthLength, baseHeightLength};
    }

//Lagacy
    // constexpr std::pair<int,int> GetPanelMin(const Viewpanel& panel) {
    //     const int pW = static_cast<int>(panel.hasParent()? panel.parent->rect.extent.width : panel.rect.extent.width);
    //     const int pH = static_cast<int>(panel.hasParent()? panel.parent->rect.extent.height : panel.rect.extent.height);
    //     const int minWidth = ReadLengthValue(panel.minWidth, pW, 0);
    //     const int minHeight = ReadLengthValue(panel.minHeight, pH, 0);
    //
    //     const auto [autoBaseWidth, autoBaseHeight] = GetPanelAutoBaseLength2D(panel);
    //
    //     const int baseWidth = ReadLengthValue(panel.width, pW, autoBaseWidth);
    //     const int baseHeight = ReadLengthValue(panel.height, pH, autoBaseHeight);
    //
    //     const float invertFlex = mathf::InvertRatio(panel.flexShrink);
    //     const int shrinkBaseWidth = mathf::MulToInt(baseWidth, invertFlex);
    //     const int shrinkBaseHeight = mathf::MulToInt(baseHeight, invertFlex);
    //
    //     const int& w = shrinkBaseWidth > minWidth? shrinkBaseWidth: minWidth;
    //     const int& h = shrinkBaseHeight > minHeight? shrinkBaseHeight: minHeight;
    //
    //     return { w, h };
    // }
    //
    //
    // constexpr void InitRectLayoutContext(Viewpanel& panel) noexcept {
    //     const auto [w, h] = GetPanelMin(panel);
    //
    //     if (panel.isChildrenEmpty()) {
    //         panel.layout2.minWidth = w;
    //         panel.layout2.minHeight = h;
    //
    //         debug::log(debug::LogLevel::eInfo, "old panel {} min w {}", panel.name, panel.layout2.minWidth);
    //         debug::log(debug::LogLevel::eInfo, "old panel {} min h {}", panel.name, panel.layout2.minHeight);
    //         return;
    //     }
    //
    //     const bool isRow = panel.isRow();
    //
    //     int sumsWidth = 0;
    //     int sumsHeight = 0;
    //
    //     for (const auto &child : panel.children) {
    //       InitRectLayoutContext(*child);
    //       if (isRow) {
    //         sumsWidth += child->layout2.minWidth;
    //         if (child->layout2.minHeight > sumsHeight)
    //           sumsHeight = child->layout2.minHeight;
    //       } else {
    //         if (child->layout2.minWidth > sumsWidth)
    //           sumsWidth = child->layout2.minWidth;
    //         sumsHeight += child->layout2.minHeight;
    //       }
    //     }
    //
    //     const int finalWidth = std::max(sumsWidth, w);
    //     const int finalHeight = std::max(sumsHeight, h);
    //
    //
    //     panel.layout2.minWidth = finalWidth;
    //     panel.layout2.minHeight = finalHeight;
    //
    //     debug::log(debug::LogLevel::eInfo, "old panel {} min w {}", panel.name, panel.layout2.minWidth);
    //     debug::log(debug::LogLevel::eInfo, "old panel {} min h {}", panel.name, panel.layout2.minHeight);
    // }
    //
    // constexpr std::pair<int,int> GetPanelSumsMinSize2D(const Viewpanel& panel) noexcept {
    //
    //     const auto [w, h] = GetPanelMin(panel);
    //
    //     if (panel.isChildrenEmpty()) {
    //         return { w, h };
    //     }
    //
    //     int sumsWidth = 0;
    //     int sumsHeight = 0;
    //
    //     if (panel.isRow()) {
    //         for (const auto& child : panel.children) {
    //             const auto size = GetPanelSumsMinSize2D(*child);
    //             sumsWidth += size.first;
    //             if (size.second > sumsHeight) sumsHeight = size.second;
    //         }
    //     }
    //     else {
    //         for (const auto& child : panel.children) {
    //             const auto size = GetPanelSumsMinSize2D(*child);
    //             if (size.first > sumsWidth) sumsWidth = size.first;
    //             sumsHeight += size.second;
    //         }
    //     }
    //
    //     int finalWidth = sumsWidth > w ? sumsWidth : w;
    //     int finalHeight = sumsHeight > h ? sumsHeight : h;
    //
    //     return std::make_pair(finalWidth, finalHeight);
    // }
    //
    // constexpr int GetPanelSumsMinSize1D(const bool& isRow,const Viewpanel& panel) noexcept {
    //     auto [w, h] = GetPanelSumsMinSize2D(panel);
    //     return isRow ? w : h;
    // }

    constexpr void ComputeDiscadeltaBase(DiscadeltaBaseFiller& filler, const size_t& currentStep) noexcept {
        const int& offset = filler.offsets[currentStep];
        const int& min = filler.mins[currentStep];
        const float& stepRatio = filler.stepRatios[currentStep];
        int& remainLength = filler.remainLength;
        float& remainStepRatio = filler.accumulateStepRatio;
        int& accumulateOffset = filler.accumulateOffset;
        int& baseLength = *filler.lengths[currentStep];
        const int endLength = remainLength - accumulateOffset;
        const int flexLength = remainLength <= 0? 0 :mathf::MulToInt(mathf::Divide(endLength, remainStepRatio), stepRatio);

        baseLength = std::max(flexLength + offset, min);

        accumulateOffset -= offset;
        remainLength -= baseLength;
        remainStepRatio -= stepRatio;
    }

    constexpr void RedistributeDiscadeltaRemainGlow(const DiscadeltaGlowFiller& filler) noexcept {
        int   remainGlow     = filler.remainLength;
        int   accumulated    = 0;
        float remainRatio    = filler.accumulateStepRatio;

        DiscadeltaGlowFiller nextFiller{};

        for (size_t i = 0; i < filler.lengths.size(); ++i) {
            int&  length     = *filler.lengths[i];
            const float ratio = filler.stepRatios[i];
            const int   max   = filler.maxs[i];

            const int flex = remainGlow <= 0 ? 0
                : mathf::MulToInt(mathf::Divide(remainGlow, remainRatio), ratio);

            const int overflow = std::max(0, flex - max);
            const int used     = flex - overflow;
            length += used;

            if (overflow <= 0) {
                nextFiller.accumulateStepRatio += ratio;
                nextFiller.lengths.push_back(&length);
                nextFiller.maxs.push_back(max);
                nextFiller.stepRatios.push_back(ratio);
            }

            remainGlow   -= flex;
            accumulated  += used;
            remainRatio  -= ratio;
        }

        nextFiller.remainLength = std::max(0, filler.remainLength - accumulated);

        if (nextFiller.remainLength > 0 && !nextFiller.lengths.empty()) {
            RedistributeDiscadeltaRemainGlow(nextFiller);
        }
    }

    constexpr DiscadeltaContext MakeDiscadeltaContext(const Viewpanel& viewpanel) {
        const size_t childCount = viewpanel.childCount();
        DiscadeltaContext discadeltaCtx(GetPanelAlignOffset(viewpanel),childCount);
        DiscadeltaBaseFiller baseFiller(childCount);

        const int relativeLength = GetPanelAlignLength(viewpanel);
        int absoluteBaseLength{0};
        float remainSumsFlexGlow{0.0f};

        for (size_t i = 0; i < childCount; i++) {
            const auto child = viewpanel.getChild(i);
            if (child == nullptr)
                continue;

            const auto [autoBaseWidth, autoBaseHeight] = GetPanelAutoBaseLength2D(*child);
            const int baseLength = viewpanel.isRow()? ReadLengthValue(child->width,relativeLength, autoBaseWidth):ReadLengthValue(child->height,relativeLength, autoBaseHeight);
            const int alignMin = viewpanel.isRow()? child->layout.greaterMinWidth: child->layout.greaterMinHeight;
            const int greaterMax = std::max(alignMin, baseLength);

            const int alignMax = viewpanel.isRow()? ReadLengthValue(child->maxWidth, relativeLength, greaterMax): ReadLengthValue(child->maxHeight, relativeLength, greaterMax);
            int finalBaseLength = mathf::Clamp(baseLength, alignMin, alignMax);
            absoluteBaseLength += finalBaseLength;

            const int flexShrinkOffset = mathf::MulToInt(finalBaseLength, mathf::InvertRatio(child->flexShrink));
            const int flexShrinkLength = mathf::MulToInt(finalBaseLength, child->flexShrink);
            const float flexShrinkRatio = mathf::Divide(flexShrinkLength, 100.0f) ;

            discadeltaCtx.baseLengths.push_back(finalBaseLength);

            baseFiller.lengths.push_back(&discadeltaCtx.baseLengths[i]);
            baseFiller.mins.push_back(alignMin);
            baseFiller.offsets.push_back(flexShrinkOffset);
            baseFiller.stepRatios.push_back(flexShrinkRatio);
            baseFiller.accumulateStepRatio += flexShrinkRatio;
            baseFiller.accumulateOffset += flexShrinkOffset;

            discadeltaCtx.glowLengths.push_back(0);

            remainSumsFlexGlow += child->flexGlow;
        }

        if (relativeLength >= absoluteBaseLength) {
            DiscadeltaGlowFiller filler{};

            const int flexGlowLength = std::max(0, relativeLength - absoluteBaseLength);
            int remainFlexGlowLength = flexGlowLength;
            int accumulateFlexGlowLength{0};

            for (size_t i = 0; i < childCount; i++) {
                const auto child = viewpanel.getChild(i);
                if (child == nullptr)
                    continue;

                const int& baseLength = discadeltaCtx.baseLengths[i];
                const float& ratio = child->flexGlow;
                const int flexLength = remainFlexGlowLength <= 0? 0 :mathf::MulToInt(mathf::Divide(remainFlexGlowLength, remainSumsFlexGlow), ratio);
                const int fullLength = baseLength + flexLength;
                const int max = (viewpanel.isRow()? ReadLengthValue(child->maxWidth, relativeLength, fullLength): ReadLengthValue(child->maxHeight, relativeLength, fullLength)) - baseLength;
                const int offset = std::max(0,flexLength - max);

                const int finalLength = std::max(0, flexLength - offset);
                discadeltaCtx.glowLengths[i] = finalLength;

                //populate the container with the content that not yet reaches max capacity
                if (offset <= 0) {
                    filler.accumulateStepRatio += ratio;
                    filler.lengths.emplace_back(&discadeltaCtx.glowLengths[i]);
                    filler.maxs.emplace_back(max);
                    filler.stepRatios.emplace_back(ratio);
                }

                remainFlexGlowLength -= flexLength; // consume calculated length
                accumulateFlexGlowLength += finalLength; // accumulate clamped length
                remainSumsFlexGlow -= ratio;

            }

            filler.remainLength = std::max(0, flexGlowLength - accumulateFlexGlowLength);

            if (filler.remainLength > 0 && !filler.lengths.empty()) {
                RedistributeDiscadeltaRemainGlow(filler);
            }
        }
        else {
            baseFiller.remainLength = std::max(0, relativeLength);

            for (size_t i = 0; i < childCount; i++) {
                ComputeDiscadeltaBase(baseFiller, i);
            }
        }

        return discadeltaCtx;
    }

    constexpr void MakePanelsLayout(Viewpanel& viewpanel, const int& posX, const int& posY, const int& width, const int& height) {
        SetLocalPanelRect(viewpanel, posX, posY, width, height);

        if (viewpanel.isChildrenEmpty()) return;

        DiscadeltaContext flexCtx = MakeDiscadeltaContext(viewpanel);

        const bool isRow = viewpanel.isRow();

        for (size_t i = 0; i < viewpanel.childCount(); ++i) {
            Viewpanel *child = viewpanel.getChild(i);
            if (!child) continue;

            const int baseLength = flexCtx.baseLengths[i];
            const int flexDelta = flexCtx.glowLengths[i];

            const int targetLength = std::max(0,baseLength + flexDelta );

            const int targetWidthLength = isRow? targetLength: width;
            const int targetHeightLength = isRow? height: targetLength;

            const int targetXOffset = isRow? flexCtx.accumulateOffset : viewpanel.rect.offset.x;
            const int targetYOffset = isRow? viewpanel.rect.offset.y : flexCtx.accumulateOffset;

            MakePanelsLayout(*child, targetXOffset, targetYOffset, targetWidthLength,targetHeightLength);

            flexCtx.accumulateOffset += targetLength;

            //Legacy
            //SetPanelResizerRect(*child,i < flexCtx.shareAmount, flexCtx.isRow, targetXOffset, targetYOffset, width, height);
        }
    }

    constexpr void ResizingViewport(Viewport& viewport, const int& width, const int& height) {
        viewport.extent.width  = static_cast<uint32_t>(width);
        viewport.extent.height = static_cast<uint32_t>(height);

        if (viewport.panel){
            Viewpanel& panel = *viewport.panel;
            AccumulateRectLayoutStepBaseLength(panel);
            //InitRectLayoutContext(panel);
#ifdef USE_SDL
            SDL_SetWindowMinimumSize(viewport.window.getHandle(),panel.layout.greaterMinWidth, panel.layout.greaterMinHeight);
#else
            glfwSetWindowSizeLimits(viewport.window.getHandle(),panel.layout.greaterMinWidth, panel.layout.greaterMinHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
#endif
            MakePanelsLayout(*viewport.panel, 0, 0, static_cast<int>(viewport.extent.width), static_cast<int>(viewport.extent.height));
        }
    }

    //Legacy
    // constexpr int GetResizerPushLeftThreshold(const bool& row, const Viewpanel& panel, const int& minExtent, const int& offset = 0) noexcept {
    //     return GetPanelPosition1D(row, panel) + minExtent + offset;
    // }
    //
    // constexpr int GetResizerPushRightThreshold(const bool& row, const Viewpanel& panel, const int& min, const int& offset = 0 )noexcept {
    //     return GetPanelGlobalLength(row, panel) - offset - min;
    // }
    //

    //
    // constexpr void EnabledViewportResizer(Viewport& viewport, const input::InputResource& input, const bool& state) {
    //     if (state) {
    //         ViewpanelResizerContext& ctx    = viewport.resizerContext;
    //
    //         if (!ctx.targetPanel)           return;
    //         Viewpanel* target               = ctx.targetPanel;
    //
    //         if (!target->parent)            return;
    //
    //         Viewpanel* parent               = target->parent;
    //         const bool isRow                = parent->isRow();
    //         const int parentPos             = GetPanelPosition1D(isRow, *parent);
    //         const int parentExtent          = GetPanelExtent1D(isRow, *parent);
    //         const size_t panelsCount        = parent->childCount();
    //         const size_t index              = utilities::Index_Of(std::span{parent->children}, target).value_or(static_cast<size_t>(-1));
    //         const int clickOffset           = (isRow? input.mousePosition.x - target->resizerZone.offset.x : input.mousePosition.y - target->resizerZone.offset.y) - RESIZER_OFFSET;
    //
    //         ctx.index                       = index;
    //         ctx.isRow                       = isRow;
    //         ctx.panelsCount                 = panelsCount;
    //         ctx.pushIndex                   = index;
    //         ctx.parentExtent                = parentExtent;
    //         ctx.min                         = parentPos;
    //         ctx.max                         = parentPos + parentExtent;
    //         ctx.clickOffset                 = clickOffset;
    //         ctx.isActive                    = true;
    //     }
    //     else {
    //         viewport.resizerContext.isActive = false;
    //     }
    // }
    //
    // constexpr void DisableViewportResizer(Viewport& viewport) {
    //     viewport.resizerContext.isActive = false;
    // }
    //
    // constexpr void HandleTargetPanelResizerPush(ViewpanelResizerContext& ctx);
    // constexpr void HandlePanelResizerRightPush(ViewpanelResizerContext& ctx);
    // constexpr void HandlePanelResizerLeftPush(ViewpanelResizerContext& ctx);
    //
    // constexpr void TranslatePanelResizer(ViewpanelResizerContext& ctx) {
    //     if (ctx.pushIndex >= ctx.panelsCount) {
    //         ctx.isActive = false;
    //         return;
    //     }
    //
    //     if (ctx.pushIndex == ctx.index) {
    //         HandleTargetPanelResizerPush(ctx);
    //     } else if (ctx.pushIndex < ctx.index) {
    //         HandlePanelResizerLeftPush(ctx);
    //     } else if (ctx.pushIndex > ctx.index) {
    //         HandlePanelResizerRightPush(ctx);
    //     }
    // }
    //
    // constexpr void UpdatePanelResizerValue(Viewpanel& panel, const int& value, const int& valueOffset, const int& minValue, const int& parentExtent) {
    //
    //     panel.resizerValue = static_cast<float>(value - valueOffset - minValue ) / static_cast<float>(parentExtent);
    //
    // }
    //
    // constexpr void ResetViewResizerPushContext(ViewpanelResizerContext& ctx) {
    //     ctx.valueOffset = 0;
    //     ctx.pushIndex = ctx.index;
    // }
    //
    // constexpr void HandleTargetPanelResizerPush(ViewpanelResizerContext& ctx) {
    //     const int minLeftValue = GetPanelSumsMinSize1D(ctx.isRow, *ctx.targetPanel);
    //     const int32_t pushMinThreshold = GetResizerPushLeftThreshold(ctx.isRow, *ctx.targetPanel, minLeftValue, ctx.valueOffset);
    //
    //     if (ctx.currentValue < pushMinThreshold) {
    //         if (ctx.pushIndex > 0) {
    //             ctx.pushIndex--;
    //             ctx.valueOffset = minLeftValue;
    //             TranslatePanelResizer(ctx);
    //         } else {
    //             ctx.currentValue = pushMinThreshold;
    //             UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
    //         }
    //         return;
    //     }
    //
    //     const Viewpanel* nextPanel = ctx.targetPanel->parent->getChild(ctx.index + 1);
    //     const int minRightValue = GetPanelSumsMinSize1D(ctx.isRow, *nextPanel);
    //     const int32_t pushMaxThreshold = GetResizerPushRightThreshold(ctx.isRow, *nextPanel, minRightValue, ctx.valueOffset);
    //
    //     if (ctx.currentValue > pushMaxThreshold) {
    //
    //         if (ctx.pushIndex < ctx.panelsCount - 2) {
    //             ctx.pushIndex++;
    //             ctx.valueOffset = minRightValue;
    //             TranslatePanelResizer(ctx);
    //         } else {
    //             ctx.currentValue = pushMaxThreshold;
    //             UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
    //         }
    //         return;
    //     }
    //
    //     UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
    // }
    //
    // constexpr void HandlePanelResizerRightPush(ViewpanelResizerContext& ctx) {
    //     Viewpanel* pushPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex);
    //     const Viewpanel* nextPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex + 1);
    //     const int minValue = GetPanelSumsMinSize1D(ctx.isRow, *nextPanel);
    //     const int pushMaxThreshold = GetResizerPushRightThreshold(ctx.isRow, *nextPanel, minValue, ctx.valueOffset);
    //
    //     if (ctx.currentValue > pushMaxThreshold) {
    //
    //         if (ctx.pushIndex < ctx.panelsCount - 2) {
    //             ctx.pushIndex++;
    //             ctx.valueOffset += minValue;
    //             TranslatePanelResizer(ctx);
    //         } else {
    //
    //             ctx.currentValue = pushMaxThreshold;
    //             UpdatePanelResizerValue(*ctx.targetPanel, ctx.currentValue, ctx.min, 0, ctx.parentExtent);
    //             ResetViewResizerPushContext(ctx);
    //         }
    //     } else {
    //
    //         UpdatePanelResizerValue(*pushPanel, ctx.currentValue, -ctx.valueOffset, ctx.min, ctx.parentExtent);
    //         ResetViewResizerPushContext(ctx);
    //     }
    // }
    //
    // constexpr void HandlePanelResizerLeftPush(ViewpanelResizerContext& ctx) {
    //     Viewpanel* pushPanel = ctx.targetPanel->parent->getChild(ctx.pushIndex);
    //     const int minValue = GetPanelSumsMinSize1D(ctx.isRow, *pushPanel);
    //     const int32_t pushMinThreshold = GetResizerPushLeftThreshold(ctx.isRow, *pushPanel, minValue, ctx.valueOffset);
    //
    //     if (ctx.currentValue < pushMinThreshold) {
    //         if (ctx.pushIndex > 0) {
    //             ctx.pushIndex--;
    //             ctx.valueOffset += minValue;
    //             TranslatePanelResizer(ctx);
    //         } else {
    //             ctx.currentValue = pushMinThreshold;
    //             UpdatePanelResizerValue(*pushPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
    //             ResetViewResizerPushContext(ctx);
    //         }
    //     } else {
    //         UpdatePanelResizerValue(*pushPanel, ctx.currentValue, ctx.valueOffset, ctx.min, ctx.parentExtent);
    //         ResetViewResizerPushContext(ctx);
    //     }
    // }

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

            //TranslatePanelResizer(ctx);
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
                //EnabledViewportResizer(viewport, inputResource, true);
            }else if (inputResource.leftMouseButtonAction.phase == input::ActionPhase::eEnd){
                //EnabledViewportResizer(viewport, inputResource, false);
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
