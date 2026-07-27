# Animation

`ButtonAnimation` interpolates normal, hovered, and pressed styles. Keep
interaction animation short:

- hover: 70–90 ms;
- press: 40–60 ms;
- larger navigation transitions: 120–180 ms.

```enq
let mut animation = ButtonAnimation::new(80_u64)
animation = animation.with_hovered(true)

if animation.animating() {
    animation = animation.advance(elapsed_ms)
    ui.patch_style("save", animation.style(normal, hovered, pressed))
    ui = window.draw_reactive(ui)
}
```

Only the animated target should be patched. Applying one animation state to
every button and rebuilding the root each frame defeats reactive rendering.

When no animation is active, wait indefinitely for an OS event. While active,
wait until the next frame deadline rather than running a permanent polling
loop.
