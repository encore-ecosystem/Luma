# Text editing

`TextEditor` stores the text value, cursor, and selection anchor. It supports
insertion, deletion, clipboard-oriented selection, line navigation, and
selection extension.

```enq
let editor = TextEditor::new("Hello")
let anchor = editor.with_cursor(1_usize)
let selected = anchor.with_selection(anchor.selection_anchor(), 4_usize)
```

Render its state through an input widget:

```enq
Widget::textarea("editor", editor.value())
    .with_selection(editor.selection_start(), editor.selection_end())
    .with_focus(true)
```

For pointer selection:

1. On pointer-down, use `Window::text_index` or `wrapped_text_index`.
2. Set the cursor and retain `selection_anchor()`.
3. While the pointer is down, recalculate the cursor and call
   `with_selection(anchor, cursor)`.
4. Patch the widget and present the reactive frame.

Use `event_text_input` for inserted text. Backspace, Delete, arrows, Home, End,
and shortcuts are handled from key-down events.
