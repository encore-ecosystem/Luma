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

## Variable weights

Use `Style::default().with_font_weight(425_u32)`, CSS `font-weight: 425`, or
`FontManager::activate(window, family, size, 425_u32)`. Weights are integer
values in 1–1000, without rounding to named hundreds. CSS also accepts `normal`
(400) and `bold` (700). A variable face clamps the requested weight to its own
`wght` range and uses real outlines, not synthetic emboldening. Other variation
axes retain their defaults. Italic variable files retain their italic outlines
when explicitly registered as a separate family; this does not add a CSS
`font-style` property.

SDL3_ttf does not expose an axis setter. Luma therefore uses the public
[HarfBuzz instancing API](https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-subset-input.cc)
to create font instances in memory. Install HarfBuzz **including the subset
library**, version 6.0 or newer, alongside SDL3_ttf. Missing instancing support
is reported as an error rather than silently ignoring the requested weight.
All glyphs and layout features are retained; there is no font-file generation
on disk and no shell command or Python dependency at runtime.

Each window caches up to 16 instance blobs / 16 MiB, independently of text size,
and up to 32 opened size/weight variants. Open fonts own references to their
bytes even after cache eviction. A pixel-density change recreates opened
variants while reusing the size-independent blobs. Destroying or clearing the
window's fonts releases both caches. Live opened fonts can retain blobs beyond
the instance-cache budget. Individual font data is limited to 64 MiB.

Ordinary static TTF/OTF fonts remain supported, with the existing synthetic-bold
behavior for requested weights of 600 or above.

Focused validation from the repository root:

```sh
encore test --filter font_weights --jobs 1
clang -std=gnu11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
  tests/font_instances.c $(pkg-config --cflags --libs sdl3 sdl3-ttf harfbuzz) \
  -ldl -o target/font-instances-test
ASAN_OPTIONS=detect_leaks=1 target/font-instances-test
```

The native Linux test verifies intermediate weights on all bundled faces,
size-independent reuse, range clamping, static fonts and ownership on eviction.
It writes a rendered specimen to `target/variable-font-weights.bmp`.
