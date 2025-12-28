# Discadelta Algorithm 🦊 (Chapter 1)

## Overview
> **Discadelta** = **Distance** + **Cascade** + **Delta**

Discadelta is a 1D partitioning algorithm for dividing a line (width, height, edge length, etc.) into resizable segments. It powers the fit rect layout in the **UFox** engine's GUI/Editor system, providing a reliable method for linear splitting that prevents rounding errors.

### Core Components
- **Distance**: The total available linear space (root distance).
- **Cascade**: A method where the remainder of a calculation is passed to the next step to prevent rounding errors.
- **Delta**: The specific amount added to or subtracted from a segment to reach the target size.


## Theory
### Goal
Calculate the distance of an individual segment given the root distance.

### 1. Fair Share Scenario
The simplest form of partitioning where every segment receives an equal portion of the total distance.

**Method**:
* **rootDistance** = `800`
* **numSegments** = `4`
* **Formula**: $SegmentDistance = rootDistance / numSegments$

![Discadelta: Simple equal partitioning (800px root → 4 segments of 200px each)](images/DiscadeltaDocImage01.jpg)

Logically, each segment should get **200**. However, it becomes complex if each segment has its own **configuration** determining its proportional share.
### 2. Dynamic Segment Scenario (Base & Share Ratios Configuration)


**Key Definitions**:
1. **Base** Default distance configuration.
2. **Share Ratio** Normalized share portion among segments configuration.
3. **Accumulate Ratio** A Sum of all `shareRatioₙ`.

**Method**:
* **rootDistance** = `800`
* **Accumulate Ratio**: It's a sum of all `segmentRatioₙ` configuration.
* **Formula**: $SegmentDistance = rootDistance / accumulateRatio * segmentRatioₙ$

* **Configuration**:
    
    | Segment | Base   | Ratio |
    |---------|--------|-------|
    | 1       | 100    | 0.5   |
    | 2       | 150    | 1.0   |
    | 3       | 200    | 2.0   |
    | 4       | 50     | 0.5   |

* **Results**:
    
    | Segment | Distance           |
    |---------|--------------------|
    | 1       | 100 + 37.5 = 137.5 |
    | 2       | 150 + 75   = 225   |
    | 3       | 200 + 150  = 350   |
    | 4       | 50  + 37.5 = 87.5  |

* **Total**: `137.5` + `225` + `350` + `87.5` = `800`

![Discadelta: segment with base and share ratio](images/DiscadeltaDocImage02.jpg)

### 3. Solving Precision Loss (The Cascade)
When segments use ratios, floating-point math can result in small overflows or gaps (e.g., $800.000001$). Discadelta solves this by cascading the reduction of the remaining distance.

**The Cascading Logic**:
1.  **shareDeltaₙ**: $remainShareDistance / remainRatio \times segmentRatioₙ$
2.  **Update Distance**: $remainShareDistance = remainShareDistance - shareDeltaₙ$
3.  **Update Ratio**: $remainRatio = remainRatio - segmentRatioₙ$

**Method**:
* **rootDistance** = `800`
* **Prepare Begin Compute Phase**
    * **accumulateBaseDistance** = sum(`baseₙ`)
    * **accumulateShareRatio** = sum(`segmentRatioₙ`)
    * **remainShareRatio** = `accumulateShareRatio`
    * **remainBaseDistance** = `accumulateBaseDistance`
    * **remainShareDistance** = `rootDistance` - `accumulateBaseDistance`


* **Configuration**:
        
    | Segment | Base   | Ratio |
    |---------|--------|-------|
    | 1       | 100    | 0.3   |
    | 2       | 150    | 1.0   |
    | 3       | 70     | 1.0   |
    | 4       | 50     | 0.8   |

* **Results**:
        
    | Segment | rDistance / rRatio * Ratio  | shareDelta | Base +  Distance   |
    |---------|-----------------------------|------------|--------------------|
    | 1       | 300        /   3.1  *   0.3 | 29.032258  | 129.032258         |
    | 2       | 70.967742  /   2.8  *   1.0 | 96.774193  | 246.774193         |
    | 3       | 174.193549 /   1.8  *   1.0 | 96.774193  | 296.774193         |
    | 4       | 77.419356  /   0.8  *   0.8 | 77.419356  | 127.419356         |

