# Icons and fonts

`IconManager` loads SVG documents from a directory or the bundled Lucide
catalog:

```enq
let icons = IconManager::with_bundled("assets/icons")
let save = icons.document("save")
let button = Widget::icon_button("save", "Save", save)
```

SVG icons are scaled from their view box and rendered with anti-aliased round
caps and joins. Test important icons at 12, 16, 20, 24, 32, and 48 pixels.

`FontManager` registers font families with a window and activates a default
face. The optional `fonts` package feature includes the bundled Inter,
Open Sans, Roboto, and Roboto Mono assets.

Applications can also load their own font directory instead of shipping all
bundled families.
