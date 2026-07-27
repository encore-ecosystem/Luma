# Luma renderer architecture

## Goal

Luma is a reactive native UI framework with one retained paint contract and
a required Vulkan backend. A compatible Vulkan implementation is a runtime
prerequisite on every supported platform.

## Architecture

```text
Application -> widget tree -> retained layout/damage -> paint contract
                                                   `-> Vulkan
```

Widget painting and explicit `DisplayList` canvas drawing converge on the same
native primitive contract. Reactive changes repaint only affected cache
regions. Keeping the retained widget traversal direct is intentional with the
current Encore value model: materializing and copying a second full command
tree on every structural frame measurably increases latency without improving
backend isolation. `DisplayList` remains the backend-neutral public API for
custom drawing and damage descriptions.

`RendererKind::Auto` resolves to Vulkan. Vulkan initialization failures remain
visible to the caller. `RendererInfo` reports the backend, device and driver.

## Delivery roadmap

### Stage A: backend-independent rendering maximum

1. Finish scale-aware SVG/geometry quality and visual regression coverage at
   representative 1x/1.25x/1.5x/2x scales and small icon sizes.
2. Make the first visible application frame complete and atomic: no dark,
   transparent, partially laid-out or intermediate window states.
3. Keep Graphene's local debug startup below one second, with the first
   complete frame below 200 ms on the reference machine.
4. Replace backend-specific retained caches with one backend-neutral segmented
   display-list model. Stable base regions and overlays keep explicit
   lifetimes; local damage rebuilds only the affected segments.
5. Remove the remaining per-frame retained-tree searches, temporary
   collections, redundant damage work, descriptor lifetime hazards and
   avoidable frame-resource waits.
6. Use a persistent glyph/image atlas with generation-safe resource ownership
   instead of retaining backend descriptor pointers in cached commands.
7. Profile startup, idle, hover, animation and uncapped continuous rendering.
   Optimize until the profiles contain no known avoidable full-tree work,
   allocation, upload, pipeline transition or synchronous CPU/GPU wait in
   Luma-owned code.

Stage A is a hard delivery gate. Its SVG, startup and performance acceptance
criteria must pass on the existing SDL Vulkan implementation before the direct
backend is promoted beyond its existing research prototype. This keeps
renderer-independent defects out of production Vulkan integration and gives it
a stable reference.

### Stage B: direct Vulkan composition backend

1. Resume the existing research prototype as a separate production backend
   only after Stage A is accepted.
2. Make Luma the sole owner of the active Vulkan frame lifecycle and let
   Graphene record its viewport inside that lifecycle.
3. Record retained base UI, the Graphene viewport and transient overlays into
   one composition graph with one owner-controlled submit and present.
4. Reach visual, input, resize, HiDPI, present-mode, resource-lifetime and
   lifecycle parity with the Stage A reference on Linux and Windows.
5. Switch `RendererKind::Auto` to direct Vulkan after runtime validation.
   Initialization failure is explicit.

The direct backend is an internal tagged backend implementation, not an
abstract refrain. It owns the Vulkan instance, device, queue, swapchain,
atlases and persistent frame resources. The final composition graph records
retained base UI, the Graphene viewport pass and transient overlays in order,
then performs one owner-controlled submit and present.

## Embedded rendering

Luma owns the window, Vulkan instance/device, swapchain and presentation.
`Window::vulkan_interop()` lends the exact backend device and swapchain
owners without transferring ownership. `Window::begin_vulkan_frame()` records
the retained base and returns a short-lived active frame. Graphene binds its
compatible pipeline and records the viewport directly into that render pass;
`Window::present_vulkan_frame()` appends transient overlays and performs the
only submit and present. The borrowed frame becomes invalid at present.

The old CPU compositor API (`create_gpu_surface`, `surface_pixels`,
`surface_pitch`) and the sampled intermediate viewport are absent from the
editor path.

## Current implementation status

- The renderer-neutral `DisplayList`, damage regions and replay entrypoint are
  available for explicit canvas-style drawing. Widget rendering uses the same
  native primitive contract without duplicating its retained tree.
- `RendererKind::Auto` and the explicit production renderer resolve to Vulkan.
  Deterministic rasterization fixtures may use a private headless test harness,
  but it is not an application renderer or runtime mode.
- The showcase and examples exercise the same Vulkan path as applications.
- Direct frame interop records retained base, Graphene viewport and transient
  overlays in one compatible render pass and one Luma-owned submission.
- Graphene borrows Luma's exact backend device/swapchain owners. It cannot
  resize, submit, present or destroy them.
- Managed applications are event-driven by default. Retained reconciliation,
  indexed hit testing, focus and pointer state, animation deadlines, native
  system fonts, stale-handle protection, and typed creation errors are
  implemented.
- Interaction CSS is parsed once and patched onto the retained target instead
  of restyling the complete tree. Animated subtree replacement resolves all
  requested IDs in one declarative traversal, relayouts only those subtrees,
  and splices only their entries into the flat hit-test index; unrelated
  layout and index branches are not rebuilt on animation ticks.
- Window creation may map an inexpensive technical frame only when the
  compositor supports hiding it with opacity. `Window::reactive` builds the
  retained cache before presentation and reveals exactly one complete
  application frame; a focused lifecycle regression test verifies the
  single-present invariant.
- Composite caching excludes the external viewport and transient overlays, so
  every layer is painted exactly once and borders keep stable alpha.
- Indexed composition accepts retained external-image bounds and overlay
  subtrees directly. Stable transient layers are cached as transparent
  textures and refreshed only on overlay damage, rather than rasterized again
  for every viewport frame.
- Graphene's uncapped debug path sustains roughly 6100 frames per second
  without an overlay and 5500 with a cached tooltip on the reference machine,
  instead of recursively searching and repainting the full tree every frame.
- Fixed application icon sets resolve named bundled SVGs lazily without
  enumerating the complete catalog. Flex layout skips grid-only intrinsic
  recursion and already ordered children use a linear fast path.
- The reference Graphene debug startup presents its atomic startup frame in
  roughly 160 ms and reaches the complete interactive editor in roughly
  520–550 ms.
- The rebuilt Luma showcase presents its first complete SDL Vulkan frame in
  168 ms on the reference NVIDIA/Wayland machine. The icon showcase has been
  visually audited at 12/14/16/20/24/32/48/64 px and after live resize at the
  monitor's fractional scale; strokes remain coherent and the layout updates
  in the resize transaction.
- The backend-neutral retained scene now has stable keyed segments, independent
  generations, base/overlay classification and old/new-bounds damage.
  Application cache and present paths replay those segments directly without
  flattening or allocating a full temporary display list; local subtree
  replacement preserves unrelated segment and hit-index generations.
- SDL Vulkan text rendering now packs complete shaped runs into persistent
  atlas pages instead of allocating one texture and descriptor per string.
  Cache entries borrow generation-tagged atlas regions, atlas reset invalidates
  every borrower atomically, and oversized runs retain a bounded standalone
  path. Direct Vulkan implements the same generation-safe atlas-region
  contract.
- A Linux Direct Vulkan backend has been extracted into Luma. It owns a Vulkan
  window/swapchain, renders shapes, text and external images, and has passed a
  local NVIDIA lifecycle smoke test. Graphene now uses it for Stage B runtime
  validation.
- Direct Vulkan batches retain texture objects rather than raw descriptor-set
  pointers. Cache eviction invalidates retained borrowers and defers descriptor
  and image destruction until both frame-slot fences have crossed retirement,
  without a device- or queue-idle wait on the frame path.
- The production Direct Vulkan device requires and enables
  `VK_EXT_swapchain_maintenance1`. Every present carries a presentation fence;
  local runtime validation recreated a live Wayland swapchain from
  1770x1140 to 2096x2076 and presented the complete replacement frame without
  a queue-idle stall or partial surface. Direct compositor teardown and
  render-pass replacement also wait only their own render/presentation fences;
  no `queue_wait_idle` remains in the Direct UI/swapchain implementation.
- Direct Vulkan uses the backend-neutral segmented cache and a single
  Luma-owned Graphene/UI submission. FIFO and
  Immediate/Mailbox present policies are selected by the direct swapchain.
  Rebuild atomically activates a fresh swapchain and retires old generations
  after their `VK_EXT_swapchain_maintenance1` presentation fences complete,
  without a queue-wide wait. Direct text uses persistent atlas
  textures/descriptors, UV regions and
  one coalesced dirty-page upload per frame. Windows validation remains a
  Stage B production task.

## Direct Vulkan API direction

- Add `RendererKind::DirectVulkan`; keep the current explicit SDL Vulkan path
  temporarily as a Vulkan-only migration reference.
- `RendererKind::Auto` resolves to direct Vulkan. Failure returns the original
  Vulkan diagnostic.
- Extend `RendererInfo` with the selected Vulkan implementation.
- Add a short-lived borrowed `VulkanFrameInterop` for the active command
  buffer, frame slot, extent and permitted viewport region. It becomes invalid
  at present and never transfers ownership to Graphene.
- Linux supports Wayland/X11 surfaces and Windows supports Win32. macOS is
  outside this delivery until it has a Vulkan-compatible implementation; it
  does not switch to Metal.

## Stage A acceptance

- Showcase reports Vulkan on a compatible machine and stays responsive.
- Absence of a compatible Vulkan device fails startup with an actionable
  diagnostic.
- Borders, text and SVG golden images agree within the documented tolerance.
- Hover and animation repaint only damaged regions.
- Retained base UI and overlays use backend-neutral cache segments; a local
  change does not invalidate or traverse unrelated segments.
- Cached text and images have generation-safe atlas ownership and cannot hold
  stale backend descriptors after eviction.
- Resize and HiDPI changes do not produce stale or partial frames.
- The first mapped frame is the complete application frame; startup never
  exposes an empty, transparent or partially composed window.
- The startup frame appears within 200 ms and the complete local debug editor
  is interactive within one second on the reference machine.
- Startup, idle, hover, animation and continuous-frame profiles have no known
  avoidable full-tree traversal, per-frame allocation, redundant redraw or
  synchronous CPU/GPU wait in Luma-owned code.
- The existing SDL Vulkan implementation remains the performance and visual
  reference for the next stage.

## Stage B acceptance

- Direct composition has visual parity with SDL, uses no per-frame CPU upload,
  and retains idle zero-frame behavior.
- Linux and Windows require the direct Vulkan backend. Initialization or
  runtime incompatibility is reported as an error.
- Direct Vulkan records retained UI, the Graphene viewport and overlays into
  one Luma-owned frame lifecycle with one owner-controlled submit/present.
- VSync and immediate present policies rebuild the direct swapchain safely.
- Wayland/X11 and Win32 initialization, resize, minimize/restore and teardown
  pass targeted runtime coverage without device-idle waits on the frame path.
- Format, check, lint, tests and builds pass for Luma and the showcase.
