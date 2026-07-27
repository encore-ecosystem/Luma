# Layout

Luma supports vertical/horizontal flex layout, wrapping, grid tracks,
positioned children, scrolling, clipping, margins, padding, constraints, and
box sizing.

The simplest container is:

```enq
let mut column = Widget::panel("content")
    .with_direction(vertical())
    .with_style(css_style("padding: 16px; gap: 8px;"))
```

`with_flex(basis, grow)` controls the main-axis basis and growth:

```enq
sidebar.with_flex(240.0_f32, 0.0_f32)
content.with_flex(0.0_f32, 1.0_f32)
```

CSS layout declarations are supported:

```enq
css_style(
    "display: grid; grid-template-columns: 240px 1fr; " +
    "gap: 12px; padding: 16px; box-sizing: border-box;"
)
```

Use `overflow: hidden` when child painting and hit testing must be clipped.
Use stable component containers as reactive layout boundaries.
