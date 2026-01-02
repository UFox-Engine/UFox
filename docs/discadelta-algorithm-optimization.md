# Discadelta: Pre-compute & Scaling Pass Constraints Pass Optimization🦊 (Chapter 3)

## Overview
The previous chapter introduced Recursive Redistribution for handling min/max constraints in compression, providing fair proportional allocation across unconstrained segments. While effective, recursion can introduce performance overhead in high-segment-count scenarios or real-time GUI updates.

This chapter optimizes the algorithm into a **single-pass iterative loop** for both compression and expansion. By pre-computing priority orders (for min-hit likelihood in compression and max-growth room in expansion), Discadelta achieves the same fair results without recursion — maximizing CPU cache efficiency, reducing stack usage, and enabling better Vulkan/SDL3/GLFW performance in dynamic layouts.

The technique relies on sorting segments by "tolerance" (base - min or compress solidify) or "growth room" (max - base for expansion) — allowing the cascade to process in an optimal order that minimizes order bias in one pass.

## Core Goals
1. **Eliminate recursion** for performance and simplicity
2. **Maintain fairness** (proportional redistribution, order-independent)
3. **Preserve precision** (exact total sum, no floating-point drift)
4. **Handle clamping** (min/max) in a single pass

## Technique: Priority Sorting in Pre-Compute
To achieve single-pass fairness, we pre-compute **priority orders** during MakeDiscadeltaContext:

- **Compression Priority** (`compressPriorityIndies`): Sorted by tolerance = (base - min) / compressRatio (lowest first = hits min soonest).  
  This processes "vulnerable" segments (low tolerance) first, reducing bias.

- **Expansion Priority** (`expandPriorityIndies`): Sorted by growth room = max - base (highest first = can grow most).  
  This processes "greedy" segments (high room) first, balancing allocation.

These sorted indices allow the cascade to process in optimal order, mimicking recursive freezing without multiple passes.

### Pre-Compute Code (Priority Calculation)

#### Struct:
```cpp
struct DiscadeltaPreComputeMetrics {
// ... existing fields ...

    std::vector<size_t> compressPriorityIndies;  // sorted indices (lowest tolerance first)
    std::vector<size_t> expandPriorityIndies;    // sorted indices (highest growth room first)
};
```

#### MakeDiscadeltaContext:
```cpp
// After calculating compressCapacity, solidify, etc.
const float greaterMin = std::max(compressSolidify, minVal);
const float compressPriorityValue = std::max(0.0f, baseVal - greaterMin);

if (compressPriorityValue <= compressPriorityLowestValue) {
preComputeMetrics.compressPriorityIndies.insert(preComputeMetrics.compressPriorityIndies.begin(), i);
compressPriorityLowestValue = compressPriorityValue;
} else {
preComputeMetrics.compressPriorityIndies.push_back(i);
}
```

```cpp
// For expansion
const float expandPriorityValue = std::max(0.0f, maxVal - baseVal);
if (expandPriorityValue >= expandPriorityLowestValue) {
preComputeMetrics.expandPriorityIndies.push_back(i);
expandPriorityLowestValue = expandPriorityValue;
} else {
preComputeMetrics.expandPriorityIndies.insert(preComputeMetrics.expandPriorityIndies.begin(), i);
}
```
This sorting ensures the cascade processes in an order that minimizes bias, achieving fairness in one pass.

#### Helper Method
```cpp
constexpr float DiscadeltaScaler(const float& distance, const float& accumulateFactor, const float& factor) {
    return distance <= 0.0f || accumulateFactor <= 0.0f || factor <= 0.0f ? 0.0f : distance / accumulateFactor * factor;
}
```

## Single-Pass Compression

Using the priority order, the cascade processes vulnerable segments first:
```cpp
void DiscadeltaCompressing(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    float cascadeCompressDistance = preComputeMetrics.inputDistance;
    float cascadeBaseDistance = preComputeMetrics.accumulateBaseDistance;
    float cascadeCompressSolidify = preComputeMetrics.accumulateCompressSolidify;

    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        const size_t index = preComputeMetrics.compressPriorityIndies[i];
        DiscadeltaSegment* seg = preComputeMetrics.segments[index];

        // ... same cascade math ...
        const float compressBaseDistance = (remainDist <= 0 || remainCap <= 0 || cap <= 0 ? 0.0f :
                                            remainDist / remainCap * cap) + solidify;

        const float clampedDist = std::max(compressBaseDistance, minVal);

        seg->base = seg->distance = clampedDist;
        cascadeCompressDistance -= clampedDist;
        cascadeCompressSolidify -= solidify;
        cascadeBaseDistance -= base;
    }
}
```
This single pass achieves the same fairness as recursion by ordering intelligently.

