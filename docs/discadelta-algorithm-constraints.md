# Discadelta Algorithm Constraints 🦊 (Chapter 2)

## Overview
The previous chapter implemented the core Discadelta algorithm: Solving Precision Loss, underflow/overflow handling via **Compress Base Distance** and **Expand Delta**.

This chapter adds **min** and **max** constraints per segment to ensure individual parts stay within acceptable size bounds.

Min/Max constraints usually break "perfect" proportional sharing because they force a segment to stop growing or shrinking, leaving the "**Expand Delta**" or "**Compress Base Distance**" to be redistributed among the remaining segments.

The challenge is:
- After applying constraints, the total must still exactly equal the root distance.
- We must distribute "clamped" space fairly among unconstrained segments.

## Core Goals
1. **Respect min/max** per segment
2. **Preserve total sum** = root distance
3. **Distribute excess/deficit fairly** using ratios
4. **Handle multiple clamping cases** (some segments at min, some at max, some free)
5. **Remain numerically stable** (avoid catastrophic cancellation)

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


## Theory
### Validate Min, Max and Base

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

4. **Clamp Base**
   - To ensure the preferred `base` value is logically consistent with the constraints, we clamp it to `[min, max]` upfront.  
     This is a deliberate design choice: it doesn't seem right to have a config where `base` is bigger than `max` or lower than `min` — it would mean the "ideal" size is impossible from the start.  
     Pre-clamping the `base` prevents invalid intermediate states and aligns with modern layout standards (e.g., CSS Flexbox clamps flex-basis to min/max before distribution).  
     Use `std::clamp` for this, as max and min are already validated. If needed for older compilers, use an if-statement equivalent.

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

### Redistribute Clamped Delta (Compression only)
The current algorithm processes segments in a single forward pass, following the natural loop order.  
Each segment has no awareness of its siblings — it only sees the remaining space at its turn.  
When a segment hits its min/max limit, it takes its full guaranteed amount, forcing all following segments to absorb the entire adjustment.  
Earlier segments remain unaffected, even if they have higher priority or more flexibility.  
This forward-pass design can lead to unfair distribution when constraints are active: later segments may give up disproportionately more space.

To achieve proportional fairness (relist and reset pre-compute metrics of unconstrained segments and recalculate, regardless of order), we need a redistribution pass — ideally iterative or recursive — after detecting any clamping.


#### Relist DiscadeltaSegment And Redistribute

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

