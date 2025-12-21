# Discadelta Algorithm 🦊
## Overview
> **Discadelta** = **Distance** + **Cascade** + **Delta**

Discadelta is a 1D partitioning algorithm for dividing a line (width, height, edge length, etc.) into resizable segments.  
It powers fit rect layout in UFox’s GUI/Editor system, but is also designed to be used anywhere in the engine that needs reliable, interactive linear splitting.

### Core Components
- **Distance**: One-dimensional positive value.
- **Cascade**: Additive cascading offset for the next segment start distance.
- **Delta**: Filling/sharing leftover distance to each segment.

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
When `accumulateBaseDistance` exceeds `rootDistance`, the algorithm **uses a dynamic base** to proportionally scale down allocations to fit within `rootDistance`.  
This leverages the `reduceRatio` configuration to maintain proportional sharing among segments.

#### Key Behaviors in This Scenario:
- Treats `reduceDistance` as a **solidified base distance** that resists scaling.
- Normalizes `segmentBase<sub>n</sub>` (`baseShareRatio<sub>n</sub>`) for scaling.
- Applies a **cascading strategy** to solve loss of precision or overflow.
- Calculates **share delta** if the remaining share distance is greater than 0.



**Configuration**:

| Segment | Base | reduceRatio | shareRatio |
|---------|------|-------------|------------|
| 1       | 200  | 0.7         | 0.1        |
| 2       | 300  | 1.0         | 1.0        |
| 3       | 150  | 1.0         | 2.0        |
| 4       | 250  | 0.3         | 0.5        |

**Formula**:

**Prepare Compute Context**

> **rootDistance** = `800`

> **remainShareDistance** = `rootDistance`

> **reduceDistanceₙ** = `segmentBaseₙ` * (`1` - `reduceRatioₙ`)

> **baseShareDistanceₙ** = `segmentBaseₙ` * `reduceRatioₙ`

> **baseShareRatioₙ** = `baseShareDistanceₙ` / `100`

> **accumulateReduceDistance** = `accumulateReduceDistance` + `reduceDistanceₙ`

> **accumulateBaseShareRatio** = `accumulateBaseShareRatio` + `baseShareRatioₙ`

**Compute Base Distance**

> **remainReduceDistance** = `remainShareDistance` - `accumulateReduceDistance`

> **baseDistanceₙ** = `remainReduceDistance` / `accumulateBaseShareRatio` * `baseShareRatioₙ` + `reduceDistanceₙ`

**Cascading strategy**

> **accumulateReduceDistance** = `accumulateReduceDistance` - `reduceDistanceₙ`

> **remainShareDistance** = `remainShareDistance` - `baseDistanceₙ`

> **accumulateBaseShareRatio** = `accumulateBaseShareRatio` - `baseShareRatioₙ`

> **SegmentDistanceₙ** = `baseDistanceₙ` + `(0) shareDeltaₙ`

**Results** (using cascading strategy):

| Segment | reduceDistance | baseShareDistance | baseShareRatio | remainReduceDistance / accumulateBaseShareRatio * baseShareRatioₙ | baseDistanceₙ   |
|---------|----------------|-------------------|----------------|-------------------------------------------------------------------|-----------------|
| 1       | 60             | 140               | 1.4            | 565 / 6.65 * 1.4 ≈ 118.947368                                     | 178.947368      |
| 2       | 0              | 300               | 3.0            | 446.052632 / 5.25 * 3.0 ≈ 254.887218                              | 254.887218      |
| 3       | 0              | 150               | 1.5            | 191.165414 / 2.25 * 1.5 ≈ 127.443609                              | 127.443609      |
| 4       | 175            | 75                | 0.75           | 63.721805 / 0.75 * 0.75 = 63.721805                               | 238.721805      |

> **Total**: `178.947368` + `254.887218` + `127.443609` + `238.721805` = `800`


## Code Sample

Below is a standalone C++23 implementation of the **Dynamic Base Segment (Underflow Handling)** scenario — the most complex case.

