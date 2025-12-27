# Discadelta Algorithm 🦊
## Overview
> **Discadelta** = **Distance** + **Cascade** + **Delta**

Discadelta is a 1D partitioning algorithm for dividing a line (width, height, edge length, etc.) into resizable segments.  
It powers fit rect layout in UFox’s GUI/Editor system, but is also designed to be used anywhere in the engine that needs reliable, interactive linear splitting.

### Core Components
- **Distance**: The total available linear space (root distance).
- **Cascade**: A method where the remainder of a calculation is passed to the next step to prevent rounding errors.
- **Delta**: The specific amount added to or subtracted from a segment to reach the target size.

## Use Cases
- **Tools/Editor**
- **Procedural Generation**
- **Constraints**


## Theory
### Goal
Calculate the distance of an individual segment given the root distance.

### Fair Share Scenario (No Segment Configuration)

**Formula**:
> **rootDistance** = `800`

> **numSegments** = `4`

> **SegmentDistanceₙ** = `rootDistance` / `numSegments`

**Results**:

| Segment | Distance |
|---------|----------|
| 1       | 200      |
| 2       | 200      |
| 3       | 200      |
| 4       | 200      |

![Discadelta: Simple equal partitioning (800px root → 4 segments of 200px each)](images/DiscadeltaDocImage01.jpg)

Logically, each segment should get **200**. However, it becomes complex if each segment has its own **configuration** determining its proportional share.

### Dynamic Segment Scenario (Share Ratios Configuration)

**Configuration**:

| Segment | Ratio     |
|---------|-----------|
| 1       | 0.1       |
| 2       | 1.0       |
| 3       | 2.0       |
| 4       | 0.5       |

**Formula**:
> **rootDistance** = `800`

> **accumulateRatio** = `0.1` + `1.0` + `2.0` + `0.5` = `3.6`

> **SegmentDistanceₙ** = `rootDistance` / `accumulateRatio` * `segmentRatioₙ`

**Results**:

| Segment | Distance   |
|---------|------------|
| 1       | 22.222222  |
| 2       | 222.222222 |
| 3       | 444.444444 |
| 4       | 111.111111 |

> **Total**: `799.999999` (rounds to `800` due to floating-point precision).

![Discadelta: segment with share ratio](images/DiscadeltaDocImage02.jpg)

This demonstrates pure proportional sharing — small ratios get tiny shares, large ratios dominate.

In the next scenario, the segment uses a default distance.  
At a segment ratio of 0.0, the segment's distance falls back to this default.

### Base(default) and Share Ratio Configuration Scenario

Now each segment has two parts:
- **Base distance** — the default size that must be satisfied first.
- **Share ratio factor** — share of any **leftover** space after bases.

**Configuration**:

| Segment | Base   | Ratio |
|---------|--------|-------|
| 1       | 100    | 0.5   |
| 2       | 150    | 1.0   |
| 3       | 200    | 2.0   |
| 4       | 50     | 0.5   |

**Formula**:
> **rootDistance** = `800`

> **accumulateRatio** = `0.5` + `1.0` + `2.0` + `0.5` = `3.6`

> **accumulateBaseDistance** = `100` + `150` + `200` + `50` = `500`

> **remainShareDistance** = `rootDistance` - `accumulateBaseDistance` = `300`

> **shareDeltaₙ** = `remainShareDistance` / `accumulateRatio` * `segmentRatioₙ`

> **SegmentDistanceₙ** = `baseDistanceₙ` + `shareDeltaₙ`

**Results**:

| Segment | Distance           |
|---------|--------------------|
| 1       | 100 + 37.5 = 137.5 |
| 2       | 150 + 75   = 225   |
| 3       | 200 + 150  = 350   |
| 4       | 50  + 37.5 = 87.5  |

> **Total**: `137.5` + `225` + `350` + `87.5` = `800` 

 ![Discadelta: segment with base and share ratio](images/DiscadeltaDocImage03.jpg)

In this sample, the current configuration achieves a perfect sum of the total distance (800).

However, some configurations may lose precision or have overflow. The next scenario introduces a strategy
to address this. 

### Solving Lose Precision or Overflow Scenario

**Configuration**:

| Segment | Base   | Ratio |
|---------|--------|-------|
| 1       | 100    | 0.3   |
| 2       | 150    | 1.0   |
| 3       | 70     | 1.0   |
| 4       | 50     | 0.8   |

**Results** (using the previous method):

