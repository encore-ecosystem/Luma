# Slider interaction test plan

This document defines the regression matrix for `Slider` and `RangeSlider`.
The controls are correct only when their model value, retained visual value,
dependent labels, interaction state, and committed application state agree at
every observable event boundary.

## Invariants

1. A pointer event must produce at most one value change and must never restore
   an older retained value afterward.
2. While captured, each displayed frame uses the newest available pointer
   sample without smoothing, inertia, replaying a backlog, or dependence on
   unrelated focus events.
3. At every rendered frame, the thumb position and every dependent value label
   represent the same value.
4. Releasing anywhere after a captured press commits exactly once. Leaving the
   widget bounds must not discard persistence or the application action.
5. Capture ends only at an explicit boundary: pointer-up, focus loss, or a new
   conflicting primary down. A motion packet with an unreliable/no-buttons
   mask must neither cancel the active drag nor enqueue deferred movement.
6. Starting a new gesture cancels any previous visual settle transition and
   starts from the currently rendered value.
7. A range slider must always provide a way to select either thumb, including
   when both endpoints are equal.
8. Pointer and keyboard paths use the same clamp, step, snap, change, and commit
   semantics.
9. Disabled controls never change, focus, capture, or dispatch actions.
10. Resize, scale factor, frame rate, and background application ticks must not
    alter the logical value or interrupt capture.

## Layer 1: pure value tests

- Normalize pointer positions at the start, middle, end, and outside both ends.
- Clamp ordinary values to `[0, 1]`.
- Select the nearest range thumb on both sides and at the exact midpoint.
- Resolve equal endpoints using pointer direction or the last active thumb so
  both thumbs remain reachable.
- Prevent lower/upper crossing while allowing a collapsed range to reopen.
- Exercise tick snapping immediately before, at, and after every midpoint.
- Prove that magnetic preview values are continuous and committed values land
  exactly on a tick.
- Test zero width, one tick, two ticks, many ticks, and reduced motion.

## Layer 2: interaction state-machine tests

For each case assert event order, change count, commit count, active target,
focused target, captured thumb, model value, and retained value.

- Press and release the current thumb without motion.
- Click the track nearer and farther from the current thumb.
- Alternate between two distant track points with 0, 1, 2, 5, 8, and 16 ms
  gaps. Run both final directions and require the last press to win within one
  displayed frame, without replaying an older point afterward.
- Click minimum, maximum, and the same target repeatedly.
- Hold a track click for short and long intervals without motion.
- Slow, fast, one-pixel, and full-span drag.
- Pause for at least 500 ms while still holding the primary button, then resume
  motion. Capture both states and require the second half to continue from the
  held coordinate rather than ending capture or jumping only on release.
- Feed motion faster than refresh and verify that queued input is reduced to
  the newest coordinate before presentation, while sparse motion still paints
  every available step.
- Reverse direction several times in one captured drag.
- Move and release before/after both bounds.
- Release outside the widget and outside the window.
- Omit `pointer-up`, then receive motion with no buttons pressed. Capture must
  remain authoritative until focus loss or a new primary down ends it.
- Lose focus while held, then release while unfocused.
- Start a new drag during a snap/settle transition.
- Interleave background ticks and unrelated hover events with every phase.
- Attempt a second button or second pointer while the primary pointer owns
  capture; the original gesture must remain authoritative.

## Layer 3: keyboard and accessibility tests

- Focus with pointer and with forward/backward Tab traversal.
- Left/Right adjust one step; Home/End reach bounds.
- Range modifiers select lower versus upper thumb consistently.
- Keyboard changes dispatch the same live change and one final commit as the
  pointer path.
- Enter/Space do not accidentally reset a slider to the left edge.
- Disabled and unfocused controls ignore adjustment keys.

## Layer 4: retained rendering tests

- Capture before press, while held without motion, during drag, immediately
  after release, during settle, and after settle.
- At each capture compare the painted thumb coordinate to the model-derived
  coordinate and compare the visible label to the same model value.
- Sample a slow drag at several frames and require monotonic movement for a
  monotonic pointer path.
- Require a stationary pointer to produce identical consecutive frames.
- Verify hover, active, focus, disabled, and reduced-motion CSS states without
  changing control geometry or restoring stale widget data.
- Repeat at minimum/default/large window sizes and supported display scales.

## Layer 5: FastWord integration tests

- Display Time updates live while dragging but writes `state.tsv` only once on
  commit.
- A click without motion does not show a label/thumb mismatch.
- A release outside still persists the displayed duration.
- Word scheduling resumes from the newly selected duration without requiring a
  focus change.
- CEFR preview does not rebuild the complete settings card.
- CEFR commit resets the session exactly once and persists both endpoints.
- Collapsing a CEFR range does not trap either thumb.

## Real-input acceptance run

Use an emulated mouse and keyboard against the newly built application. Run
every state-machine equivalence class above, inspect the complete screenshots,
then perform at least one minute of continuous mixed interaction. Confirm the
tested PID and binary path, process liveness, persistence contents, backend
logs, and absence of crash dumps.

## Baseline findings (2026-08-10)

