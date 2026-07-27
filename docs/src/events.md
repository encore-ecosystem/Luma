# Events and input

`Event` exposes its kind, pointer coordinates, key, modifiers, wheel delta,
and committed text input.

Use `InteractionState` for consistent hover, active, and keyboard focus:

```enq
if event.kind() == event_pointer_move() {
    interaction = interaction.pointer_move(ui.layout(), event.x(), event.y())
}
if event.kind() == event_pointer_down() {
    interaction = interaction.pointer_down(ui.layout(), event.x(), event.y())
}
if event.kind() == event_pointer_up() {
    let action = interaction.pointer_release_target(
        ui.layout(), event.x(), event.y())
    interaction = interaction.pointer_up(ui.layout(), event.x(), event.y())
}
```

Apply an action only when `pointer_release_target` matches the element pressed
on pointer-down. This prevents a drag outside a button from activating it.

For keyboard navigation, call `focus_next` or `focus_previous` with the retained
root. Enter and Space can resolve through `focused_action`.

Text entry arrives through `event_text_input`; key-down is for navigation and
commands, not for converting key codes into characters.
