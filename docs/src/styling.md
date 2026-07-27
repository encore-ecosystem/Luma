# Styling and CSS

Styles can be built through typed methods or CSS declarations.

```enq
let typed = Style::default()
    .with_background(Color::rgb(18_u8, 153_u8, 126_u8))
    .with_foreground(Color::rgb(255_u8, 255_u8, 255_u8))
    .with_radius(6.0_f32)

let css = css_style(
    "background: #12997e; color: #ffffff; " +
    "border: 1px solid #0d745f; border-radius: 6px;"
)
```

`StyleSheet` supports selectors, classes, IDs, attributes, combinators,
pseudo-classes, variables, and the cascade:

```enq
let sheet = StyleSheet::parse(
    "button.primary { background: #12997e; } " +
    "button.primary:hover { background: #0d745f; }"
)
let styled = sheet.apply(root, hovered, active, focused)
```

Do not rebuild and lay out the whole application for a hover color. Restyle or
patch the affected widget and present it through `ReactiveUi`.
