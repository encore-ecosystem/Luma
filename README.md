# Luma

Luma is a cross-platform native UI toolkit for Encore. It provides widgets,
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

Then import the API from `luma`:

```enq
import luma::{Rect, Theme, Widget, layout_widget, vertical}

fn view() -> Widget {
    let theme = Theme::light()
    let mut root = Widget::panel("root")
        .with_direction(vertical())
        .with_style(theme.panel())
    root.push(Widget::button("save", "Save"))
    ret root
}

fn layout() {
    let tree = layout_widget(view(), Rect::new(
        0.0_f32, 0.0_f32, 800.0_f32, 600.0_f32))
}
```

## Features

- Standard controls, form widgets, icons, and state-aware hit testing
- Flexbox, grid, absolute and relative positioning, scrolling, and clipping
- CSS declarations, stylesheets, variables, selectors, and pseudo-classes
- Color and geometry primitives
- SVG path parsing and a bundled Lucide icon catalog
- Bundled Open Sans, Inter, Roboto, and Roboto Mono font families
- Native SDL3 windows, software rendering, clipboard, text input, and HiDPI
  support

The `fonts` feature is available for consumers that want the bundled font
assets:

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