## Single-Pass Expansion

Similarly, for expansion, process high-growth segments first:

```cpp
constexpr void DiscadeltaExpanding(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    float cascadeExpandDelta = std::max(preComputeMetrics.inputDistance - preComputeMetrics.accumulateBaseDistance, 0.0f);
    float cascadeExpandRatio = preComputeMetrics.accumulateExpandRatio;

    if (cascadeExpandDelta <= 0.0f) return;
    
    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        const size_t index = preComputeMetrics.expandPriorityIndies[i];
        DiscadeltaSegment* seg = preComputeMetrics.segments[index];
        const float& base = preComputeMetrics.baseDistances[index];
        const float& ratio = preComputeMetrics.expandRatios[index];

        const float expandDelta = DiscadeltaScaler(cascadeExpandDelta, cascadeExpandRatio, ratio);
        // const float expandDelta = cascadeExpandRatio <= 0.0f || ratio <= 0.0f ? 0.0f :
        //                            cascadeExpandDelta / cascadeExpandRatio * ratio;

        // Apply MAX Constraint
        const float maxDelta = std::max(0.0f, preComputeMetrics.maxDistances[index] - base);
        const float clampedDelta = std::min(expandDelta, maxDelta);

        seg->expandDelta = clampedDelta;
        seg->distance = base + clampedDelta;
        cascadeExpandDelta -= clampedDelta;
        cascadeExpandRatio -= ratio;
    }
}
```

