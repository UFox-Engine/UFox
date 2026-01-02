# Discadelta Algorithm Constraints 🦊 (Chapter 2)

## Overview
The previous chapter implemented the core Discadelta algorithm: solving precision loss and handling underflow/overflow via **Compress Base Distance** and **Expand Delta**. However, in a real-world GUI or editor system, segments often have **min** and **max** boundaries.

This chapter introduces **Recursive Redistribution**. When a segment hits a constraint, it "locks" its size, and the remaining distance is redistributed among the other segments to ensure the total distance remains perfectly equal to the root.

## Organize Data Struct

```ccp
struct DiscadeltaSegment {
    std::string name;//optional
    float base;
    float expandDelta;
    float distance;
};
```

```cpp
struct DiscadeltaSegmentConfig {
    std::string name;//optional
    float base;
    float compressRatio;
    float expandRatio;
    float min;
    float max;
};
```

```cpp
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

    DiscadeltaPreComputeMetrics() = default;
    ~DiscadeltaPreComputeMetrics() = default;

    explicit DiscadeltaPreComputeMetrics(const size_t segmentCount, const float& rootBase) : inputDistance(rootBase) {
        compressCapacities.reserve(segmentCount);
        compressSolidifies.reserve(segmentCount);
        baseDistances.reserve(segmentCount);
        expandRatios.reserve(segmentCount);
        minDistances.reserve(segmentCount);
        maxDistances.reserve(segmentCount);
    }
};
```


## Validate Min, Max and Base

Before performing any computation, Discadelta must **validate** and **sanitize** the `min` and `max` values for each segment. Invalid or inconsistent bounds can lead to:

- Negative sizes
- `max < min`
- Unbounded growth when `max` is negative or too small
- Numerical instability

**Validation Rules**:
1. **`min` must be ≥ 0**  
   → Negative minimum sizes are meaningless in most partitioning contexts.

2. **`max` must be ≥ `min`**  
   → Ensures the bounds are logically consistent.

3. **Default / fallback behavior**
   - If `min` is invalid (< 0) → clamp to `0.0f`
   - If `max` is invalid (< `min`) → set `max` = `min` (effectively fixed size)

4. **Pre-Clamp Base**
   - The preferred `base` is clamped to `[min, max]` during pre-computation.  
     This prevents illogical configurations where the "ideal" size exceeds hard constraints.  
     It aligns with modern standards (CSS Flexbox, Yoga) and ensures fairness from the start.

   
**Technical Note:** These operations require the <algorithm> header for std::clamp and std::max. Ensure your project includes it to prevent compiler errors.

### Pre-Compute Helper Method
This helper now uses standard library utilities to sanitize inputs before they enter the cascade.
```cpp
constexpr auto MakeDiscadeltaContext = [](const std::vector<DiscadeltaSegmentConfig>& configs, const float& inputDistance) {
    const float validatedInputDistance = std::max(0.0f, inputDistance);
    const size_t segmentCount = configs.size();

    std::vector<DiscadeltaSegment> segments{};
    segments.reserve(segmentCount);
    DiscadeltaPreComputeMetrics preComputeMetrics(segmentCount, validatedInputDistance);

    for (const auto& [name, rawBase, rawCompressRatio, rawExpandRatio, rawMin, rawMax] : configs) {
        // --- INPUT VALIDATION (std::max and std::clamp from <algorithm>) ---
        // Clamp all configurations to 0.0 to prevent negative distance logic errors
        const float validatedMin = std::max(0.0f, rawMin);
        const float validatedMax = std::max(validatedMin, rawMax);
        const float validatedBase = std::clamp(rawBase ,validatedMin, validatedMax);

        const float compressRatio = std::max(rawCompressRatio, 0.0f);
        const float expandRatio = std::max(rawExpandRatio, 0.0f);

        // --- COMPRESS METRICS ---
        const float compressCapacity = validatedBase * compressRatio;
        const float compressSolidify = std::max(0.0f, validatedBase - compressCapacity);

        // --- STORE PRE-COMPUTE ---
        preComputeMetrics.compressCapacities.push_back(compressCapacity);
        preComputeMetrics.compressSolidifies.push_back(compressSolidify);
        preComputeMetrics.expandRatios.push_back(expandRatio);
        preComputeMetrics.baseDistances.push_back(validatedBase);
        preComputeMetrics.maxDistances.push_back(validatedMax);
        preComputeMetrics.minDistances.push_back(validatedMin);
        preComputeMetrics.accumulateBaseDistance += validatedBase;
        preComputeMetrics.accumulateCompressSolidify += compressSolidify;
        preComputeMetrics.accumulateExpandRatio += expandRatio;

        // -- INITIALIZE THE SEGMENT DEFAULT VALUE --
        segments.push_back({name, validatedBase,0.0f, validatedBase});
    }

    const bool processingCompression = validatedInputDistance < preComputeMetrics.accumulateBaseDistance;

    return std::make_tuple(segments,preComputeMetrics, processingCompression);
};
```
### Main Sample

