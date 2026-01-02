# Discadelta: Dynamic Placing Pass 🦊 (Chapter 4)

## Overview
In the previous chapters, we solved the "How big?" problem. Every segment now has a finalized `distance` that respects its constraints and fair-share proportions. However, in a high-performance engine like UFox, knowing the size is only half the battle. We need to know the **position**.

This chapter introduces the third and final stage of the pipeline: **The Placing Pass**. By decoupling size from position, we gain the ability to reorder elements visually without ever touching the complex scaling math again.

## Anatomy of the Placing Pass
To support flexible positioning, we use two key members in our data structures:

* **`size_t order`**: This defines the visual sequence (`0, 1, 2...`). Because we use `size_t` (an unsigned type), it is mathematically impossible for the order to be below `0`, providing built-in safety for our layout logic.
* **`float offset`**: This is the final world-space `X` or `Y` coordinate where the segment begins.

### Updated Data Structs
```cpp
struct DiscadeltaSegmentConfig {
    // ... previous members ...
    size_t order; // Defined position in the visual sequence
};

struct DiscadeltaSegment {
    // ... previous members ...
    float offset; // Resolved in Pass 3
    size_t order; // Transferred from Config
};
```

### Pre-Compute Transfer

```cpp
// Inside MakeDiscadeltaContext loop:
const auto& [name, rawBase, rawCR, rawER, rawMin, rawMax, rawOrder] = configs[i];

// --- CREATE OWNED SEGMENT ---
auto seg = std::make_unique<DiscadeltaSegment>();
seg->name = name;
seg->order = rawOrder;
seg->base = baseVal;
seg->distance = baseVal;
seg->expandDelta = 0.0f;

preComputeMetrics.segments.push_back(seg.get());
segments.push_back(std::move(seg));

```

### Helper Method
#### Debugger
```cpp
void Debugger(const DiscadeltaSegmentsHandler& segmentDistances, const DiscadeltaPreComputeMetrics &preComputeMetrics) {
    std::cout <<"=== Discadelta Layout: Metrics & Final Distribution ===" << std::endl;
    std::cout << std::format("Input distance: {}", preComputeMetrics.inputDistance)<< std::endl;

    // Table header
    std::cout << std::left
              << "|"
              << std::setw(10) << "Segment"
              << "|"
              << std::setw(20) << "Compress Distance"
              << "|"
              << std::setw(15) << "Expand Delta"
              << "|"
              << std::setw(15) << "Scaled Distance"
              << "|"
              << std::setw(15) << "Placing Order"
              << "|"
              << std::setw(15) << "Placing Offset"
              << "|"
              << std::endl;

    std::cout << std::left
                   << "|"
                   << std::string(10, '-')
                   << "|"
                   << std::string(20, '-')
                   << "|"
                   << std::string(15, '-')
                   << "|"
                   << std::string(15, '-')
                   << "|"
                   << std::string(15, '-')
                   << "|"
                   << std::string(15, '-')
                   << "|"
                   << std::endl;

    float total{0.0f};
    for (const auto & res : segmentDistances) {
        total += res->distance;

        std::cout << std::left<< "|"<< std::setw(10) << res->name<< "|"
                  << std::setw(20) << std::format("{:.3f}",res->base)
                  << "|"
                  << std::setw(15) << std::format("{:.3f}",res->expandDelta)
                  << "|"
                  << std::setw(15) << std::format("{:.3f}",res->distance)
                  << "|"
                  << std::setw(15) << std::format("{}",res->order)
                  << "|"
                  << std::setw(15) << std::format("{:.3f}",res->offset)
                  << "|"
                  << std::endl;
    }
}
```

### Reorder
```cpp
constexpr void SetSegmentOrder(DiscadeltaPreComputeMetrics& preComputeMetrics, std::string_view name, const size_t order) {
  const auto it = std::ranges::find_if(
      preComputeMetrics.segments, [&](const auto& seg) { return seg->name == name; });
    if (it != preComputeMetrics.segments.end()) (*it)->order = order;
}
```

**Technical Note:** Developer can free design reorder logic, set DiscadeltaSegmentsHandler directly or DiscadeltaPreComputeMetrics segment pointer container.

