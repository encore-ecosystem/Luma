# Getting started

## Requirements

- Encore 0.1.4 or newer
- SDL3 at runtime
- SDL3_ttf when loading TrueType or OpenType fonts

Luma dynamically loads SDL, so consumers do not need to reproduce its native
linker configuration.

## Add the dependency

For a published release:

```sh
encore add luma
```

When developing against a local checkout, use a workspace or path dependency
through Encore's normal package workflow. Do not copy Luma's native backend
into the application.

## First application

```enq
import luma::{
    AppEvent, Application, Widget, Window, run_application
}

struct Counter { count: u32 }

impl Application for Counter {
    fn view(self: Self) -> Widget {
        ret Widget::button("increment", "Increment")
    }
    fn update(self: Self, event: AppEvent) -> Self {
        if event.is_action() && event.action() == "increment" {
            ret Self { self.count + 1_u32 }
        }
        ret self
    }
}

fn main() -> u32 {
    let window = Window::create("Counter", 320_u32, 180_u32)
    if !window.available() { ret 1_u32 }
    let _final = run_application(window, Counter { 0_u32 })
    ret 0_u32
}
```

Luma automatically provides hover/press feedback, keyboard activation, focus
navigation, system-font setup, retained reconciliation, and blocking idle
waits. Use [`ReactiveUi`](reactive.md) directly only for custom engine loops.

## Run the component catalog

```sh
cd demo
encore run
```

A minimal managed example is available in
[`examples/counter-button`](../../examples/counter-button/src/main.enq).
A complete low-level retained-update example is available in
[`examples/reactive-counter`](../../examples/reactive-counter/src/main.enq).
