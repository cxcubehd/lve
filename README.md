# lve

> This project is a work in progress.

## Building

The renderer requires a C++23 compiler, Vulkan 1.2 with swapchain support, and
a window-system Vulkan driver. Configure and build a debug binary with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target lve
```

Run it with:

```bash
./build/lve
```

## Renderer milestone

The application now presents a continuously cleared empty frame through a
two-frame VulkanHpp RAII lifecycle. Debug builds enable the Khronos validation
layer when it is installed and continue with a warning when it is unavailable.
Initialization and per-frame failures are reported before SDL exits cleanly.

Pixel-size, display-scale, minimize, maximize, and restore events only mark the
swapchain dirty. A 100 ms settle interval coalesces rapid size changes; the
latest drawable extent is used for one rebuild. Rendering is suspended without
waiting while the window is hidden, minimized, or has a zero-sized drawable,
and resumes when a usable extent returns. A device-idle wait occurs only for an
actual swapchain replacement, so duplicate events and same-size restores do not
stall or rebuild resources.

Renderer ownership is deliberately concrete: device-wide state,
swapchain-dependent state, and per-frame state are distinct. Future sky, map,
and player renderers can be owned by `Renderer`, initialized explicitly, and
recorded in order at the marked point inside the render pass; none are part of
this milestone.

### Platform notes

- macOS uses Vulkan portability enumeration and supports MoltenVK's portability
  subset when advertised. A Metal-capable graphical session and a MoltenVK ICD
  are still required.
- Headless sessions and remote or virtual machines may compile successfully but
  cannot run the windowed smoke test when no Vulkan presentation driver is
  available.
- FIFO presentation is used on every platform for predictable, bounded frame
  pacing.
