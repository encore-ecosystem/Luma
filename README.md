# Luma

Luma is a cross-platform native UI framework for Encore. It provides widgets,
flex and grid layout, CSS-like styling, animations, SVG and Lucide icons,
font management, file picking, and an SDL3 renderer.

## Requirements

- Encore 0.1.4 or newer
- SDL3 available at runtime
- SDL3_ttf for TrueType/OpenType font rendering

Luma loads SDL dynamically, so applications do not need platform-specific SDL
linker flags.

## Install

Once Luma is available in the Encore package index:

```sh
encore add luma
```

Then use the managed, event-driven application runtime:

```enq
import luma::{AppEvent, Application, Widget, Window, run_application}

struct Counter { value: u32 }

impl Application for Counter {
    fn view(self: Self) -> Widget {
        ret Widget::button("counter", "Count")
    }
    fn update(self: Self, event: AppEvent) -> Self {
        if event.is_action() && event.action() == "counter" {
            ret Self { self.value + 1_u32 }
        }
        ret self
    }
    fn can_close(self: Self) -> bool { ret true }
    fn background_interval_ms(self: Self) -> u32 { ret 0_u32 }
    fn background_revision(self: Self) -> u64 { ret 0_u64 }
}

fn main() -> u32 {
    let window = Window::create("Counter", 320_u32, 180_u32)
    if !window.available() { ret 1_u32 }
    let _final = run_application(window, Counter { 0_u32 })
    ret 0_u32
}
```

Managed applications receive normalized `TextEditIntent` values for committed
text, deletion, cursor movement, selection, and clipboard shortcuts. The
application remains the owner of its `TextEditor` or external document model;
Luma only translates platform events into edit requests.

Pointer placement and drag selection are delivered through the same intent.
Native close requests are delivered to `update` before `can_close` is queried,
so applications can protect unsaved state. Unhandled key-down events remain
available as `AppEvent::is_key` for application commands such as Save.
Models may return a non-zero `background_interval_ms` while observing a PTY,
language server, or other external service. Returning zero restores blocking
native-event waits with no periodic redraws.

Large applications can implement `IncrementalApplication` and return a single
updated widget from `view_target`. `run_incremental_application` applies that
widget directly to retained state, while `None` keeps the safe full-view
fallback for structural changes. Pointer hover/focus dispatch uses the retained
path index and never calls the application view.

Editors and engines that render into a retained `Widget::render_surface` can
implement `DirectRenderApplication`. The managed runtime then owns input,
layout, Vulkan composition, submission and presentation while the application
records pre-composition compute work and its surface graphics pass directly
into the borrowed frame. `with_secondary_pointer_lock` provides reusable
RMB-look semantics; raw surface input preserves key releases, relative motion,
button state, modifiers and native timestamps.

## Typed HTML, CSS, and closure events

HTML-like builders remain ordinary typed Encore values; Luma never parses an
HTML string. A stylesheet is stored once on the window, layered over
`StyleSheet::default()`, and reused by every retained widget:

```enq
let count = Signal[u32]::new(0_u32)
let view = Html::new().with_child(
    Body::new("body").with_child(
        Html::div("counter").with_child(
            Button::new("increment", "Increment").with_on_click(|event| {
                count.set(count.get() + 1_u32)
            }),),)
let window = Window::create("Counter", 480_u32, 320_u32)
    .with_stylesheet(StyleSheet::from_file("app.css").unwrap_or(StyleSheet::parse("")))
```

Events propagate through capture, target, and bubble phases and support
`stop_propagation()` and `prevent_default()`. CSS pseudo-classes including
`:hover`, `:active`, `:focus`, `:disabled`, and `:checked` are resolved through
a bounded computed-declaration cache. See `examples/html-counter` for a
complete runnable application.

## Features

- Standard controls, form widgets, icons, and state-aware hit testing
- Typed HTML-like builders, owning Encore closure callbacks, and DOM-style
  capture/target/bubble propagation
- Flexbox, grid, absolute and relative positioning, scrolling, and clipping
- Window-owned CSS files, default user-agent styles, cascade, variables,
  selectors, pseudo-classes, and retained computed-style caching
- Color and geometry primitives
- SVG path parsing and a bundled Lucide icon catalog
- Bundled Open Sans, Inter, Roboto, and Roboto Mono font families
- Native SDL3 windows, automatic hardware/software renderer selection,
  reactive cached frames, clipboard, text input, and HiDPI support

## Documentation

The complete user guide is an mdBook in [`docs/`](docs/src/README.md):

```sh
mdbook serve docs --open
```

It covers application structure, reactive rendering, widgets, layout, CSS,
text editing, animation, icons, fonts, GPU rendering, and embedding.

Font support and native system-font discovery work by default. The `fonts`
feature also makes bundled font assets available to package consumers:

```toml
dependencies = [
    "index@luma@^0.1.0[fonts]",
]
```

## Development

```sh
encore sync
encore format --check
encore check
encore lint
encore test
```

Run the interactive catalog from `demo/`:

```sh
cd demo
encore run
```

## License

Luma is released under the MIT License. Bundled fonts are distributed under
their respective SIL Open Font License files in `assets/fonts/`.
