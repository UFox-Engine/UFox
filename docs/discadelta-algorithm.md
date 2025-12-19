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

| Segment | Demand | Distance |
|---------|--------|----------|
| 1       | None   | 200      |
| 2       | None   | 200      |
| 3       | None   | 200      |
| 4       | None   | 200      |

![Discadelta: Simple equal partitioning (800px root → 4 segments of 200px each)](images/DiscadeltaDocImage01.jpg)

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
SegmentDistanceₙ root_distance / accumulateRatio * segmentRatioₙ
```

**Results**:

| Segment | Distance   |
|---------|------------|
| 1       | 22.222222  |
| 2       | 222.222222 |
| 3       | 444.444444 |
| 4       | 111.111111 |

**Total**: `799.999999` (rounds to `800` due to floating-point precision).

![Discadelta: Raw ratio demand example – ratios 0.1:1.0:2.0:0.5 → proportional sizes ~22|222|444|111](images/DiscadeltaDocImage02.jpg)

This demonstrates pure proportional sharing — small ratios get tiny shares, large ratios dominate.
