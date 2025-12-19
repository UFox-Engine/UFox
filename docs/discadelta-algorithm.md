# Discadelta Algorithm 🦊

## What is Discadelta?

**Discadelta** = **Distance** + **Cascade** + **Delta**

**Distance**: One-dimensional positive value.  
**Cascade**: Front segment consumes remaining distance.  
**Delta**: Filling/sharing leftover distance to each segment.


Discadelta is a 1D partitioning algorithm for dividing a line (width, height, 
edge length, etc.) into resizable segments. 
It powers fit rect layout in UFox’s GUI/Editor system, 
but is also designed to be used anywhere in the engine that needs reliable, 
interactive linear splitting.

 Use Cases:
- **Tools/Editor**
- **Procedural Generation**
- **Constraints**

## Theory

### Goal:
Calculate the distance of an individual segment given the root distance.

### Scenario:
Given a **root distance** of **800** and **4 segments** with no demand:

### Formula:
```
root_distance / num_segments 
```
```
800 / 4 = 200
```
![Discadelta: Simple equal partitioning (800px root → 4 segments of 200px each)](images/DiscadeltaDocImage01.jpg)

Logically, each segment should get **200**. However, it becomes complex if each segment has its own **demand** determining its proportional share.