```cpp
int main()
{
    std::vector<DiscadeltaSegmentConfig> segmentConfigs{
        {"1",200.0f, 0.7f, 0.1f ,0.0f, 100.0f},
        {"2",200.0f, 1.0f, 1.0f ,300.0f, 800.0f},
        {"3",150.0f, 0.0f, 2.0f, 0.0f, 200.0f},
        {"4",350.0f, 0.3f, 0.5f, 50.0f, 300.0f}
    };
    
    auto [segmentDistances, preComputeMetrics, processingCompression] = MakeDiscadeltaContext(segmentConfigs, 800.0f);


#pragma region //Compute Segment Base Distance

    if (processingCompression) {
        //compressing
        float cascadeCompressDistance = preComputeMetrics.inputDistance;
        float cascadeBaseDistance = preComputeMetrics.accumulateBaseDistance;
        float cascadeCompressSolidify = preComputeMetrics.accumulateCompressSolidify;

        for (size_t i = 0; i < segmentDistances.size(); ++i) {
            const float remainCompressDistance = cascadeCompressDistance - cascadeCompressSolidify;
            const float remainCompressCapacity = cascadeBaseDistance - cascadeCompressSolidify;
            const float& compressCapacity = preComputeMetrics.compressCapacities[i];
            const float& compressSolidify = preComputeMetrics.compressSolidifies[i];
            const float compressBaseDistance = (remainCompressDistance <= 0 || remainCompressCapacity <= 0 || compressCapacity <= 0? 0.0f:
            remainCompressDistance / remainCompressCapacity * compressCapacity) + compressSolidify;

            DiscadeltaSegment& segment = segmentDistances[i];
            segment.base = compressBaseDistance;
            segment.distance = compressBaseDistance; //overwrite pre compute

            cascadeCompressDistance -= compressBaseDistance;
            cascadeCompressSolidify -= compressSolidify;
            cascadeBaseDistance -= preComputeMetrics.baseDistances[i];
        }
    }
    else {
        //Expanding
        float cascadeExpandDistance = std::max(preComputeMetrics.inputDistance - preComputeMetrics.accumulateBaseDistance, 0.0f);
        float cascadeExpandRatio = preComputeMetrics.accumulateExpandRatio;

        if (cascadeExpandDistance > 0.0f) {
            for (size_t i = 0; i < segmentDistances.size(); ++i) {
                const float& expandRatio = preComputeMetrics.expandRatios[i];
                const float expandDelta = cascadeExpandRatio <= 0.0f || expandRatio <= 0.0f? 0.0f :
                cascadeExpandDistance / cascadeExpandRatio * expandRatio;

                segmentDistances[i].expandDelta = expandDelta;
                segmentDistances[i].distance += expandDelta; //add to precompute

                cascadeExpandDistance -= expandDelta;
                cascadeExpandRatio -= expandRatio;
            }
        }
    }

#pragma endregion //Compute Segment Base Distance


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

        total += res.distance;

        std::cout << std::fixed << std::setprecision(3)
                  << std::setw(2) << "|"
                  << std::setw(10) << (i + 1)
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",preComputeMetrics.compressSolidifies[i])
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",preComputeMetrics.compressCapacities[i])
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",res.base)
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",res.expandDelta)
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",res.distance)
                  << std::setw(2) << "|"
                  << '\n';
    }

        std::cout << std::string(123, '-') << '\n';
        std::cout << std::format("Total: {:.4f} (expected 800.0)\n", total);
        #pragma endregion //Print Result

        return 0;
    }
```

| Segment   | Compress Solidify | Compress Capacity | Compress Distance | Expand Delta | Distance |
   |-----------|-------------------|-------------------|-------------------|--------------|----------|