- PASS: slow/fast/reversing drag is continuous; no post-release inertia.
- PASS: values clamp visually at both bounds.
- PASS: missing release is recovered by a no-buttons motion event.
- PASS: a new range drag cancels an active settle transition.
- PASS: FastWord word scheduling continues after an ordinary slider commit.
- PASS: CEFR keyboard adjustment works in the application-specific handler.
- FAIL: click without motion restores a stale interaction snapshot while held,
  then jumps on release. The value label and painted thumb disagree.
- FAIL: release outside the widget changes the live value but does not dispatch
  the commit action, so persistence remains stale.
- FAIL: focus loss cancels capture without committing or rolling back the live
  model; a later model reconciliation causes a delayed jump.
- FAIL: equal range endpoints always select the upper thumb, trapping the
  collapsed range for pointer input.
- FAIL: ordinary sliders have no default arrow/Home/End keyboard adjustment in
  the Luma runtime.

## Fixed-build verification (2026-08-10)

- PASS: a held track click and its release paint the same thumb coordinate and
  display the same value; no stale interaction snapshot is restored.
- PASS: releasing beyond the window commits the retained live value to the
  application state.
- PASS: focus loss commits the retained live value, ends capture, and does not
  cause a later reconciliation jump.
- PASS: a collapsed CEFR range can be reopened in either direction by the
  first directional pointer motion.
- PASS: Arrow, Home, End, and range-thumb keyboard adjustment update and
  commit through the same control path; Enter/Space cannot reset a slider.
- PASS: FastWord continued advancing words without a focus change after a
  Display Time interaction.
- PASS: one minute of continuous pointer and keyboard interaction completed
  with the tested process alive, no stderr/backend error, and no crash dump.

Evidence is stored in FastWord's `target/validation` directory. The composite
`final-slider-regression-matrix.png` covers the discrete interaction matrix;
`final-11-timer-after-slider.png` and `final-12-timer-advanced.png` prove timer
progress; `final-13-after-soak.png` is the complete frame after the soak.

## Burst/drag follow-up verification (2026-08-10)

- PASS: the user-equivalent two-point sequence at 0, 1, 2, 5, 8, and 16 ms
  gaps converges to the final point in 3 ms in both directions.
- PASS: the deliberately excessive 24-click burst still preserves the final
  point in both directions and drains within 3–64 ms after input generation.
- PASS: retained updates from one native event are batched into one present;
  queued input is handled before CSS/range transition frames.
- PASS: a 2-second drag sampled across 16 complete frames moves monotonically
  and does not pause before jumping to the release coordinate.
- PASS: a drag paused for 600 ms while held remains captured and resumes from
  2.5 s to 4.7 s before committing 4792 ms.

Additional evidence: `fixed-slider-burst.tsv`,
`fixed-slider-two-point.tsv`, `fixed-slider-drag-rates.tsv`,
`fixed-drag-contact-sheet.png`, and `fixed-paused-drag-comparison.png` in
FastWord's `target/validation` directory.

## Frame-coalesced reducer verification (2026-08-10)

- PASS: Luma retains only the newest timestamped pointer coordinate and emits
  at most one application preview plus one present per displayed frame.
- PASS: one rapid two-point gesture, both final directions, reached the final
  point; 2, 4, 6, 8, 12, 16, 20, 24, and 32 zero-gap synthetic clicks also
  preserved the final point after queue drain.
- PASS: a 30-reversal, 1 ms/sample drag showed both directions during capture
  and committed its final coordinate without post-release movement.
- PASS: a 600 ms stationary hold kept capture and its value, then resumed from
  that coordinate instead of teleporting on release.
- PASS: focus loss committed once; the later unfocused release did not change
  the value again.
- PASS: FastWord changed the visible Display Time preview without writing
  `state.tsv`; release committed the exact visible value. Word scheduling then
  advanced without an additional focus event.
- PASS: CEFR preview remained continuous and release settled on a ruler mark.
- PASS: 62.854 seconds of continuous mouse drags, range manipulation, and
  keyboard adjustment completed with PID 47803 alive, no core dump, and no
  backend error.

Evidence is in FastWord's `target/validation/systematic-*` files. The key
visual summaries are `systematic-oscillate-controls.png`,
`systematic-paused-controls.png`, `systematic-20-timer-start.png` through
`systematic-22-timer-advanced-again.png`, and
`systematic-26-after-soak.png`.

## Stationary pending-sample regression (2026-08-10)

- PASS: one motion immediately after pointer-down, followed by 1.2 seconds of
  complete pointer stillness, painted its final thumb and `3.7 s` label while
  still held. No release, focus event, or second motion was needed.
- PASS: `state.tsv` remained at `5000` during that held preview and committed
  `3713` only on release.
- PASS: ordinary Slider input now uses the same six-pixel inset as its painted
  track, keeping pointer and thumb coordinates identical across the span.

Evidence: FastWord `target/validation/display-time-01-single-motion-held.png`,
`display-time-02-single-motion-committed.png`,
`display-time-single-motion.tsv`, and
`display-time-zigzag-controls.png`.
