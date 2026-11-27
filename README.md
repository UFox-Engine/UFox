# UFox Engine 🦊

[![C++23](https://img.shields.io/badge/C++-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![Vulkan 1.3](https://img.shields.io/badge/Vulkan-1.3_Core-orange.svg)](https://www.khronos.org/registry/vulkan/specs/1.3/html/)
[![SDL3](https://img.shields.io/badge/SDL-3-green.svg)](https://www.libsdl.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Current Development State
- In-House GUI

**In-House Technologies**
- [Discadelta Algorithm](src/ufox_geometry_core.cppm#L572) — Incremental 1D chain partitioning for GUI Layout System.
- [UFox GUI](docs/ufox-gui.md) — The UFox GUI is a retained-mode, GPU-accelerated user interface system built on top of the Discadelta layout engine featuring SDF Font.

**Third-Party Technologies**
- [MSDF-ATLAS-GEN](https://github.com/Chlumsky/msdf-atlas-gen) — Multi-channel signed distance field atlas for Font atlas texture