| 1         | 30.0000           | 70.0000           | 92.3913           | 0.0000       | 92.3913  |
| 2         | 0.0000            | 300.0000          | 267.3913          | 0.0000       | 267.3913 |
| 3         | 150.0000          | 0.0000            | 150.0000          | 0.0000       | 150.0000 |
| 4         | 210.0000          | 90.0000           | 290.2174          | 0.0000       | 290.2174 |

* **Total**: `92.3913` + `267.3913` + `150.0000` + `290.2174` = `800`

## Redistribute Clamped Delta
The current algorithm processes segments in a single forward pass, following the natural loop order.  
Each segment has no awareness of its siblings — it only sees the remaining space at its turn.  
When a segment hits its min/max limit, it takes its full guaranteed amount, forcing all following segments to absorb the entire adjustment.  
Earlier segments remain unaffected, even if they have higher priority or more flexibility.  
This forward-pass design can lead to unfair distribution when constraints are active: later segments may give up disproportionately more space.

To achieve proportional fairness (relist and reset pre-compute metrics of unconstrained segments and recalculate, regardless of order), we need a redistribution pass — ideally iterative or recursive — after detecting any clamping.


### Relist DiscadeltaSegment And Redistribute

The current **Pre-Compute Helper Method** is returning a copy/new instance of metrics and segments there we can't grab a point within a method, we will rework so it can.

* **Pre Compute Metrics Struct**: add vector to store Non-owning DiscadeltaSegment pointers.
```cpp
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

    // NEW: Non-owning pointers
    std::vector<DiscadeltaSegment*> segments;

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
    }
};
```

* **RAII Handler**: a handler for vector DiscadeltaSegment unique pointer
```cpp
using DiscadeltaSegmentsHandler = std::vector<std::unique_ptr<DiscadeltaSegment>>;
```

* **Pre-Compute Helper Method**: now the helper can safely transfer ownership to the caller.
```cpp
constexpr auto MakeDiscadeltaContext = [](const std::vector<DiscadeltaSegmentConfig>& configs, const float inputDistance) -> std::tuple<DiscadeltaSegmentsHandler, DiscadeltaPreComputeMetrics, bool>
{
    const float validatedInputDistance = std::max(0.0f, inputDistance);
    const size_t segmentCount = configs.size();

    DiscadeltaSegmentsHandler segments;
    segments.reserve(segmentCount);

    DiscadeltaPreComputeMetrics preComputeMetrics(segmentCount, validatedInputDistance);

    for (const auto& cfg : configs) {
        const auto& [name, rawBase, rawCompressRatio, rawExpandRatio, rawMin, rawMax] = cfg;

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
    }

    const bool processingCompression = validatedInputDistance < preComputeMetrics.accumulateBaseDistance;

    return { std::move(segments), std::move(preComputeMetrics), processingCompression };
};
```

### Underflow: Compression Redistribution 

When a segment hits a constraint, we recalculate the remaining layout recursively to ensure fair distribution.

#### The Compression Logic

We use a Recursive Safeguard. We track how many segments we tried to process vs. how many were actually flexible. If a segment was clamped, we trigger a recursion with the updated "remaining" distance and a smaller pool of segments.

