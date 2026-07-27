# Application model

The normal Luma application implements `Application` and lets
`run_application` own the window loop, retained tree, focus, pointer states,
idle waiting, reconciliation, and presentation:

```enq
impl Application for Model {
    fn view(self: Self) -> Widget { ret view(self) }
    fn update(self: Self, event: AppEvent) -> Self {
        if event.is_action() && event.action() == "save" {
            ret save(self)
        }
        ret self
    }
}
```

Stable widget IDs are action IDs. The runtime is event-driven by default and
does not render periodic idle frames. `ApplicationOptions::animated(fps)`
delivers tick events only for applications that explicitly request them.

The lower-level APIs remain available for editors and engines that need to
coordinate their own command bus or external renderer.

`Window::wait(timeout_ms)` sleeps until the operating system supplies an event
or the timeout expires. Prefer it over polling and sleeping in application
code.

At the lower level, use a long timeout while idle and a short timeout only
when an active animation has a known next frame deadline:

```enq
let timeout = if animation.animating() { 8_u32 } else { 1000_u32 }
let event = window.wait(timeout)
```

Keep state changes separate from view construction:

```text
OS event → hit test → update model → patch/replace retained UI → present
```

Full `window.layout(view())` calls are appropriate after a resize, navigation
to a completely different screen, or a root-level structural change. They
should not be the default response to pointer movement.
