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

> **remainingDistance** = `rootDistance` - `accumulateBaseDistance` = `300`

> **shareDeltaₙ** = `remainingDistance` / `accumulateRatio` * `segmentRatioₙ`

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

> **shareDeltaₙ** = `remainingDistance` / `accumulateRatio` * `segmentRatioₙ`
 
> **remainingDistance** = `remainingDistance` - `shareDeltaₙ`
 
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

**Key Behaviors in This Scenario:**
- Clamp `remainingDistance` to `0`.
- `shareDeltaₙ` calculation can be ignored and returns `0`.
- Normalize `segmentBaseₙ` (`baseShareRatioₙ`).
- Cascading strategy to Solving Lose Precision or Overflow

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

> **remainBaseDistance** = `rootDistance`

> **reduceDistanceₙ** = `segmentBaseₙ` * (`1` - `reduceRatioₙ`)

> **baseShareDistanceₙ** = `segmentBaseₙ` * `reduceRatioₙ`

> **baseShareRatioₙ** = `baseShareDistanceₙ` / `100`

> **accumulateReduceDistance** = `accumulateReduceDistance` + `reduceDistanceₙ`

> **accumulateBaseShareRatio** = `accumulateBaseShareRatio` + `baseShareRatioₙ`

**Compute Base Distance**

> **remainReduceDistance** = `remainBaseDistance` - `accumulateReduceDistance`

> **baseDistanceₙ** = `remainReduceDistance` / `accumulateBaseShareRatio` * `baseShareRatioₙ` + `reduceDistanceₙ`

**Cascading strategy**

> **accumulateReduceDistance** = `accumulateReduceDistance` - `reduceDistanceₙ`

> **remainBaseDistance** = `remainBaseDistance` - `baseDistanceₙ`

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