```cpp
constexpr void RedistributeDiscadeltaCompressDistance(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    float cascadeCompressDistance = preComputeMetrics.inputDistance;
    float cascadeBaseDistance = preComputeMetrics.accumulateBaseDistance;
    float cascadeCompressSolidify = preComputeMetrics.accumulateCompressSolidify;

    DiscadeltaPreComputeMetrics nextLineMetrics(preComputeMetrics.segments.size(), cascadeCompressDistance);

    int recursiveSafeguardValue{0}; // Tracks flexible segments
    int recursiveSafeguardLimit{0}; // Tracks total segments in this pass

    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        DiscadeltaSegment* seg = preComputeMetrics.segments[i];
        if (seg == nullptr) break;

        const float remainDist = cascadeCompressDistance - cascadeCompressSolidify;
        const float remainCap = cascadeBaseDistance - cascadeCompressSolidify;
        const float& cap = preComputeMetrics.compressCapacities[i];
        const float& solidify = preComputeMetrics.compressSolidifies[i];
        const float& base = preComputeMetrics.baseDistances[i];

        // Proportional math with safety checks
        const float compressBaseDistance = (remainDist <= 0 || remainCap <= 0 || cap <= 0 ? 0.0f :
                                      remainDist / remainCap * cap) + solidify;

        // Apply MIN Constraint
        const float& min = preComputeMetrics.minDistances[i];
        const float clampedDist = std::max(compressBaseDistance, min);

        // If Clamped: Reduce the budget for the next pass
        // If Flexible: Add to the pool for the next pass
        if (compressBaseDistance != clampedDist || cap <= 0.0f) {
            nextLineMetrics.inputDistance -= clampedDist;
        }
        else {
            nextLineMetrics.accumulateBaseDistance += base;
            nextLineMetrics.accumulateCompressSolidify += solidify;
            nextLineMetrics.compressCapacities.push_back(cap);
            nextLineMetrics.compressSolidifies.push_back(solidify);
            nextLineMetrics.baseDistances.push_back(base);
            nextLineMetrics.minDistances.push_back(min);
            nextLineMetrics.segments.push_back(seg);
            recursiveSafeguardValue++;
        }

        seg->base = seg->distance = clampedDist;
        cascadeCompressDistance -= clampedDist;
        cascadeCompressSolidify -= solidify;
        cascadeBaseDistance -= base;
        recursiveSafeguardLimit++;
    }

    // Recursion ends when Value == Limit (No new segments hit constraints)
    if (recursiveSafeguardValue < recursiveSafeguardLimit) {
        RedistributeDiscadeltaCompressDistance(nextLineMetrics);
    }
}
```
### Overflow: Expansion Redistribution

Expansion follows a similar recursive principle but focuses on the max constraint. If a segment reaches its maximum allowed size, any leftover expandDelta is redistributed to the remaining segments.

#### The Expansion Logic
```cpp
constexpr void RedistributeDiscadeltaExpandDistance(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    float cascadeExpandDelta = std::max(preComputeMetrics.inputDistance - preComputeMetrics.accumulateBaseDistance, 0.0f);
    float cascadeExpandRatio = preComputeMetrics.accumulateExpandRatio;
    if (cascadeExpandDelta <= 0.0f) return;

    DiscadeltaPreComputeMetrics nextLineMetrics(preComputeMetrics.segments.size(), cascadeExpandDelta);
    int recursiveSafeguardValue{0}, recursiveSafeguardLimit{0};

    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        DiscadeltaSegment* seg = preComputeMetrics.segments[i];
        const float& base = preComputeMetrics.baseDistances[i];
        const float& ratio = preComputeMetrics.expandRatios[i];

        const float expandDelta = (cascadeExpandRatio <= 0.0f || ratio <= 0.0f) ? 0.0f :
                                      cascadeExpandDelta / cascadeExpandRatio * ratio;

        // Apply MAX Constraint
        const float maxDelta = std::max(0.0f, preComputeMetrics.maxDistances[i] - base);
        const float clampedDelta = std::min(expandDelta, maxDelta);

        if (clampedDelta != expandDelta || ratio <= 0.0f) {
            nextLineMetrics.inputDistance -= clampedDelta;
        } else {
            nextLineMetrics.accumulateBaseDistance += base;
            nextLineMetrics.accumulateExpandRatio += ratio;
            nextLineMetrics.expandRatios.push_back(ratio);
            nextLineMetrics.baseDistances.push_back(base);
            nextLineMetrics.maxDistances.push_back(preComputeMetrics.maxDistances[i]);
            nextLineMetrics.segments.push_back(seg);
            recursiveSafeguardValue++;
        }

        seg->expandDelta = clampedDelta;
        seg->distance = base + clampedDelta;
        cascadeExpandDelta -= clampedDelta;
        cascadeExpandRatio -= ratio;
        recursiveSafeguardLimit++;
    }

    // Add back base distance for absolute distance input in recursion
    nextLineMetrics.inputDistance += nextLineMetrics.accumulateBaseDistance;

    if (recursiveSafeguardValue < recursiveSafeguardLimit) {
        RedistributeDiscadeltaExpandDistance(nextLineMetrics);
    }
}
```

