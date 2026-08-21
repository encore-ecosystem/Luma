# Rendering and GPU acceleration

## Normal windows

`Window::create` calls SDL3's `SDL_CreateWindowAndRenderer`. SDL selects an
available accelerated renderer for the platform. Inspect it at runtime:

```enq
let info = window.renderer_info()
println(info.backend() + " / " + info.device())
```

Typical values include `vulkan`, `metal`, `direct3d11`, `direct3d12`, and
`opengl`. `software_emulated()` also detects Vulkan software adapters such as
llvmpipe, lavapipe, and SwiftShader by physical-device name.

Luma submits rectangles, textures, text glyph textures, gradients, rounded
geometry, SVG strokes, and cached layout textures through SDL's render API.
The normal window path therefore already renders on the GPU when SDL reports
an accelerated backend.

## Cached reactive frames

`Window::reactive` stores the stable UI in a render-target texture.
`draw_reactive` patches only dirty rectangles and then composites that texture.
This is usually more important for UI latency than moving layout computation
to a GPU.

## External Vulkan images

Luma owns the window and presentation. An embedded renderer borrows
`Window::vulkan_interop()`, creates sampled images on that exact device, and
passes them to `CompositeFrame::with_external_vulkan_image`. Luma composes the
retained base, external image, and transient overlays on one graphics queue.
There is no CPU readback, upload, second swapchain, or per-frame fence wait.

## Direct native composition

`RendererKind::DirectVulkan` is the Linux direct renderer and
`RendererKind::DirectMetal` is the Apple Silicon macOS direct renderer. Use
`Window::direct_graphics_interop` and the backend-neutral direct-frame methods
when an embedded renderer supports both platforms. Luma records retained base
UI, application compute and scene work, then transient overlays into one native
frame and performs exactly one submit/present.

Metal shaders are precompiled application artifacts. The runtime does not
invoke shaderc, SPIRV-Cross, or Apple's Metal compiler. Applications that ask
for DirectMetal on a non-macOS target, or DirectVulkan on macOS, fail fast
instead of silently selecting a different renderer.
