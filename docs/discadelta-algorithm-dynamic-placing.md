# Discadelta: Dynamic Placing Pass 🦊 (Chapter 4)

## Overview
In the previous chapters, we solved the "How big?" problem. Every segment now has a finalized `distance` that respects its constraints and fair-share proportions. However, in a high-performance engine like UFox, knowing the size is only half the battle. We need to know the **position**.

This chapter introduces the third and final stage of the pipeline: **The Placing Pass**. By decoupling size from position, we gain the ability to reorder elements visually without ever touching the complex scaling math again.

---

## 1. Anatomy of the Placing Pass
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