* **Redistribute Compressed Metrics**: When a segment hits a constraint, we recalculate the remaining layout recursively to ensure fair distribution.
```cpp
constexpr void RedistributeDiscadeltaCompressDistance(const DiscadeltaPreComputeMetrics& preComputeMetrics) {
    // --- SETUP CASCADE VARIABLES ---
    float cascadeCompressDistance = preComputeMetrics.inputDistance;
    float cascadeBaseDistance = preComputeMetrics.accumulateBaseDistance;
    float cascadeCompressSolidify = preComputeMetrics.accumulateCompressSolidify;

    DiscadeltaPreComputeMetrics nextLineMetrics(preComputeMetrics.segments.size(), preComputeMetrics.inputDistance);


    int recursiveSafeguardValue = 0;
    int recursiveSafeguardLimit = 0;

    for (size_t i = 0; i < preComputeMetrics.segments.size(); ++i) {
        // --- PROPORTIONAL CALCULATION ---
        DiscadeltaSegment* seg = preComputeMetrics.segments[i];
        if (seg == nullptr) break;

        const float remainCompressDistance = cascadeCompressDistance - cascadeCompressSolidify ;
        const float remainCompressCapacity = cascadeBaseDistance - cascadeCompressSolidify ;
        const float& compressCapacity = preComputeMetrics.compressCapacities[i];
        const float& compressSolidify = preComputeMetrics.compressSolidifies[i];
        const float base = preComputeMetrics.baseDistances[i];

        const float compressBaseDistance = (remainCompressDistance <= 0 || remainCompressCapacity <= 0 || compressCapacity <= 0 ? 0.0f :
                                      remainCompressDistance / remainCompressCapacity * compressCapacity) + compressSolidify;

        // --- APPLY CONSTRAINT ---
        const float min = preComputeMetrics.minDistances[i];
        const float clampedCompressBaseDistance = std::max(compressBaseDistance, min);

        // --- SETUP RECURSIVE METRICS ---
        //Exclude the clamped segment or reach 0 compressCapacity.
        //Reduce Metrics input by clamped segment distance amount 
        if (compressBaseDistance != clampedCompressBaseDistance || compressCapacity <= 0.0f) {
            nextLineMetrics.inputDistance -= clampedCompressBaseDistance;
        }
        else {
            nextLineMetrics.accumulateBaseDistance += base;
            nextLineMetrics.accumulateCompressSolidify += compressSolidify;
            nextLineMetrics.accumulateExpandRatio += preComputeMetrics.expandRatios[i];

            nextLineMetrics.compressCapacities.push_back(compressCapacity);
            nextLineMetrics.compressSolidifies.push_back(compressSolidify);
            nextLineMetrics.baseDistances.push_back(base);
            nextLineMetrics.maxDistances.push_back(preComputeMetrics.maxDistances[i]);
            nextLineMetrics.expandRatios.push_back(preComputeMetrics.expandRatios[i]);
            nextLineMetrics.minDistances.push_back(preComputeMetrics.minDistances[i]);
            nextLineMetrics.segments.push_back(seg);

            recursiveSafeguardValue++;
        }


        seg->base = clampedCompressBaseDistance;
        seg->distance = clampedCompressBaseDistance;

        cascadeCompressDistance -= clampedCompressBaseDistance;
        cascadeCompressSolidify -= compressSolidify;
        cascadeBaseDistance -= base;
        recursiveSafeguardLimit++;
    }

    if (recursiveSafeguardValue < recursiveSafeguardLimit) {
        RedistributeDiscadeltaCompressDistance(nextLineMetrics);
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
        //Expanding
        float cascadeExpandDistance = std::max(preComputeMetrics.inputDistance - preComputeMetrics.accumulateBaseDistance, 0.0f);
        float cascadeExpandRatio = preComputeMetrics.accumulateExpandRatio;

        if (cascadeExpandDistance > 0.0f) {
            for (size_t i = 0; i < segmentDistances.size(); ++i) {
                const float& expandRatio = preComputeMetrics.expandRatios[i];
                const float expandDelta = cascadeExpandRatio <= 0.0f || expandRatio <= 0.0f? 0.0f :
                cascadeExpandDistance / cascadeExpandRatio * expandRatio;

                segmentDistances[i]->expandDelta = expandDelta;
                segmentDistances[i]->distance += expandDelta; //add to precompute

                cascadeExpandDistance -= expandDelta;
                cascadeExpandRatio -= expandRatio;
            }
        }
    }
```

### Result Table (Pre-Clamped Base)
| Segment   | Compress Solidify | Compress Capacity | Compress Distance | Expand Delta | Distance |
|-----------|-------------------|-------------------|-------------------|--------------|----------|
| 1         | 30.0000           | 70.0000           | 78.1250           | 0.0000       | 78.1250  |
| 2         | 0.0000            | 300.0000          | 300.0000          | 0.0000       | 300.0000 |
| 3         | 150.0000          | 0.0000            | 150.0000          | 0.0000       | 150.0000 |
| 4         | 210.0000          | 90.0000           | 271.8750          | 0.0000       | 271.8750 | 

**Important Note**: Developers may choose not to pre-clamp the base in the pre-compute step. The result will differ significantly.

> const float baseVal = rawBase; // no clamping (Unity-like behavior)


| Segment   | Compress Solidify | Compress Capacity | Compress Distance | Expand Delta | Distance |
|-----------|-------------------|-------------------|-------------------|--------------|----------|
| 1         | 60.0000           | 140.0000          | 85.7143           | 0.0000       | 85.7143  |
| 2         | 0.0000            | 200.0000          | 300.0000          | 0.0000       | 300.0000 |
| 3         | 150.0000          | 0.0000            | 150.0000          | 0.0000       | 150.0000 |
| 4         | 245.0000          | 105.000           | 264.2857          | 0.0000       | 264.2857 | 

## Summary
With this implementation, Discadelta successfully handles constraints in the compression loop using a recursive redistribution method.  
This demonstration solves the constraint scenario straightforwardly, but it is not performance-friendly due to reliance on recursion.

This document does not showcase recursive constraints for the expansion loop. We will continue with a more performant approach to redistributing constrained segments in future chapters.