# Testing and troubleshooting

Validate a Luma checkout with:

```sh
encore format --check
encore check
encore lint
encore test

cd demo
encore check
encore build
```

Build the documentation with:

```sh
mdbook build docs
```

## SDL cannot be loaded

Check `backend_error()` and confirm that SDL3 and SDL3_ttf are installed for
the target architecture.

## UI feels delayed

- verify that pointer movement does not call `window.layout` for the root;
- use `ReactiveUi` and `draw_reactive`;
- patch only the changed target;
- inspect `window.renderer_name()` for an unexpected `software` backend;
- avoid animation durations above approximately 100 ms for hover feedback.

## A partial update paints incorrectly

Use `invalidate_layout` when geometry changes. Use subtree replacement when
children change. Rebuild the root when `needs_structure()` is true.
