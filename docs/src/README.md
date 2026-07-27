# Luma

Luma is a native, retained-mode UI framework for applications written in
[Encore](https://github.com/encore-ecosystem/encore). It combines a declarative
widget tree with flex/grid layout, CSS-like styling, reactive partial updates,
SDL3 rendering, text input, SVG icons, fonts, and native window integration.

Luma is designed around four layers:

1. `Application` maps state and high-level actions to a `Widget` tree.
2. `WidgetLayout` retains geometry and hit-testing data.
3. `ReactiveUi` reconciles trees and redraws only invalidated subtrees.
4. `Window` owns rendering, presentation, and external Vulkan composition.

The normal `Window::create` path uses the renderer selected by SDL3. On a
supported desktop this is normally Vulkan, Metal, Direct3D, or OpenGL. Use
`window.renderer_info()` to inspect the selected backend and physical adapter.

```enq
let info = window.renderer_info()
println("Renderer: " + info.backend() + " / " + info.device())
```

Start with [Getting started](getting-started.md), then read
[Reactive rendering](reactive.md) before building a large or animated UI.