```cpp
#include <iostream>
#include <vector>
#include <format>
#include <iomanip>  // for std::setw, std::fixed, std::setprecision

struct DiscadeltaSegment {
    float base;
    float delta;
    float value;
};

struct DiscadeltaSegmentConfig {
    float base;
    float reduceRatio;
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

    const size_t segmentCount = segmentConfigs.size();

    std::vector<DiscadeltaSegment> segmentDistances{};
    segmentDistances.reserve(segmentCount);

#pragma region // Prepare Compute Context
    constexpr float rootBase = 800.0f;

    std::vector<float> reduceDistances{};
    reduceDistances.reserve(segmentCount);

    std::vector<float> baseShareDistances{};
    baseShareDistances.reserve(segmentCount);

    std::vector<float> baseShareRatios{};
    baseShareRatios.reserve(segmentCount);

    std::vector<float> shareRatios{};
    shareRatios.reserve(segmentCount);

    float accumulateReduceDistance{0.0f};
    float accumulateBaseShareRatio{0.0f};

    float accumulateBaseDistance{0.0f};
    float accumulateShareRatio{0.0f};

    for (size_t i = 0; i < segmentCount; ++i) {
        const auto &[base, reduceRatio, shareRatio] = segmentConfigs[i];

        const float reduceDistance = base * (1.0f - reduceRatio);
        const float baseShareDistance = base * reduceRatio;
        const float baseShareRatio = baseShareDistance / 100.0f;

        accumulateReduceDistance += reduceDistance;
        accumulateBaseShareRatio += baseShareRatio;

        accumulateBaseDistance += base;
        accumulateShareRatio += shareRatio;

        reduceDistances.push_back(reduceDistance);
        baseShareDistances.push_back(baseShareDistance);
        baseShareRatios.push_back(baseShareRatio);

        shareRatios.push_back(shareRatio);
    }
#pragma endregion //Prepare Compute Context

#pragma region //Compute Segment Base Distance

    float remainShareDistance = rootBase;
    float total{0.0f};

    for (size_t i = 0; i < segmentCount; ++i) {
        DiscadeltaSegment segment{};

        if (rootBase <= accumulateBaseDistance) {
            const float remainReduceDistance = remainShareDistance - accumulateReduceDistance;
            const float shareRatio = baseShareRatios[i];
            const float baseDistance = remainReduceDistance <= 0.0f || accumulateBaseShareRatio <= 0.0f || shareRatio <= 0.0f ? 0.0f :
                remainReduceDistance / accumulateBaseShareRatio * shareRatio + reduceDistances[i];

            accumulateReduceDistance -= reduceDistances[i];
            remainShareDistance -= baseDistance;
            accumulateBaseShareRatio -= shareRatio;

            segment.base = baseDistance;
            segment.value = baseDistance;
            total += baseDistance;
        }
        else {
            const float baseDistance = segmentConfigs[i].base;
            segment.base = baseDistance;
            segment.value = baseDistance;
            remainShareDistance -= baseDistance;
        }

        segmentDistances.push_back(segment);
    }

#pragma endregion //Compute Segment Base Distance

#pragma region //Compute Segment Delta

    if (remainShareDistance > 0.0f) {
        float remainShareRatio = accumulateShareRatio;

        for (size_t i = 0; i < segmentCount; ++i) {
            const float shareDelta = remainShareDistance / remainShareRatio * shareRatios[i];

            segmentDistances[i].delta = shareDelta;
            segmentDistances[i].value += shareDelta;

            remainShareDistance -= shareDelta;
            remainShareRatio -= shareRatios[i];
        }
    }

#pragma endregion //Compute Segment Delta

#pragma region //Print Result
    std::cout << "\n=== Dynamic Base Segment (Underflow Handling) ===\n";
    std::cout << std::format("Root distance: {:.1f}\n\n", rootBase);

    // Table header
    std::cout << std::left
              << std::setw(8) << "Segment"
              << std::setw(14) << "reduceDist"
              << std::setw(16) << "baseShareDist"
              << std::setw(16) << "baseShareRatio"
              << std::setw(14) << "base"
              << std::setw(14) << "delta"
              << std::setw(14) << "distance"
              << '\n';

    std::cout << std::string(100, '-') << '\n';

    for (size_t i = 0; i < segmentCount; ++i) {
        const auto& seg = segmentConfigs[i];
        const auto& res = segmentDistances[i];

        const float reduceDistance = seg.base * (1.0f - seg.reduceRatio);
        const float baseShareDistance = seg.base * seg.reduceRatio;
        const float baseShareRatio = baseShareDistance / 100.0f;

        std::cout << std::fixed << std::setprecision(3)
                  << std::setw(8) << (i + 1)
                  << std::setw(14) << reduceDistance
                  << std::setw(16) << baseShareDistance
                  << std::setw(16) << baseShareRatio
                  << std::setw(14) << res.base
                  << std::setw(14) << res.delta
                  << std::setw(14) << res.value
                  << '\n';
    }

    std::cout << std::string(100, '-') << '\n';
    std::cout << std::format("Total: {:.3f} (expected 800.0)\n\n", total);
#pragma endregion //Print Result

    return 0;
}
```

