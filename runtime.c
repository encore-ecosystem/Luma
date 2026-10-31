#include "workspace/ui_native/backend.c"
#include "workspace/ui_native/vulkan_compositor.inc"
#include "assets/default_css.generated.h"
#include "workspace/ui_native/css_file.c"

#include <stdatomic.h>

/* Graph identities are process-wide; state and dependencies remain UI-owned. */
uint64_t luma_reactive_identity(void) {
    static _Atomic uint64_t next_identity = 1;
    return atomic_fetch_add_explicit(&next_identity, 1, memory_order_relaxed);
}

/* Cached UI surface packets and coherent GPU-local buffer placement with host fallback. */

/* Prepared surface packets and direct compositor error propagation. */

/* Retained batches pin evicted atlas snapshots until repaint and GPU retirement. */

#include "workspace/ui_native/inspection.inc"