* **Main Sample**
```cpp
    std::vector<DiscadeltaSegmentConfig> segmentConfigs{
      {"1", 200.0f, 0.7f, 0.1f, 0.0f, 100.0f},
      {"2", 200.0f, 1.0f, 1.0f, 300.0f, 800.0f},
      {"3", 150.0f, 0.0f, 2.0f, 0.0f, 200.0f},
      {"4", 350.0f, 0.3f, 0.5f, 50.0f, 300.0f}};

    constexpr float rootDistance = 800.0f;
    auto [segmentDistances, preComputeMetrics, processingCompression] = MakeDiscadeltaContext(segmentConfigs, rootDistance);


    if (processingCompression) {

        RedistributeDiscadeltaCompressDistance(preComputeMetrics);
    }
    else {
        RedistributeDiscadeltaExpandDistance(preComputeMetrics);
    }
```

### Result Table (Pre-Clamped Base, rootDistance = 800)

| Segment   | Compress Solidify | Compress Capacity | Compress Distance | Expand Delta | Distance |
|-----------|-------------------|-------------------|-------------------|--------------|----------|
| 1         | 30.0000           | 70.0000           | 78.1250           | 0.0000       | 78.1250  |
| 2         | 0.0000            | 300.0000          | 300.0000          | 0.0000       | 300.0000 |
| 3         | 150.0000          | 0.0000            | 150.0000          | 0.0000       | 150.0000 |
| 4         | 210.0000          | 90.0000           | 271.8750          | 0.0000       | 271.8750 | 

**Important Note**: Developers may choose how to handle the `base` value in pre-computation.
- **Pre-clamped base**: (recommended, default): Clamp `base` to [min, max] before calculating metrics. This ensures logical consistency and aligns with modern standards (CSS Flexbox, Yoga). Use for fair, order-independent results.


- **Raw base**: (Unity-like): Use the original `base` for compression metrics (capacity/solidify). This reproduces Unity's behavior but can be less fair. Requires separate raw tracking:


- **Compression**: Use metrics.rawBaseDistances and metrics.accumulateRawBaseDistance if you want Unity mode.


- **Expansion**: Always use clamped metrics.baseDistances and metrics.accumulateBaseDistance.

```cpp
// --- COMPRESS METRICS ---
const float compressCapacity = rawBase * compressRatio;
const float compressSolidify = std::max (0.0f, rawBase - compressCapacity);

....

preComputeMetrics.rawBaseDistances.push_back(rawBase);
preComputeMetrics.accumulateRawBaseDistance += rawBase;

```



| Segment   | Compress Solidify | Compress Capacity | Compress Distance | Expand Delta | Distance |
|-----------|-------------------|-------------------|-------------------|--------------|----------|
| 1         | 60.0000           | 140.0000          | 85.7143           | 0.0000       | 85.7143  |
| 2         | 0.0000            | 200.0000          | 300.0000          | 0.0000       | 300.0000 |
| 3         | 150.0000          | 0.0000            | 150.0000          | 0.0000       | 150.0000 |
| 4         | 245.0000          | 105.000           | 264.2857          | 0.0000       | 264.2857 | 

### Expansion Result Table (rootDistance 1300)

| Segment   | Compress Solidify | Compress Capacity | Compress Distance | Expand Delta | Distance |
|-----------|-------------------|-------------------|-------------------|--------------|----------|
| 1         | 30.0000           | 70.0000           | 100.0000          | 0.0000       | 100.0000 |
| 2         | 0.0000            | 300.0000          | 300.0000          | 0.0000       | 300.0000 |
| 3         | 150.0000          | 0.0000            | 150.0000          | 0.0000       | 200.0000 |
| 4         | 210.0000          | 90.000            | 300.0000          | 0.0000       | 300.0000 | 

## Performance & Stability Summary

1. **Guaranteed Convergence**: The `recursiveSafeguardValue < recursiveSafeguardLimit` logic ensures termination as soon as a pass is stable. Since at least one segment is removed in every clamping pass, worst-case complexity is O(N).

2. **Memory Optimization**: Internal metric containers use `reserve(segmentCount)` to minimize allocations during pre-computation.

3. **Numerical Stability**: Cascade subtraction (`cascadeDistance -= allocated`) ensures rounding errors never accumulate — a final sum is always exactly the root distance.

### Next Chapter: Iterative Optimization
While recursion provides a mathematically perfect baseline, the next phase converts these recursive passes into a high-performance iterative loop to maximize CPU cache efficiency and real-time performance in UFox.

### Next Chapter: [Discadelta Algorithm Optimization](discadelta-algorithm-optimization.md)