| Segment | Base + rDistance / aRatio * Ratio   | Distance   |
|---------|-------------------------------------|------------|
| 1       | 100  +   300     /   3.1    *   0.3 | 129.032258 |
| 2       | 150  +   300     /   3.1    *   1.0 | 246.774194 |
| 3       | 200  +   300     /   3.1    *   1.0 | 296.774194 |
| 4       | 50   +   300     /   3.1    *   0.8 | 127.419355 |

> **Total**: `129.032258` + `246.774194` + `296.774194` + `127.419355` = `800.000001` 
 
The current configuration with previous method produces `.000001` overflow.
Below is a strategy to solve this Scenario by cascading remain distance reduction.

> **shareDeltaₙ** = `remainShareDistance` / `accumulateRatio` * `segmentRatioₙ`
 
> **remainShareDistance** = `remainShareDistance` - `shareDeltaₙ`
 
> **accumulateRatio** = `accumulateRatio` - `segmentRatioₙ`

> **SegmentDistanceₙ** = `baseDistanceₙ` + `shareDeltaₙ`

**Results**:

| Segment | Base + rDistance / aRatio * Ratio   | shareDelta | Distance   |
|---------|-------------------------------------|------------|------------|
| 1       | 100 + 300        /   3.1    *   0.3 | 29.032258  | 129.032258 |
| 2       | 150 + 270.967742 /   2.8    *   1.0 | 96.774193  | 246.774193 |
| 3       | 200 + 174.193549 /   1.8    *   1.0 | 96.774193  | 296.774193 |
| 4       | 50  + 77.419356  /   0.8    *   0.8 | 77.419356  | 127.419356 |

> **Total**: `129.032258` + `246.774193` + `296.774193` + `127.419356` = `800`

The next scenario will introduce **Dynamic Base Segment** in case `accumulateBaseDistance` is greater than rootDistance.

### Dynamic Base Segment Scenario (Underflow Handling)
When `rootBase` < `accumulateBaseDistance`, scale down bases proportionally using `compressRatio`, `compressBaseDistance` and `solidifyBaseDistance`.

* **Accumulate Base Distance**: A sum of all elements base distance to be compressing.
* **Compress Ratio**: A normalized compressible distance configuration value.
* **Compress Base Distance**: This is the "flexible" portion of the segment's base size. When space is tight, the algorithm scales this portion down first to fit the root distance.
* **Solidify Base Distance**: This represents the "unyielding" portion of a segment. It acts as a hard anchor that the algorithm tries to protect during compression.


**Configuration**:

| Segment | Base | compressRatio | shareRatio |
|---------|------|---------------|------------|
| 1       | 200  | 0.7           | 0.1        |
| 2       | 300  | 1.0           | 1.0        |
| 3       | 150  | 1.0           | 2.0        |
| 4       | 250  | 0.3           | 0.5        |

**Formula**:

**Prepare Begin Compute Phase**

> **compressBaseDistanceₙ** = `baseₙ` * `compressRatioₙ`

> **solidifyBaseDistanceₙ** = `baseₙ` - `compressBaseDistanceₙ`

> **accumulateBaseDistance** = sum(`baseₙ`)

> **accumulateSolidifyBaseDistance** = sum(`solidifyBaseDistanceₙ`)

**Set Remain Metrics for Cascading**

> **remainShareDistance** = `rootDistance`

> **remainBaseDistance** = `accumulateBaseDistance`

> **remainSolidifyBaseDistance** = `accumulateSolidifyBaseDistance` 


**Compute Dynamic Base Distance and Cascading**

> Formula: **dynamicBaseDistanceₙ** = (`remainShareDistance` - `remainSolidifyBaseDistance`) / (`remainBaseDistance` - `remainSolidifyBaseDistance`) * `compressBaseDistanceₙ` + `solidifyBaseDistanceₙ`

```
for each segment n:
    currentShareDistance = remainShareDistance - remainSolidifyBaseDistance
    currentBaseCapacity = remainBaseDistance - remainSolidifyBaseDistance
    dynamicBaseDistanceₙ = currentShareDistance / currentBaseCapacity × compressBaseDistanceₙ + solidifyBaseDistanceₙ
    dynamicBaseDistanceₙ = max(0, dynamicBaseDistanceₙ)   // prevent negative optional

    // Cascade
    remainShareDistance -= dynamicBaseDistanceₙ
    remainSolidifyBaseDistance -= solidifyBaseDistanceₙ
    remainBaseDistance -= baseₙ
```
**Pre-Compute Table**:

