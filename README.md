# Luma

Luma is a cross-platform native UI framework for Encore. It provides widgets,
flex and grid layout, CSS-like styling, animations, SVG and Lucide icons,
font management, file picking, and an SDL3 renderer.

## Requirements

- Encore 0.1.6 or newer
- SDL3 available at runtime
- SDL3_ttf for TrueType/OpenType font rendering
- HarfBuzz with its subset library (6.0+) for font loading and variable weights
- A Vulkan-capable driver for the default renderer

Luma loads SDL dynamically, so applications do not need platform-specific SDL
linker flags.

## Install

Color primitives and the native UI bridge are bundled as private workspace
packages. External dependencies are resolved through the Encore index.

Install from the selected Encore package index (`ENCORE_DEFAULT_INDEX`):

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

### Local stylesheet imports

Split application styles into files and load the entry point with
`StyleSheet::from_file("app.css")`:

```css
@import "styles/theme.css";
@import url("styles/controls.css");
@import url(styles/dialogs.css);
```

Imports precede ordinary rules in each file. Nested paths are relative to the
importing file, and repeated imports participate in the cascade at each
occurrence. Quoted paths accept CSS escapes and UTF-8. Missing files, invalid
syntax, cycles, and more than 64 nested files return an error with the import
chain. Only local files are supported; network URLs and `media`, `supports`,
or `layer` conditions return errors.

`reload()` rereads the entry point and its imports; failed reloads keep the
previous stylesheet and log the error. It does not install a file watcher.
`try_parse()` rejects imports because in-memory CSS has no file origin;
`parse()` logs and skips them, preserving subsequent rules without file IO.
Ship all imported files alongside the root stylesheet.

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
- Bundled families use variable font files; redundant static faces are omitted.
  Integer weights from 1 to 1000 are preserved, including intermediate values
  such as 425. Variable fonts use their real `wght` axis, clamped to the face's
  range; ordinary static TTF/OTF fonts retain regular/synthetic bold support.
- Native SDL3 windows, default direct Vulkan composition,
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
    "index@luma@0.0.0[fonts]",
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

Run the minimal interaction reproducer from `demo/`:

```sh
cd demo
encore run
```

The complete interactive catalog is preserved in `showcase/` and runs with the
same command. `LUMA_TRACE_LATENCY=1` enables event/handling/submission timing for
managed applications; it is off by default.

## License

Luma is released under the MIT License. Bundled fonts are distributed under
their respective SIL Open Font License files in `assets/fonts/`.

### Timeline

`TimelineEditor` displays generic tracks, clip spans and point keys. The host owns
`TimelineDocument`, validates it before binding, and translates returned
`TimelineEdit` values into its own commands. Drag edits emit Begin/Preview/Commit
or Cancel; keyboard nudges emit Place as a single command. Seeking is separate
from editing. Locking, muting, frame snapping, anchored zoom, scrolling, trim
handles, keyboard navigation and a disabled state are included.

Use `widget()` for the focusable retained canvas, `input()` with its actual
layout bounds, and `display()` as a clipped foreground overlay. Supply surface,
accent and text styles from the application theme; feed `cursor()` to the owning
window. Tab leaves the timeline after the last item. Reset gestures when changing
documents, and restore the host's saved authoring values on Cancel.

### Inspection and diagnostic capture

`inspect_ui(layout, offset, limit, query)` returns visible widget IDs, labels,
roles and clipped logical bounds without triggering layout or paint.
`ReactiveUi.inspection_revision()` identifies the current geometry/hit paths.
`UiInputQueue` preserves ordered synthetic events; drain one event per ordinary
application event-loop iteration so application shortcuts, focus and gestures
follow their usual paths.

`Window.waker()` creates an owned `WindowWaker` whose integer handle can wake a
waiting window from another thread. Join those threads before destroying the
waker and then the window. For screenshots, call `Window.request_capture()`,
present a complete application frame, then call `Window.capture_png(crop,
maximum_width)`. The result contains PNG bytes, dimensions and frame number;
crop uses physical pixels and an empty rectangle captures the full client area.
Direct Vulkan capture records one color copy before presentation, including
external surfaces and overlays. Readback and encoding occur only on request;
Linux PNG compression loads the system zlib dynamically.