### Code Sample (C++23)
```cpp
#include <format>
#include <iomanip>
#include <iostream>
#include <memory>
#include <vector>

struct DiscadeltaSegment {
    std::string name;
    float base;
    float expandDelta;
    float distance;
};

struct DiscadeltaSegmentConfig {
    std::string name;
    float base;
    float compressRatio;
    float expandRatio;
    float min;
    float max;
};

struct DiscadeltaPreComputeMetrics {
    float inputDistance{};
    std::vector<float> compressCapacities{};
    std::vector<float> compressSolidifies{};
    std::vector<float> baseDistances{};
    std::vector<float> expandRatios{};
    std::vector<float> minDistances{};
    std::vector<float> maxDistances{};

    float accumulateBaseDistance{0.0f};
    float accumulateCompressSolidify{0.0f};
    float accumulateExpandRatio{0.0f};

    // NEW: Non-owning pointers — safe because ownedSegments outlives metrics
    std::vector<DiscadeltaSegment*> segments;

    std::vector<size_t> compressPriorityIndies;
    std::vector<size_t> expandPriorityIndies;

    DiscadeltaPreComputeMetrics() = default;
    ~DiscadeltaPreComputeMetrics() = default;

    explicit DiscadeltaPreComputeMetrics(const size_t segmentCount, const float& rootBase) : inputDistance(rootBase) {
        compressCapacities.reserve(segmentCount);
        compressSolidifies.reserve(segmentCount);
        baseDistances.reserve(segmentCount);
        expandRatios.reserve(segmentCount);
        minDistances.reserve(segmentCount);
        maxDistances.reserve(segmentCount);
        segments.reserve(segmentCount);

        compressPriorityIndies.reserve(segmentCount);
        expandPriorityIndies.reserve(segmentCount);
    }
};

using DiscadeltaSegmentsHandler = std::vector<std::unique_ptr<DiscadeltaSegment>>;

constexpr auto MakeDiscadeltaContext = [](const std::vector<DiscadeltaSegmentConfig>& configs, const float inputDistance) -> std::tuple<DiscadeltaSegmentsHandler, DiscadeltaPreComputeMetrics, bool>
{
    const float validatedInputDistance = std::max(0.0f, inputDistance);
    const size_t segmentCount = configs.size();

    DiscadeltaSegmentsHandler segments;
    segments.reserve(segmentCount);

    DiscadeltaPreComputeMetrics preComputeMetrics(segmentCount, validatedInputDistance);

    float compressPriorityLowestValue = std::numeric_limits<float>::max();
    float expandPriorityLowestValue{0.0f};

    for (size_t i = 0; i < segmentCount; ++i) {
        const auto& [name, rawBase, rawCompressRatio, rawExpandRatio, rawMin, rawMax] = configs[i];

        // --- INPUT VALIDATION ---
        const float minVal = std::max(0.0f, rawMin);
        const float maxVal = std::max(minVal, rawMax);
        const float baseVal = std::clamp(rawBase, minVal, maxVal);

        const float compressRatio = std::max(0.0f, rawCompressRatio);
        const float expandRatio   = std::max(0.0f, rawExpandRatio);

        // --- COMPRESS METRICS ---
        const float compressCapacity = baseVal * compressRatio;
        const float compressSolidify = std::max (0.0f, baseVal - compressCapacity);

        // --- STORE PRE-COMPUTE ---
        preComputeMetrics.compressCapacities.push_back(compressCapacity);
        preComputeMetrics.compressSolidifies.push_back(compressSolidify);
        preComputeMetrics.baseDistances.push_back(baseVal);
        preComputeMetrics.expandRatios.push_back(expandRatio);
        preComputeMetrics.minDistances.push_back(minVal);
        preComputeMetrics.maxDistances.push_back(maxVal);

        preComputeMetrics.accumulateBaseDistance += baseVal;
        preComputeMetrics.accumulateCompressSolidify += compressSolidify;
        preComputeMetrics.accumulateExpandRatio += expandRatio;

        // --- CREATE OWNED SEGMENT ---
        auto seg = std::make_unique<DiscadeltaSegment>();
        seg->name = name;
        seg->base = baseVal;
        seg->distance = baseVal;
        seg->expandDelta = 0.0f;

        preComputeMetrics.segments.push_back(seg.get());
        segments.push_back(std::move(seg));

        // --- COMPRESS PRIORITY ---
        const float greaterMin = std::max(compressSolidify, minVal);
        const float compressPriorityValue = std::max(0.0f, baseVal - greaterMin);

        if (compressPriorityValue <= compressPriorityLowestValue) {
            preComputeMetrics.compressPriorityIndies.insert(preComputeMetrics.compressPriorityIndies.begin(), i);
            compressPriorityLowestValue = compressPriorityValue;
        }
        else {
            preComputeMetrics.compressPriorityIndies.push_back(i);
        }

        // --- EXPAND PRIORITY ---
        const float expandPriorityValue = std::max(0.0f, maxVal - baseVal);
        if (expandPriorityValue >= expandPriorityLowestValue) {
            preComputeMetrics.expandPriorityIndies.push_back(i);
            expandPriorityLowestValue = expandPriorityValue;
        }
        else {
            preComputeMetrics.expandPriorityIndies.insert(preComputeMetrics.expandPriorityIndies.begin(), i);
        }
    }

    const bool processingCompression = validatedInputDistance < preComputeMetrics.accumulateBaseDistance;

    return { std::move(segments), std::move(preComputeMetrics), processingCompression };
};

constexpr float DiscadeltaScaler(const float& distance, const float& accumulateFactor, const float& factor) {
    return distance <= 0.0f || accumulateFactor <= 0.0f || factor <= 0.0f ? 0.0f : distance / accumulateFactor * factor;
}

constexpr void DiscadeltaCompressing(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    float cascadeCompressDistance = preComputeMetrics.inputDistance;
    float cascadeBaseDistance = preComputeMetrics.accumulateBaseDistance;
    float cascadeCompressSolidify = preComputeMetrics.accumulateCompressSolidify;

    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        const size_t index = preComputeMetrics.compressPriorityIndies[i];
        DiscadeltaSegment* seg = preComputeMetrics.segments[index];
        if (seg == nullptr) break;

        const float remainDist = cascadeCompressDistance - cascadeCompressSolidify;
        const float remainCap = cascadeBaseDistance - cascadeCompressSolidify;
        const float& solidify = preComputeMetrics.compressSolidifies[index];

        const float compressBaseDistance = DiscadeltaScaler(remainDist, remainCap, preComputeMetrics.compressCapacities[index]) + solidify;

        const float& min = preComputeMetrics.minDistances[index];
        const float clampedDist = std::max(compressBaseDistance, min);

        seg->base = seg->distance = clampedDist;
        cascadeCompressDistance -= clampedDist;
        cascadeCompressSolidify -= solidify;
        cascadeBaseDistance -= preComputeMetrics.baseDistances[index];
    }
}

constexpr void DiscadeltaExpanding(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    float cascadeExpandDelta = std::max(preComputeMetrics.inputDistance - preComputeMetrics.accumulateBaseDistance, 0.0f);
    float cascadeExpandRatio = preComputeMetrics.accumulateExpandRatio;

    if (cascadeExpandDelta <= 0.0f) return;
    
    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        const size_t index = preComputeMetrics.expandPriorityIndies[i];
        DiscadeltaSegment* seg = preComputeMetrics.segments[index];
        const float& base = preComputeMetrics.baseDistances[index];
        const float& ratio = preComputeMetrics.expandRatios[index];

        const float expandDelta = DiscadeltaScaler(cascadeExpandDelta, cascadeExpandRatio, ratio);
        // const float expandDelta = cascadeExpandRatio <= 0.0f || ratio <= 0.0f ? 0.0f :
        //                            cascadeExpandDelta / cascadeExpandRatio * ratio;

        // Apply MAX Constraint
        const float maxDelta = std::max(0.0f, preComputeMetrics.maxDistances[index] - base);
        const float clampedDelta = std::min(expandDelta, maxDelta);

        seg->expandDelta = clampedDelta;
        seg->distance = base + clampedDelta;
        cascadeExpandDelta -= clampedDelta;
        cascadeExpandRatio -= ratio;
    }
}

int main()
{
    std::vector<DiscadeltaSegmentConfig> segmentConfigs{
      {"Segment_1", 200.0f, 0.7f, 0.1f, 0.0f, 100.0f},
      {"Segment_2", 200.0f, 1.0f, 1.0f, 300.0f, 800.0f},
      {"Segment_3", 150.0f, 0.0f, 2.0f, 0.0f, 200.0f},
      {"Segment_4", 350.0f, 0.3f, 0.5f, 50.0f, 300.0f}};

    constexpr float rootDistance = 800.0f;
    auto [segmentDistances, preComputeMetrics, processingCompression] = MakeDiscadeltaContext(segmentConfigs, rootDistance);

    if (processingCompression) {
        DiscadeltaCompressing(preComputeMetrics);
    }
    else {
        DiscadeltaExpanding(preComputeMetrics);
    }

#pragma region //Print Result
    std::cout << "\n=== Dynamic Base Segment (Underflow Handling) ===\n";
    std::cout << std::format("Root distance: {:.4f}\n\n", preComputeMetrics.inputDistance);

  std::cout << std::string(123, '-') << '\n';
    // Table header
    std::cout << std::left
              << std::setw(2) << "|"
              << std::setw(10) << "Segment"
              << std::setw(2) << "|"
              << std::setw(20) << "Compress Solidify"
              << std::setw(2) << "|"
              << std::setw(20) << "Compress Capacity"
              << std::setw(2) << "|"
              << std::setw(20) << "Compress Distance"
              << std::setw(2) << "|"
              << std::setw(20) << "Expand Delta"
              << std::setw(2) << "|"
              << std::setw(20) << "Scaled Distance"
              << std::setw(2) << "|"
              << '\n';

    std::cout << std::string(123, '-') << '\n';
    float total{0.0f};
    for (size_t i = 0; i < segmentDistances.size(); ++i) {
        const auto& res = segmentDistances[i];

        total += res->distance;

        std::cout << std::fixed << std::setprecision(3)
                  << std::setw(2) << "|"
                  << std::setw(10) << res->name
                  << std::setw(2) << "|"
                  << std::setw(20) << preComputeMetrics.compressSolidifies[i]
                  << std::setw(2) << "|"
                  << std::setw(20) << preComputeMetrics.compressCapacities[i]
                  << std::setw(2) << "|"
                  << std::setw(20) << res->base
                  << std::setw(2) << "|"
                  << std::setw(20) << res->expandDelta
                  << std::setw(2) << "|"
                  << std::setw(20) << res->distance
                  << std::setw(2) << "|"
                  << '\n';
    }

        std::cout << std::string(123, '-') << '\n';
        std::cout << std::format("Total: {:.3f} (expected 800.0)\n", total);
        #pragma endregion //Print Result

        return 0;
    }
```

## Summary
With priority sorting, Discadelta achieves fair clamping in a single pass — no recursion, maximum performance for UFox real-time layouts. This is the peak optimization, making Discadelta faster than recursive alternatives while maintaining precision and fairness.

## What's Next: The Placing Pass
We have successfully solved the "How big?" question. Every segment now has a finalized `distance` that respects its min, max, and proportional fair share.

However, in a high-performance engine, knowing the **size** isn't enough—we need to know the **position**. In the next chapter, we introduce the final piece of the pipeline: **The Placing Pass**.

We will explore how to translate these resolved distances into world-space offsets and how the `size_t order` member allows us to swap or reorder segments in real-time without ever having to touch this scaling math again.

### Next Chapter: [Discadelta: Dynamic Placing Pass](discadelta-algorithm-dynamic-placing.md)