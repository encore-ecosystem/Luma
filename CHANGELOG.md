# Changelog

All notable changes to Luma are documented here.

## Unreleased

## 0.2.1 - 2026-08-16

- Published the direct Vulkan backend as a standalone companion package and
  excluded its duplicate workspace copy from consumer archives.

## 0.2.0 - 2026-08-16

- Added managed zero-copy `DirectRenderApplication` surfaces, explicit overlay
  composition, raw render-surface input, key-release delivery and safe
  secondary-button pointer lock for editor and engine viewports.

- Added typed HTML-like builders, pipe-closure event handlers, and full
  capture/target/bubble propagation with default cancellation.
- Added `StyleSheet::default`, file loading/reloading, window-level cascading,
  default control hover/active/focus/disabled styles, and a bounded retained
  computed-declaration cache.
- Added heap-owned `Signal`/`Computed` reactive primitives and the
  `examples/html-counter` end-to-end demo.
- Added model-owned managed text edit intents for text input, deletion, cursor
  navigation, pointer placement, drag selection, and clipboard shortcuts.
- Managed applications now receive close requests before deciding whether the
  window may close, plus unhandled key events for application commands.
- Added model-controlled background update intervals; returning zero preserves
  fully event-driven idle behavior.
- Added a native VT terminal surface with ANSI, 256-color and true-color cell
  rendering.
- Added captured pointer drag events for splitter widgets, allowing managed
  applications to implement resizable layouts without a custom event loop.
- Actionable controls now receive a visible background and border lift on
  pointer hover.
- Text areas with a `syntax` attribute render lightweight token colors while
  preserving the managed editing and selection contract.
- Hover and active visuals are now part of the retained declarative tree
  before reconciliation, preventing periodic model updates from alternating
  between base and hover frames.
- Added `IncrementalApplication` target patches. Hover/focus now restyle only
  affected retained widgets, event propagation follows the indexed ancestry
  path, and hot text edits can avoid rebuilding the complete application view.
- Encore, Markdown and TOML syntax profiles now use language-specific lexical
  categories and position colored spans from full source prefixes so spacing,
  selection and caret metrics remain identical.

## 0.1.1 - 2026-07-25

- Corrected the release archive layout for Encore index compatibility.

## 0.1.0 - 2026-07-25

- Initial public release.
- Added native SDL3 windows and rendering.
- Added widgets, layout, CSS styling, animation, interaction, SVG icons, font
  management, and file picker modules.
- Added bundled Lucide icons and four bundled font families.