| Metric                         | Value |
|--------------------------------|-------|
| accumulateBaseDistance         | 900   |
| accumulateSolidifyBaseDistance | 235   |
| remainShareDistance            | 800   |
| remainBaseDistance             | 900   |
| remainSolidifyBaseDistance     | 235   |


**Dynamic Base Table**:

| Segment | Solidify Distance | Compress Distance   | Dynamic Distance  |
|---------|-------------------|---------------------|-------------------|
| 1       | 60                | 140                 | 178.9474          |
| 2       | 0                 | 300                 | 254.8872          |
| 3       | 0                 | 150                 | 127.4436          |
| 4       | 175               | 75                  | 238.7218          |

> **Total**: `178.9474` + `254.8872` + `127.4436` + `238.7218` = `800`

**Cascade Table**

| Iterate | remainShareDistance | -dynamicBase | remainBaseDistance | -base | remainSolidifyBaseDistance | -solidify |
|---------|---------------------|--------------|--------------------|-------|----------------------------|-----------|
| 0       | 800                 | -178.9474    | 900                | -200  | 235                        | -60       |
| 1       | 621.0526            | -254.8872    | 700                | -300  | 175                        | -0        |
| 2       | 366.1654            | -127.4436    | 400                | -150  | 175                        | -0        |
| 3       | 238.7218            | -238.7218    | 250                | -250  | 175                        | -175      |

> **Remain**: 
> - `remainShareDistance` = `0`, 
> - `remainBaseDistance` = `0`, 
> - `remainSolidifyBaseDistance` = `0`

## Code Sample

Below is a standalone C++23 implementation of the **Dynamic Base Segment (Underflow Handling)** scenario — the most complex case.

