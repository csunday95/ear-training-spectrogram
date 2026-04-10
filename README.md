# OpenGL Compute Shader Template

A minimal OpenGL 4.5 starter kit with compute shader support for GPU-driven particle simulation.

## Features

- **Compute Shaders**: GPU-accelerated particle updates
- **GLFW Window**: Cross-platform windowing
- **ImGui Overlay**: Real-time stats display (particle count, FPS)
- **Camera Controls**: Mouse-based orbit camera
- **Catch2 Testing**: Shader compilation tests
- **CMake Build**: FetchContent dependencies (GLFW, GLM, ImGui, Catch2)

## Building

```bash
cmake --preset debug
cmake --build build/debug
./build/debug/opengl_template
```

## Options

```
--particles N     Number of particles (default: 4096)
--width W         Window width (default: 1280)
--height H        Window height (default: 720)
```

## Architecture

- `src/core/`: GL initialization, shader loading, camera
- `shaders/compute/`: Particle update kernel
- `shaders/render/`: Point rendering (vert + frag)
- `tests/`: Shader compilation tests