## The Logic: Decoupling Size from Order
The beauty of the 3-pass system is that Pass 2 (Scaling) can process segments in any order it wants (Priority Sorting) to ensure fairness. Pass 3 (Placing) then takes those finished sizes and arranges them according to the order member.
### Use Case: Real-Time Swapping
If a user drags a UI element to swap its position with another:
1. Skip Pass 1 & 2: The sizes don't need to change.
2. Update Order: Swap the order values of the two segments.
3. Run Pass 3: Re-calculate the offsets. The segments "slide" into their new positions instantly with zero heavy lifting.

## Implementation: The Placing Pass
The Placing Pass is a simple, high-speed $O(N)$ linear accumulation. We sort by the order and then stack the distances.

```ccp
constexpr void DiscadeltaPlacing(DiscadeltaPreComputeMetrics& preComputeMetrics) {
    // 1. Sort segments by their desired visual 'order'
    std::sort(preComputeMetrics.segments.begin(), preComputeMetrics.segments.end(), [](const auto& a, const auto& b) {
        return a->order < b->order;
    });

    // 2. Linear accumulation of offsets
    float currentOffset = 0.0f;
    for (auto* seg : preComputeMetrics.segments) {
        if (!seg) continue;

        // The segment starts at the current-accumulated distance
        seg->offset = currentOffset;

        // Push the offset forward by the segment's resolved size
        currentOffset += seg->distance;
    }
}
```

### Code Sample (C++23)
```cpp
int main() {
    std::vector<DiscadeltaSegmentConfig> segmentConfigs{
          {"Segment_1", 200.0f, 0.7f, 0.1f, 0.0f, 100.0f, 2},
          {"Segment_2", 200.0f, 1.0f, 1.0f, 300.0f, 800.0f, 1},
          {"Segment_3", 150.0f, 0.0f, 2.0f, 0.0f, 200.0f, 3},
          {"Segment_4", 350.0f, 0.3f, 0.5f, 50.0f, 300.0f, 0}};

    constexpr float rootDistance = 800.0f;
    auto [segmentDistances, preComputeMetrics, processingCompression] = MakeDiscadeltaContext(segmentConfigs, rootDistance);

    if (processingCompression) {
        DiscadeltaCompressing(preComputeMetrics);
    }
    else {
        DiscadeltaExpanding(preComputeMetrics);
    }

    DiscadeltaPlacing(preComputeMetrics);

    Debugger(segmentDistances, preComputeMetrics);
    //the debugger is slow we sleep for 2 seconds before trigger another log
    std::this_thread::sleep_for(std::chrono::seconds(2));

    SetSegmentOrder(preComputeMetrics, "Segment_1", 3);
    SetSegmentOrder(preComputeMetrics, "Segment_3", 2);

    DiscadeltaPlacing(preComputeMetrics);

    Debugger(segmentDistances, preComputeMetrics);

    return 0;
}
```

### Result Table (rootDistance 800)

| Segment   | Base    | Delta | Distance | Order | Offset  |
|-----------|---------|-------|----------|-------|---------|
| Segment_1 | 78.125  | 0.000 | 78.125   | 2     | 571.875 |
| Segment_2 | 300.000 | 0.000 | 300.000  | 1     | 271.875 |
| Segment_3 | 150.000 | 0.000 | 150.000  | 3     | 650.000 |
| Segment_4 | 271.875 | 0.000 | 271.875  | 0     | 0.000   |

* After reorder

| Segment   | Base    | Delta | Distance | Order |  Offset |
|-----------|---------|-------|----------|-------|---------|
| Segment_1 | 78.125  | 0.000 | 78.125   | 3     | 721.875 |
| Segment_2 | 300.000 | 0.000 | 300.000  | 1     | 271.875 |
| Segment_3 | 150.000 | 0.000 | 150.000  | 2     | 571.875 |
| Segment_4 | 271.875 | 0.000 | 271.875  | 0     | 0.000   |

## The Complete Discadelta Pipeline
We have now built a professional-grade 3-pass architecture:
1. Pre-compute Pass (The Analyst): Validation and priority strategy.
2. Scaling Pass (The Resolver): Constraint-aware iterative cascading (The "Brain").
3. Placing Pass (The Builder): Geometric positioning and reordering (The "Body").

### Conclusion

Discadelta is now more than just a formula; it is a full-featured Layout Controller. It provides UFox with a rock-solid foundation where precision is guaranteed, constraints are respected, and the UI remains fluid and flexible under any condition.