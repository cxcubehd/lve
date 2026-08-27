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

The application presents a continuously cleared frame using two frames in
flight. It recreates the swapchain after window-size changes and pauses frame
submission while the window is hidden, minimized, or has no drawable area.

Debug builds use the Khronos validation layer when available. Initialization
and rendering failures are reported through the SDL application callbacks.

### Platform notes

- macOS uses Vulkan portability enumeration and supports MoltenVK's portability
  subset when advertised. A Metal-capable graphical session and a MoltenVK ICD
  are still required.
- Headless sessions and remote or virtual machines may compile successfully but
  cannot run the windowed smoke test when no Vulkan presentation driver is
  available.
- FIFO presentation is used on every platform for predictable, bounded frame
  pacing.
