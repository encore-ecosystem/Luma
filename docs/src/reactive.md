# Reactive rendering

`ReactiveUi` owns a persistent `WidgetLayout` and records the smallest class of
work required by a state change:

- paint-only patch: geometry is retained;
- subtree layout invalidation: only that subtree is laid out again;
- subtree replacement: unrelated ancestors and siblings are retained;
- structure invalidation: caller rebuilds the root.

## Start a reactive frame

```enq
let mut ui = window.reactive(view(model))
```

This lays out the initial tree, renders it into the window's cached texture,
and presents it.

## Patch paint-only state

```enq
ui.patch_text("status", "Saved")
ui.patch_checked("notifications", true)
ui.patch_value("progress", 0.75_f32)
ui.patch_style("save", hovered_style)
ui = window.draw_reactive(ui)
```

`draw_reactive` flushes pending subtree layout work, updates only dirty cache
regions, and presents the cached frame.

Handle window resize without reconstructing application state:

```enq
if event.kind() == event_resize() {
    ui = window.relayout_reactive(ui)
}
```

## Replace a component

```enq
ui.replace_subtree("settings-panel", settings_view(model))
ui = window.draw_reactive(ui)
```

The replacement is laid out inside the component's existing bounds. If the
change must affect parent or sibling geometry, invalidate the nearest stable
layout boundary:

```enq
ui.invalidate_layout("content")
ui = window.draw_reactive(ui)
```

## Paint versus layout

Safe paint-only changes include:

- foreground and background color;
- opacity;
- text when its existing bounds are intentionally retained;
- checked, focused, disabled, and slider values;
- border, shadow, and decoration when they do not change component sizing.

Invalidate layout when changing padding, margin, width, height, gap, flex/grid
properties, wrapping, or child structure.

If `needs_structure()` is true, provide the new root explicitly:

```enq
ui = window.rebuild_reactive(ui, view(model))
```

`draw_reactive` intentionally refuses to guess how application state maps back
to a root widget tree. It keeps presenting the previous valid frame until the
caller supplies the replacement.
