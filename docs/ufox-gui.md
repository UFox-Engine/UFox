# UFox GUI

## Overview

The **UFox GUI** is a retained-mode, GPU-accelerated user interface system built on top of the Discadelta layout engine. It provides a hierarchical tree of visual elements that are laid out, styled, and rendered efficiently through Vulkan storage buffers and a dedicated graphics pipeline.

Key characteristics:

- Retained-mode scene graph (`VisualElement` tree owned by `UIDocument`)
- Layout powered by **Discadelta** (high-precision 1D rescaling with constraints, flex-like behavior)
- Style deduplication via a GPU palette (`GUIPaletteManager`)
- Direct GPU upload of element data (mapped SSBO, no staging)
- Pointer event system with hit-testing, hover, press, and bubbling
- Support for solid colors, textured backgrounds, borders with corner radii, and text (via MSDF glyphs)

Limits (compile-time):

| Constant               | Value     | Purpose                        |
|------------------------|-----------|--------------------------------|
| `MAX_GUI_ELEMENTS`     | 65536     | Maximum ElementData slots      |
| `MAX_GUI_TEXTURES`     | 256       | Descriptor array size          |
| `MAX_TOTAL_CHARACTERS` | 1 048 576 | Theoretical glyph budget       |

## Architecture

```
UIDocument
 ├── rootElement (VisualElement)
 │    ├── children (VisualElement / Label / ...)
 │    └── ...
 ├── GUIPaletteManager          (style deduplication → SSBO)
 ├── elementSSBO                (mapped ElementData[])
 └── Vulkan pipeline + descriptors
```

- **UIDocument** owns the root, the palette, the element SSBO, and the graphics pipeline. It registers itself into the engine’s event loop (`eSystemInit`, `ePreDraw`, `eRenderPass`, `ePointer`, etc.).
- **VisualElement** is the base node. It inherits the Discadelta layout base, holds a `Style`, and writes its own `ElementData` into a mapped GPU slot.
- **Label** is a specialized leaf that expands into multiple glyph quads (extra SSBO slots).
- Layout and structure changes are marked dirty; the actual GPU synchronization happens in `onPreDraw` / `onLayoutChanged`.

## Core Types

### Style (`ufox_gui_lib`)

Inherits from the Discadelta style base and adds visual properties:

- Background: `backgroundColor`, `backgroundImage`, `imageScaleMode`, `imageRepeatMode`, `imageAlignment`, `imageColor`
- Borders: per-side width + color + corner radii
- Text: `fontFamily`, `fontSize`, `fontType`, `fontColor`, `languageCodeBlock`
- Overflow: `displayOverflowMode` (`eVisible` / `eHidden`)

### ElementData (160 bytes, aligned)

GPU-side per-element record:

- `model` (mat4)
- `imageOffsetAndTiling` (vec4)
- `sdInput` (SDRectInputData – size, offsets for signed-distance border rendering)
- `scrollingOffset`
- `styleIndex` (into the palette)
- `imageID` (texture index)

### StyleStaticData (144 bytes, aligned)

Shared visual constants (colors, radii, repeat mode, isFont flag). Deduplicated by `GUIPaletteManager`.

### VisualElement

Base class for every node in the tree.

Important members:

- `document`, `name`, `style`
- `gpuSlot` / `elementData` – direct pointer into the mapped SSBO
- `leafChildren` / `branchChildren` – used for efficient subtree synchronization
- Event registration (`registerCallbackEvent<ElementEventType::...>`)

Key methods:

- `createChild<T>(...)` – creates and attaches a child (inherited from Discadelta base)
- `add(std::unique_ptr<VisualElement>)`
- `markStyleChanged()` / layout dirty flags
- `syncToGpu(...)` – writes current transform + style into the mapped slot
- `hitTest(pos)` – used by the pointer system

### Label

Final class that represents text.

- Stores UTF-32 text and resolved glyph contexts
- Occupies `1 + visibleGlyphCount` SSBO slots
- `syncToGpu` writes the background box plus one quad per glyph
- Glyphs are rendered as textured quads using the MSDF atlas

### UIDocument

The root owner of a GUI instance.

Construction:

```cpp
document.emplace(*window, *meshManager, *glyphManager, &*textureManager);
```

Public surface:

- `rootElement` – the top-level container
- Palette and mesh are managed internally
- Dirty flags: `treeStructureChangedFlag`, `layoutChangedFlag`, `styleChangedFlag`

Lifecycle hooks (registered automatically):

- `init()` – creates root, sets default flex style
- `onPostSystemInit` – creates the shared rect mesh, builds pipeline & descriptors
- `onPreDraw` – resolves style / structure / layout dirtiness
- `drawCanvas` – begins dynamic rendering, binds pipeline, draws the rect mesh with the element SSBO

