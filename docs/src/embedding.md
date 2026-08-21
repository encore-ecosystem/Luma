# Embedding Luma

Luma owns presentation. A Vulkan scene renderer borrows:

- Vulkan instance;
- physical and logical device;
- graphics queue family.

The producer creates a sampled offscreen image on that device, transitions it
to `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`, and supplies it through
`ExternalVulkanImage`. `CompositeFrame` places it at a stable widget target and
redraws named overlays above it.

The producer retains image ownership and must clear Luma's wrappers with
`clear_external_vulkan_images()` before resizing or destroying those images.
Queue submission order synchronizes the producer and Luma without a CPU copy.

For renderers that record directly into Luma's presentation frame, use
`direct_graphics_interop`, `begin_direct_frame`,
`begin_direct_composition`, and `present_direct_frame`. Those APIs select the
compile-time native backend: Vulkan on Linux or Metal on macOS. The embedded
renderer must use the returned device and swapchain handles and must not submit
the borrowed frame itself; Luma performs the single submission after overlays
have been recorded.
