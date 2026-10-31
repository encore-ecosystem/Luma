# Vulkan boundary

`sdl` owns the SDL loader and window-specific event queues. `gpu` owns Vulkan
instance/device creation, the SDL surface adapter and swapchain lifetime.
Luma keeps its event-loop policy and compositor. Release compositor resources,
then close the GPU context, then destroy the SDL window.

`vulkan_compositor.inc`, `ui_shaders.inc` and `shaders/` implement Luma's UI
rendering and are not part of the published `vulkan` archive.

`vulkan_api.h` is the vendored public C header for vulkan 0.1.0. It exposes
opaque handles and operations, not object layouts. Keep it byte-identical to
the dependency's header from the locked published archive. Do not include Vulkan's backend sources
or add direct accesses to its internal structures here.

`sdl_api.h` is the byte-identical public SDK header from `sdl`. It describes
the dispatch ABI, not an embedded SDL implementation. The Encore adapter lives
in `src/vulkan_adapter/mod.enq`; its native handles are borrowed from `gpu`.
There is no local copy of the Vulkan or SDL implementation.
Set `ENCORE_DEFAULT_INDEX` in your environment to select a development registry.
Consumers can explicitly override it in their own root project. The lockfile
pins the published archive and checksum.