* **Total**: `129.032258` + `246.774193` + `296.774193` + `127.419356` = `800`


## Dynamic Base Segment (Underflow Handling)
When the total base distance required by segments is greater than the root distance ($rootBase < accumulateBaseDistance$), the algorithm scales bases down proportionally.

**Key Definitions**:
1. **Solidify Base Distance**: The "unyielding" portion of a segment that acts as a hard anchor during compression.
2. **Compress Base Distance**: The "flexible" portion of the segment that the algorithm scales down first to fit the root distance.
3. **Accumulate Base Distance**: The sum of all base distances before compression.

### The Underflow Formula
For each segment $n$, the **Dynamic Base Distance** is calculated as:


**Method**:

* **Configuration**:
    
    | Segment | Base | compressRatio | shareRatio |
    |---------|------|---------------|------------|
    | 1       | 200  | 0.7           | 0.1        |
    | 2       | 300  | 1.0           | 1.0        |
    | 3       | 150  | 1.0           | 2.0        |
    | 4       | 250  | 0.3           | 0.5        |

* **Prepare Begin Compute Phase**
   * **compressBaseDistanceₙ** = `baseₙ` * `compressRatioₙ`
   * **solidifyBaseDistanceₙ** = `baseₙ` - `compressBaseDistanceₙ`
   * **accumulateBaseDistance** = sum(`baseₙ`)
   * **accumulateSolidifyBaseDistance** = sum(`solidifyBaseDistanceₙ`)
  
* **Set Remain Metrics for Cascading**
  * **remainShareDistance** = `rootDistance`
  * **remainBaseDistance** = `accumulateBaseDistance`
  * **remainSolidifyBaseDistance** = `accumulateSolidifyBaseDistance`
* **Formula**: $$dynamicBaseDistance = \left( \frac{remainShareDistance - remainSolidify}{remainBase - remainSolidify} \right) \times compressBase + solidifyBase$$

* **Pre-Compute Table**:
    
    | Metric                         | Value |
    |--------------------------------|-------|
    | accumulateBaseDistance         | 900   |
    | accumulateSolidifyBaseDistance | 235   |
    | remainShareDistance            | 800   |
    | remainBaseDistance             | 900   |
    | remainSolidifyBaseDistance     | 235   |


* **Dynamic Base Table**:

    | Segment | Solidify Distance | Compress Distance   | Dynamic Distance  |
    |---------|-------------------|---------------------|-------------------|
    | 1       | 60                | 140                 | 178.9474          |
    | 2       | 0                 | 300                 | 254.8872          |
    | 3       | 0                 | 150                 | 127.4436          |
    | 4       | 175               | 75                  | 238.7218          |


* **Total**: `178.9474` + `254.8872` + `127.4436` + `238.7218` = `800`

* **Cascade Table**:
    
    | Iterate   | remainShareDistance | dynamicBase | remainBaseDistance | base | remainSolidifyBaseDistance | solidify |
    |-----------|---------------------|-------------|--------------------|------|----------------------------|----------|
    | 0         | 800                 | -178.9474   | 900                | -200 | 235                        | -60      |
    | 1         | 621.0526            | -254.8872   | 700                | -300 | 175                        | -0       |
    | 2         | 366.1654            | -127.4436   | 400                | -150 | 175                        | -0       |
    | 3         | 238.7218            | -238.7218   | 250                | -250 | 175                        | -175     |

* **Remain**:
  * `remainShareDistance` = `0`,
  * `remainBaseDistance` = `0`,
  * `remainSolidifyBaseDistance` = `0`



## Code Sample (C++23)
Below is the implementation of the **Underflow Handling** scenario, ensuring the total distance exactly matches the root.

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

## Summary

Chapter 1 establishes the foundation for static partitioning. By using the Cascade method, Discadelta ensures that no matter how complex the ratios are, the sum of segments will always equal the root distance exactly.

### Next Chapter: [Discadelta Algorithm Constraints](discadelta-algorithm-constraints.md)