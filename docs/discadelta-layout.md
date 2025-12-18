# Discadelta Algorithm 🦊

## What is Discadelta?
**Discadelta** = **Dis**tance + **Ca**scade + **Delta**
- **Distance**: one-dimension positive value
- **Cascade**: the change flows smoothly along the segment
- **Delta**: use each normalized distance segment for rescaling

Discadelta is a **correctness-first** 1D partitioning algorithm for dividing a line (width, height, edge length, etc.) into resizable segments. It powers fit rect layout in UFox’s GUI/Editor system, but is designed to be used anywhere in the engine that needs reliable, interactive linear splitting.

 Use Cases
- **Tools/Editor**
- **Procedural Generation**
- **Constraints**

## Design Priorities
**Functionality and bug-free behavior come first.**  
Performance is important but never at the cost of correctness.

Discadelta chooses:
- Predictable code
- Exact mathematical results (no approximation errors)
- Full support for constraints (min/max sizes, fixed pixels)
- Stable ratios when parent size changes

Speed is a welcome side effect of the incremental design, not the main goal.
