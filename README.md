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
}

fn main() -> u32 {
    let window = Window::create("Counter", 320_u32, 180_u32)
    if !window.available() { ret 1_u32 }
    let _final = run_application(window, Counter { 0_u32 })
    ret 0_u32
}
```

## Features

- Standard controls, form widgets, icons, and state-aware hit testing
- Flexbox, grid, absolute and relative positioning, scrolling, and clipping
- CSS declarations, stylesheets, variables, selectors, and pseudo-classes
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
