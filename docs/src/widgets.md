# Widgets

Every interactive widget should have a stable, unique ID. Reactive updates,
hit testing, focus, and subtree replacement all use this ID.

Common constructors:

| Constructor | Purpose |
| --- | --- |
| `Widget::panel(id)` | Container |
| `Widget::label(text)` | Static text |
| `Widget::button(id, text)` | Action |
| `Widget::input(id, value)` | Single-line editor |
| `Widget::textarea(id, value)` | Multi-line editor |
| `Widget::checkbox(id, text, checked)` | Boolean choice |
| `Widget::radio(id, text, checked)` | Exclusive choice |
| `Widget::select(id, value)` | Select control |
| `Widget::slider(id, value)` | Numeric value from 0 to 1 |
| `Widget::progress(value)` | Read-only progress |
| `Widget::icon(id, svg)` | SVG icon |
| `Widget::icon_button(id, text, svg)` | Action with icon |

Composition is explicit:

```enq
let mut row = Widget::panel("actions").with_direction(horizontal())
row.push(Widget::button("cancel", "Cancel"))
row.push(Widget::button("save", "Save"))
```

Use builder methods for style, direction, flex sizing, attributes, state, text
selection, and SVG content. Keep IDs stable across renders.