### GUIPaletteManager

Deduplicates `StyleStaticData` into a compact GPU buffer and provides stable indices. Clears dynamic styles on tree rebuilds.

## Layout Integration (Discadelta)

Every `VisualElement` inherits `DiscadeltaRectLayoutBase`. Layout is performed by calling:

```cpp
rootElement->updateLayout(width, height, force);
```

Typical style controls (inherited):

- `flexExpand` / `flexCompress`
- `minWidth` / `maxWidth` / `minHeight` / `maxHeight` (with `LengthMode`)
- `margin*` / `border*Width` / `border*Radius`
- `direction`, `justifyContent`, `alignItems`, `alignSelf`
- `width` / `height` (absolute or percentage)

Changes to style that affect layout must be followed by a call that marks the document dirty (or `updateMetrics()` + dirty flag).

## Event System

Events are type-safe via `ElementEventType` and `EventTraits`.

Supported events:

- `AttachmentEvent` / `DetachEvent`
- `PointerEnter` / `PointerLeave` / `PointerMotion`
- `PointerDown` / `PointerUp` / `PointerClick` / `PointerWheel`

Registration:

```cpp
element->registerCallbackEvent<gui::ElementEventType::PointerEnter>(
    [](gui::ElementPointerEvent& e, void* user) {
        auto* el = static_cast<gui::VisualElement*>(user);
        el->style.backgroundColor = {1, 0, 0, 1};
        el->markStyleChanged();
    },
    element);
```

The document maintains hover/press state and a queue that is flushed in `eProcessInput`. Events can stop propagation.

## GPU Rendering Pipeline

1. **Shared mesh** – a unit quad (`gui_rect_mesh`) is drawn once per element (instanced via SSBO).
2. **Element SSBO** – host-visible, persistently mapped. `AssignGpuSlots` walks the tree and hands each element a pointer into this buffer.
3. **Style palette SSBO** – small buffer of unique `StyleStaticData`.
4. **Descriptors**:
   - Binding 0/1: element + style SSBOs
   - Binding 2: camera uniform
   - Binding 3: sampled image array (textures + glyph atlas)
   - Binding 4: sampler array (clamp / repeat combinations)
5. **Shader** (`res/shaders/test.slang`) consumes the data and performs signed-distance border rendering, image sampling (with scale/crop/repeat), and MSDF glyph rendering.
6. Drawing happens inside the engine’s `eRenderPass` callback with dynamic rendering.

Dirty handling:

- Structure change → rebuild slot assignment + texture descriptors + clear dynamic palette
- Layout change → `SyncSubtree` (write transforms & UVs)
- Style change → upload palette, then force structure rebuild

## Usage Example

From `ufox_engine.cppm` (simplified):

```cpp
auto* v11 = doc->rootElement->createChild<gui::VisualElement>(KeyID{"v11"}, "v11");

v11->style.backgroundColor = {1, 1, 1, 1};
v11->style.flexCompress    = 1.0f;
v11->style.flexExpand      = 1.0f;
v11->style.maxWidth        = discadelta::Length{discadelta::LengthMode::eFlat, 150};
v11->style.alignSelf       = discadelta::AlignMode::eStretch;
v11->style.marginLeft = v11->style.marginRight = v11->style.marginTop = v11->style.marginBottom = 5;

// Borders with radii
v11->style.borderBottomColor = {0, 0, 1, 1};
v11->style.borderBottomWidth = 3;
v11->style.borderBottomLeftRadius  = 25;
v11->style.borderBottomRightRadius = 17;

v11->registerCallbackEvent<gui::ElementEventType::PointerEnter>(...);
v11->updateMetrics();

auto* title = doc->rootElement->createChild<gui::Label>(
    KeyID{"title"}, "title", ufox_beta::GetHallo(), 24.0f);
title->setColor({1, 1, 1, 1});
```

## Design Notes & Future Directions

- **Direct mapped writes** keep the CPU→GPU path extremely cheap for layout and style updates.
- **Leaf vs branch children** allow `SyncSubtree` to skip unnecessary recursion.
- **Style palette** prevents uploading identical color/radius combinations thousands of times.
- **Label glyph expansion** is currently done on the CPU side (one slot per glyph). A future compute or geometry-shader path could reduce this.
- Scrolling offset is reserved in `ElementData` but not yet implemented.
- Overflow clipping (`eHidden`) is stored but clipping is expected to be handled in the shader or via stencil (current pipeline uses stencil attachment).

## Related Documents

- `discadelta-algorithm.md` and siblings – layout engine theory
- `ufox GUI GPU Rendering Pipeline-*.svg` – visual pipeline diagram

---

*Generated from the current `ufox_gui_core.cppm` / `ufox_gui_lib.cppm` implementation.*
