# Discadelta Algorithm 🦊
## What is Discadelta?
**Discadelta** = **Distance** + **Cascade** + **Delta**

- **Distance**: One-dimensional positive value.
- **Cascade**: Front segment consumes remaining distance.
- **Delta**: Filling/sharing leftover distance to each segment.

Discadelta is a 1D partitioning algorithm for dividing a line (width, height, edge length, etc.) into resizable segments.  
It powers fit rect layout in UFox’s GUI/Editor system, but is also designed to be used anywhere in the engine that needs reliable, interactive linear splitting.

## Use Cases
- **Tools/Editor**
- **Procedural Generation**
- **Constraints**

## Theory
### Goal
Calculate the distance of an individual segment given the root distance.

### Fair Share Scenario (No Demands)
**Root distance**: `800`  
**Formula**:

```
root_distance / num_segments = 800 / 4 = 200
```
**Results**:

| Segment | Distance |
|---------|----------|
| 1       | 200      |
| 2       | 200      |
| 3       | 200      |
| 4       | 200      |

![Discadelta: Simple equal partitioning (800px root → 4 segments of 200px each)](images/DiscadeltaDocImage01.jpg)

Logically, each segment should get **200**. However, it becomes complex if each segment has its own **demand** determining its proportional share.

### Proportional Demand Scenario (Ratios Only)
**Root distance**: `800` (no fixed sizes)

**Ratios**:

| Segment | Ratio     |
|---------|-----------|
| 1       | 0.1       |
| 2       | 1.0       |
| 3       | 2.0       |
| 4       | 0.5       |

**Accumulated ratio** (`accumulateRatio`): `0.1 + 1.0 + 2.0 + 0.5 = 3.6`

**Formula**:

```
SegmentDistanceₙ = root_distance / accumulateRatio * segmentRatioₙ
```

**Results**:

| Segment | Distance   |
|---------|------------|
| 1       | 22.222222  |
| 2       | 222.222222 |
| 3       | 444.444444 |
| 4       | 111.111111 |

**Total**: `799.999999` (rounds to `800` due to floating-point precision).

![Discadelta: segment with share ratio](images/DiscadeltaDocImage02.jpg)

This demonstrates pure proportional sharing — small ratios get tiny shares, large ratios dominate.

In the next scenario, the segment uses a default distance.  
At a segment ratio of 0.0, the segment's distance falls back to this default.

### Base(default) + Share Ratio Demand Scenario

Now each segment has two parts:
- **Base distance** — the default size that must be satisfied first.
- **Share ratio factor** — share of any **leftover** space after bases.

**Scenario** (root distance **800**):

| Segment | Base Demand | Ratio |
|---------|-------------|-------|
| 1       | 100         | 0.5   |
| 2       | 150         | 1.0   |
| 3       | 200         | 2.0   |
| 4       | 50          | 0.5   |

**Accumulated ratio** (`accumulateRatio`): `0.5` + `1.0` + `2.0` + `0.5` = `3.6`

**Accumulate Base Distance** (`accumulateBaseDistance`): `100` + `150` + `200` + `50` = `500`

**Remaining Distance** (`remainingDistance`): `800(root_distance)` - `500(accumulateBaseDistance)` = 300

**Formula**:

```
shareDeltaₙ = remainingDistance / accumulateRatio * segmentRatioₙ
SegmentDistanceₙ = baseDistanceₙ + shareDeltaₙ
```

![Discadelta: segment with base and share ratio](images/DiscadeltaDocImage03.jpg)

**Results**:

| Segment | Distance           |
|---------|--------------------|
| 1       | 100 + 37.5 = 137.5 |
| 2       | 150 + 75   = 225   |
| 3       | 200 + 150  = 350   |
| 4       | 50  + 37.5 = 87.5  |

**Total**: `800` 