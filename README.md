# UFox Engine 🦊

[![C++23](https://img.shields.io/badge/C++-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Vulkan 1.3](https://img.shields.io/badge/Vulkan-1.3_Core-orange.svg)](https://www.khronos.org/registry/vulkan/specs/1.3/html/)
[![SDL3](https://img.shields.io/badge/SDL-3-green.svg)](https://www.libsdl.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Development Goals
- Engine Core: early state
- Engine Renderer: early state
- Engine Asset Manager: early state
- Engine Editor: waiting for UFox GUI
- Mesh Asset Manager: 70%,missing features (multi format support)
- Texture Asset Manager: 70%, missing features (multi format support)
- Events & Input System: early state
- UFox GUI Rect Rendering: 100%, ready
- UFox GUI Layout System: 95%, missing features (wrapping)
- UFox GUI MSDF Text Rendering: 60%, missing features (wrapping, outline, drop shadow, spacing)
- UFox GUI Interactive and Controller: 10% Complete, Events & Input System still in early state;
- Audio system: none
- 2D Demo Game: none
- Basic Demo Game Build system: none
- 3D Scene Editor: none
- Shader and Material: none
- Animation system: none
- Physics system: none
- Navigation system: none
- Visual fidelity: none
- Particle system: none
- 3D Demo game: none
- Complex Game Build system: none
- Release UFox Engine 1.0.0: none


**In-House Technologies**
- [Discadelta Algorithm](src/ufox_geometry_core.cppm#L572) — Incremental 1D chain partitioning for GUI Layout System.
- [UFox GUI](docs/ufox-gui.md) — The UFox GUI is a retained-mode, GPU-accelerated user interface system built on top of the Discadelta layout engine featuring SDF Font.

**Third-Party Technologies**
- [MSDF-ATLAS-GEN](https://github.com/Chlumsky/msdf-atlas-gen) — Multi-channel signed distance field atlas for Font atlas texture