```cpp
#include <iostream>
#include <vector>
#include <format>
#include <iomanip>

struct DiscadeltaSegment {
    float base;
    float delta;
    float value;
};

struct DiscadeltaSegmentConfig {
    float base;
    float compressRatio;
    float shareRatio;
};

int main()
{
    std::vector<DiscadeltaSegmentConfig> segmentConfigs{
        {200.0f, 0.7f, 0.1f},
        {300.0f, 1.0f, 1.0f},
        {150.0f, 1.0f, 2.0f},
        {250.0f, 0.3f, 0.5f}
    };

    // std::vector<DiscadeltaSegmentConfig> segmentConfigs{
    //         {100.0f, 1.0f, 1.0f},
    //         {100.0f, 1.0f, 1.0f},
    //         {150.0f, 1.0f, 2.0f},
    //         {150.0f, 1.0f, 1.5f}
    // };

    const size_t segmentCount = segmentConfigs.size();

    std::vector<DiscadeltaSegment> segmentDistances{};
    segmentDistances.reserve(segmentCount);

#pragma region // Prepare Compute Context
    constexpr float rootBase = 800.0f;

    std::vector<float> compressBaseDistances{};
    compressBaseDistances.reserve(segmentCount);

    std::vector<float> solidifyBaseDistances{};
    solidifyBaseDistances.reserve(segmentCount);

    std::vector<float> baseDistances{};
    baseDistances.reserve(segmentCount);

    std::vector<float> shareRatios{};
    shareRatios.reserve(segmentCount);

    float accumulateBaseDistance{0.0f};
    float accumulateSolidifyBaseDistance{0.0f};
    float accumulateShareRatio{0.0f};

    for (size_t i = 0; i < segmentCount; ++i) {
        const auto &[base, compressRatio, shareRatio] = segmentConfigs[i];

        //Calculate base proportion metrics
        const float compressBaseDistance = base * compressRatio;
        const float solidifyBaseDistance = base - compressBaseDistance;
        const float validatedBaseDistance = std::max(base, 0.0f); // no negative

        compressBaseDistances.push_back(compressBaseDistance);
        solidifyBaseDistances.push_back(solidifyBaseDistance);
        baseDistances.push_back(validatedBaseDistance);

        //Accumulate share proportion capacity metrics:
        accumulateBaseDistance += validatedBaseDistance;
        accumulateSolidifyBaseDistance += solidifyBaseDistance;

        DiscadeltaSegment segment{};
        segment.base = validatedBaseDistance;
        segment.value = validatedBaseDistance;
        segmentDistances.push_back(segment);

        shareRatios.push_back(shareRatio);
        accumulateShareRatio += shareRatio;
    }

    //Set Remain Metrics for Cascading
    float remainShareDistance = rootBase;
    float remainBaseDistance = accumulateBaseDistance;
    float remainSolidifyBaseDistance = accumulateSolidifyBaseDistance;

#pragma endregion //Prepare Compute Context

#pragma region //Compute Segment Base Distance

    if (rootBase < accumulateBaseDistance) {
        //compressing
        for (size_t i = 0; i < segmentCount; ++i) {
            const float currentShareDistance = remainShareDistance - remainSolidifyBaseDistance;
            const float currentBaseCapacity = remainBaseDistance - remainSolidifyBaseDistance;
            const float& currentCompressBase = compressBaseDistances[i];
            const float& currentSolidifyBase = solidifyBaseDistances[i];
            const float dynamicBaseDistance = currentShareDistance <= 0 || currentBaseCapacity <= 0 || currentCompressBase <= 0? 0.0f:
            std::max(0.0f, currentShareDistance / currentBaseCapacity * currentCompressBase + currentSolidifyBase);

            DiscadeltaSegment& segment = segmentDistances[i];
            segment.base = dynamicBaseDistance;
            segment.value = dynamicBaseDistance;

            remainShareDistance -= dynamicBaseDistance;
            remainSolidifyBaseDistance -= currentSolidifyBase;
            remainBaseDistance -= baseDistances[i];
        }
    }
    else {
        //Expanding
        float currentRemainShareDistance = std::max(remainShareDistance - accumulateBaseDistance, 0.0f);
        float currentRemainShareRatio = accumulateShareRatio;

        if (currentRemainShareDistance > 0.0f) {
            for (size_t i = 0; i < segmentCount; ++i) {
                const float& shareRatio = shareRatios[i];
                const float shareDelta = currentRemainShareRatio <= 0.0f || shareRatio <= 0.0f? 0.0f :
                currentRemainShareDistance / currentRemainShareRatio * shareRatio;

                segmentDistances[i].delta = shareDelta;
                segmentDistances[i].value += shareDelta;

                currentRemainShareDistance -= shareDelta;
                currentRemainShareRatio -= shareRatio;
            }
        }
    }

#pragma endregion //Compute Segment Base Distance


#pragma region //Print Result
    std::cout << "\n=== Dynamic Base Segment (Underflow Handling) ===\n";
    std::cout << std::format("Root distance: {:.4f}\n\n", rootBase);

  std::cout << std::string(123, '-') << '\n';
    // Table header
    std::cout << std::left
              << std::setw(2) << "|"
              << std::setw(10) << "Segment"
              << std::setw(2) << "|"
              << std::setw(20) << "Solidify Distance"
              << std::setw(2) << "|"
              << std::setw(20) << "Compress Distance"
              << std::setw(2) << "|"
              << std::setw(20) << "Dynamic Distance"
              << std::setw(2) << "|"
              << std::setw(20) << "Expand Delta"
              << std::setw(2) << "|"
              << std::setw(20) << "Scaled Distance"
              << std::setw(2) << "|"
              << '\n';

    std::cout << std::string(123, '-') << '\n';
    float total{0.0f};
    for (size_t i = 0; i < segmentCount; ++i) {
        const auto& res = segmentDistances[i];

        total += res.value;

        std::cout << std::fixed << std::setprecision(3)
                  << std::setw(2) << "|"
                  << std::setw(10) << (i + 1)
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",solidifyBaseDistances[i])
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",compressBaseDistances[i])
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",res.base)
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",res.delta)
                  << std::setw(2) << "|"
                  << std::setw(20) << std::format("Total: {:.4f}",res.value)
                  << std::setw(2) << "|"
                  << '\n';
    }

        std::cout << std::string(123, '-') << '\n';
        std::cout << std::format("Total: {:.4f} (expected 800.0)\n", total);
        #pragma endregion //Print Result

        return 0;
    }
```

## Chapter Summary

Discadelta provides a reliable, configuration-driven way to partition a 1D space into resizable segments.  
It handles fair share, proportional ratios, base + ratio mixed cases, precision/overflow fixes, and underflow scaling — all while ensuring the total distance exactly matches the root.

This chapter completes the **static partitioning** foundation.  
The next chapter explores **min/max constraints** to clamp individual segments while preserving the overall layout integrity.

### Next Chapter: [Discadelta Algorithm Constraints](discadelta-algorithm-constraints.md)