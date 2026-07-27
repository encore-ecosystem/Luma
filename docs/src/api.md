# API map

| Module area | Main types and functions |
| --- | --- |
| Application | `Window`, `Event`, event and key constants |
| Retained UI | `Widget`, `WidgetLayout`, `ReactiveUi` |
| Interaction | `InteractionState`, `TextEditor` |
| Layout | `FlexItem`, grid/layout helpers, `Rect`, `Insets` |
| Styling | `Style`, `Theme`, `StyleSheet`, `css_style`, `apply_css` |
| Animation | `ButtonAnimation`, easing functions, style interpolation |
| Icons | `IconManager`, `IconAsset`, `SvgDocument` |
| Fonts | `FontManager`, `FontFace` |
| Native integration | platform handle, GPU surface pixels, renderer name |

The package root re-exports the public API:

```enq
import luma::{Widget, Window, ReactiveUi, Style}
```

Use the source-level `pub` declarations as the exact API reference while Luma
is pre-1.0. This guide documents the intended workflows and invariants.
