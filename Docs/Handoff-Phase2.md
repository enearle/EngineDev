# Handoff: Phase 2 (compute and GPU data)

Written 14 November 2026 after reviewing the Phase 0 and Phase 1 commits (`cede711`, `f37dc8d`) on
`Dev-2`. Read these first, in this order:

1. `CodingStandard.md` (root). Follow it. Comments only for external API usage and complex spots.
2. `Docs/RendererPlan.md`. Keep its Status table, outcomes and open items current as you go.
3. This file.

Branch is `Dev-2`. Commit there. Do not push. Two commits: "Phase 1 corrections" (step 1 below) and
"Phase 2" (everything else). The ground rules in `Docs/Handoff-Phase0-Phase1.md` still apply: the
layers stay separate, both backends hold at each gate, and verification is by build output and
captured stdout/stderr, not screenshots.

## Review of Phase 0 and Phase 1

Checked on 14 November 2026: the Debug solution builds, and Standalone and Editor `-play` run on
both backends, with and without `-stress`. Every run exits 0. Vulkan stderr is empty. D3D12 stderr
holds only "DirectX debugging enabled." and the known Noesis message (13 copies). `-stress` draws
10,014 objects in 14 batches per pass, 29 draw calls. Both gates hold as recorded.

The code matches what `RendererPlan.md` says landed. The findings below are from reading; none of
them shows up in the runs above.

### Fix first (step 1)

These sit under what Phase 2 builds on, so they go in before any new feature.

1. **D3D12 uploads use two fence schemes on one fence.** `DirectX12BufferAllocator::CreateBuffer`
   signals a static counter that starts at 1 (`RHI/RHI/BufferAllocator.cpp:953`).
   `SubmitTransferAndWait` signals the completed value plus one (`:1113`). `Signal` sets the fence
   to the value given, so after a few image uploads a buffer upload signals a lower value, sees the
   old higher value, and skips the wait. The next upload then resets the transfer allocator
   (`:929`) while its list may still be running. `Game.cpp` loads materials before meshes, so the
   mesh uploads on D3D12 race this way today. Separately, `D3DCore::BeginFrame` resets the
   transfer list every frame and `EndFrame` executes it empty (`DirectX12/D3DCore.cpp:104`, `:112`),
   which is why an upload during a frame fails (the Phase 1 open item).
2. **The two global samplers differ between backends.** D3D12: clamp, no anisotropy, visible to
   the pixel stage only (`RHI/RHI/PipelineLayout.cpp:171`). Vulkan: repeat, 16x anisotropy
   (`Vulkan/VulkanCore.cpp:386`). The images match only because the test UVs stay inside 0 to 1.
   A compute shader cannot sample at all on D3D12.
3. **Vulkan requires `VK_EXT_descriptor_buffer` and never uses it** (`Vulkan/VulkanCore.h:68`,
   `VulkanCore.cpp:170` to `:221`). Device creation throws on a GPU without it. Phase 1 moved to
   ordinary descriptor sets, so the extension, the feature struct and the three function pointers
   can go.
4. **Frame times are not comparable across backends.** D3D12 presents with vsync
   (`D3DCore.cpp:118`); Vulkan picks mailbox, which is uncapped (`VulkanCore.cpp:772`). The plan's
   "32 ms on D3D12 and 55 ms on Vulkan" is a vsync-quantised number against a free-running one. In
   Debug today the normal scene reads 0.9 ms on Vulkan and 6.0 ms on D3D12 (one refresh interval).
5. **Memory access is a magic number and D3D12 ignores half of it.** `MemoryAccess(8)`, `(9)` and
   `(1)` appear across `RHIConstants.h` and `BufferAllocator.cpp`. `DXMemoryType` always returns
   the default heap (`RHI/RHI/RHIStructures.cpp:704`) and `CreateBuffer` looks only at CPU write,
   so a CPU-readable buffer cannot be made on D3D12. Readback needs it.
6. **Vulkan descriptor pools never grow.** Each layout and set index gets one pool of 256 sets
   (`BufferAllocator.h:145`). The 257th material throws. Mip generation allocates a set per mip.
7. **Vulkan throws when the swapchain is out of date or suboptimal** (`VulkanCore.cpp:1063`,
   `:1127`). Resize works because the window event recreates the swapchain first. Any other cause
   ends the program.

### Where Phase 1 stops, and Phase 2 continues

Not defects. These are the edges the design below extends.

- One view per image and whole-image transitions (`PipelineExecutor.cpp:117`, `:394`). Upload
  covers one mip and one layer.
- The D3D12 storage image view is mip 0 of a 2D texture (`BufferAllocator.cpp:1365`). A storage
  buffer is a read-only SRV on D3D12. `ImageLayout::General` maps to `COMMON` on D3D12
  (`RHIStructures.cpp:614`), so there is no unordered-access state.
- The executor binds sets and pushes constants at the graphics bind point only. On Vulkan the
  `ShaderReadOnly` stage mask is vertex and fragment (`PipelineExecutor.cpp:293`).
- The pass graph knows images only, always creates them sampled plus attachment
  (`Renderer/PassGraph.cpp:50`), has no compute kind, and cannot take an image it did not create.
- `UpdateDescriptorSet` rewrites in place. That is safe only after `WaitForGpu`, which resize does
  (`Renderer.cpp:265`). New code allocates a new set and frees the old one instead.

### Carried, not part of Phase 2

Add these to the open items in `RendererPlan.md`.

- Colour space: textures and the back buffer are `UNORM`, the lighting shader clamps and there is
  no tone mapping. Mips are therefore filtered on stored values. sRGB formats cannot be storage
  targets without typeless or mutable formats. Phase 4 decides.
- Every buffer and image is its own device allocation.
- The lighting pass squeezes the full-window G-buffer into the Editor's game viewport.
- `RHIStructures.h` exposes `d3d12.h` and Vulkan types to every layer.
- `D3DPipelineExecutor::GetBackBuffer` uses its own frame counter, not
  `GetCurrentBackBufferIndex`.

## Phase 2 design

Gate from the plan: a compute pass writes a texture, a raster pass samples it, and the CPU reads it
back two frames later. On both backends with validation clean.

One refinement. Three frames are in flight, so "two frames later" cannot be a fixed number without
stalling. The readback completes when the frame that recorded the copy has finished on the GPU,
found by polling, never by waiting. The gate test prints the measured latency; it should read 1 to
3 frames.

Change these decisions if you find a reason, and say why.

### RHI

- **Upload context** (finding 1). One per backend with its own allocator or command buffer and a
  fence that only counts up: begin, record copies, submit and wait. `CreateBuffer` and
  `CreateImage` both use it. Delete the per-frame transfer list from `D3DCore`. An upload is then
  safe at any time, including while a frame is being recorded.
- **Sampler table** (finding 2). One list of `SamplerDesc` (filter, address mode, anisotropy) in
  the RHI builds both the D3D12 static samplers, visible to all stages, and the Vulkan set 0. Four
  entries: `s0` linear repeat with 16x anisotropy, `s1` point clamp, `s2` linear clamp, `s3` point
  repeat. On Vulkan `SampledImage` becomes `VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE` and
  `DescriptorBinding::Sampler` is deleted, so set 0 is the only source of samplers. Material
  textures use `s0`; the shadow map and G-buffer reads use `s2` and `s1`.
- **Access and usage** (finding 5). Named `MemoryAccess` constants. A CPU-readable buffer is a
  `READBACK` heap on D3D12. `BufferType` becomes usage flags (vertex, index, uniform, storage), so
  a buffer can be storage plus transfer source now and storage plus vertex in Phase 3.
- **Formats.** Add `R8_UNORM`, `R16_UNORM`, `R16_FLOAT`, `R16G16_FLOAT`, `R16_UINT`, `R32_UINT`.
  `CreateImage` checks that the format supports the requested usage and throws a clear error.
- **Subresource views.** `DescriptorSetBinding`, `ColorAttachment` and `DepthAttachment` take a
  range: base mip, mip count, base layer, layer count, with zero counts meaning all. Vulkan caches
  one `VkImageView` per range on the image. D3D12 writes SRVs and UAVs with the range and claims
  extra RTV and DSV slots per range, released with the image.
- **Storage layout.** Add `ImageLayout::Storage`: `GENERAL` on Vulkan, `UNORDERED_ACCESS` on D3D12.
  `ImageTransition` takes the same range. A `Storage` to `Storage` transition is a UAV barrier on
  D3D12 and a memory barrier on Vulkan; today both backends drop a transition whose two layouts
  are equal. Add the compute stage to the Vulkan `ShaderReadOnly` mask.
- **Read-write storage buffers.** New `DescriptorType::ReadWriteStorageBuffer`: a structured UAV on
  D3D12 (default heap, `ALLOW_UNORDERED_ACCESS`), a storage buffer on Vulkan. Add
  `TransitionBuffers` with read, write, copy source and copy destination states. A slot number
  stays unique within a set across `t`, `u` and `b` registers, because DXC maps all three to the
  same Vulkan binding number.
- **Region copies** on the executor: `CopyBufferToImage` and `CopyImageToBuffer` with mip, layer
  and rectangle. The caller gives the row pitch in bytes. D3D12 requires a multiple of 256, so add
  one helper that returns the pitch for a format and width, and use it on both backends.
- **Compute pipelines.** `Pipeline::CreateCompute` from a layout and one shader. `BindPipeline`
  records whether the pipeline is compute; `BindDescriptorSet` and `PushConstants` then use the
  matching bind point. Add `Dispatch(x, y, z)`. The shader build step already maps a `cs_` file
  name to `cs_6_1` (`Tools/GenerateProjects.py:159`).
- **Completed frame number.** `PipelineExecutor::GetCompletedFrameNumber()` without blocking: the
  fence value on D3D12, `vkGetFenceStatus` per frame slot on Vulkan.
- **Timestamp queries.** One query set per frame slot. `BeginTimer` and `EndTimer` return an index.
  When a slot begins again its previous results are complete, so read them then. Results are three
  frames old, which is fine for an overlay.
- **Vsync** (finding 4). `GRAPHICS_SETTINGS.VSync`, on by default, off with `-novsync`, obeyed by
  both backends. Off means immediate or mailbox on Vulkan and `Present(0)` with tearing allowed on
  D3D12.
- **Pool growth and swapchain** (findings 6 and 7). Chain a new Vulkan pool when one fills; a set
  remembers its pool. On out of date or suboptimal, recreate the swapchain and skip the frame.
- **Remove `VK_EXT_descriptor_buffer`** (finding 3).

Every `RWTexture2D` in HLSL gets `[[vk::image_format("...")]]` matching the image, so the SPIR-V
format and the view agree. Read the previous mip through an SRV, not through the UAV: reading a
typed UAV is an optional feature on both APIs for most formats.

### Renderer

- **Pass graph.** Kept at whole-image granularity. Additions only:
  - `PassKind::Compute`: transitions, then the callback, with no rendering scope.
  - `StorageWrites` on `PassDesc`, beside `SampledReads`, each with a binding. They order passes
    the same way colour writes do and put the image in `Storage`.
  - `GraphImageDesc` gains `MipLevels` and `IsStorage`.
  - `ImportImage(handle, restingLayout)` for an image the graph does not own. It is never resized
    or destroyed by the graph, and is returned to its resting layout at the end of the frame.
  - `Order` breaks ties for every kind of pass, lowest first, then declaration order. Today it
    only orders overlays.
  - A pass may transition subresources itself through the executor, provided the whole image is
    back in the layout the graph gave it when the callback returns.
  - The graph wraps each pass in a GPU timer and reports it with the pass stats.
- **Buffers stay out of the graph in Phase 2.** The RHI supports them; a pass that writes one
  calls `TransitionBuffers` itself. Compute skinning in Phase 3 is the first client that needs
  ordering by buffer, and the declarations should be designed against it. This follows the plan's
  own warning about over-building the graph.
- **Readback** (`Renderer/Readback`). `Request(image, mip, rectangle, callback)` records a copy
  into a pooled CPU-readable buffer after the last pass that writes the image. Each frame it fires
  the callbacks whose frame number is at or below `GetCompletedFrameNumber()`.
- **Gate test** (`Renderer/Passes/ComputeTestPass`, enlisted by `-compute-test`). A compute pass
  writes a 256 x 256 `R8G8B8A8_UNORM` graph image whose texels are a function of the frame number.
  A raster pass samples it into a corner of the back buffer. A readback of one row checks the
  values against the same function for the frame that was requested and prints
  `Compute test: N readbacks matched, latency A to B frames` at shutdown, or the first mismatch.
  It uses only the public graph and readback API, like `DeferredPasses`.
- **Mip generation** (`Renderer/MipGenerator`), the first real client. Loaded textures are created
  with a full mip chain and storage usage; the upload fills mip 0. `Enqueue(image)` adds to a
  queue. One compute pass with a low `Order` drains a few textures per frame: for each mip,
  transition it to `Storage`, bind a set with the previous mip as an SRV and this mip as a UAV,
  dispatch a 2 x 2 box filter, transition back. Sets are freed through the deferred queue.
  `MaterialRegistry::Enlist` enqueues each texture.
- **Overlay.** The "Renderer" panel gains GPU milliseconds per pass, the vsync state and the
  mip queue length.

### Order of work

Build and run the cases under Verification after each step.

1. Corrections: upload context, sampler table, named memory access and readback heap, pool growth,
   swapchain recreate, `-novsync`, remove the descriptor buffer extension. The image must not
   change on either backend. Commit as "Phase 1 corrections".
2. RHI resources: formats, usage flags, subresource views and transitions, `Storage` layout,
   read-write storage buffers, buffer transitions, region copies.
3. Compute pipelines, bind points and `Dispatch`.
4. Pass graph additions.
5. Timestamp queries and the overlay column. Re-measure Release `-stress -novsync` on both
   backends and replace the frame times in `RendererPlan.md`.
6. Completed frame number and the readback service.
7. The gate test.
8. Mip generation.
9. Optional, see below: shadow maps without multiview.
10. `RendererPlan.md` (status, "Phase 2 outcome", open items), `README.md` and `SETUP.md` (new
    switches).

### Optional step 9: shadow maps without multiview

The shadow pass has a fixed view mask of four layers (`Renderer/Passes/DeferredPasses.cpp:17`).
Vulkan draws every caster four times for one light; D3D12 has no view instancing and fills layer 0
only. Once step 2 gives per-layer attachment views, the pass can render each shadow-casting light
into its own layer with its own culled draw list. That closes the D3D12 multiview open item
without `D3D12_VIEW_INSTANCING_DESC`, removes three wasted views on Vulkan, and lets `ViewMask`
leave `PipelineDesc` and `RenderingDesc`. It is not needed for the gate. Do it if steps 1 to 8
land cleanly; otherwise leave it as an open item.

## Verification

- Build and capture as in `Docs/Handoff-Phase0-Phase1.md`.
- Run after each step: Standalone Vulkan, Standalone `-dx12`, Editor `-play`,
  Editor `-play -vulkan`, and Standalone `-stress` on both. From step 7 add `-compute-test` on
  both.
- Pass condition: exit 0, and stderr empty apart from "DirectX debugging enabled." and the Noesis
  message.
- Step 1 and step 8 change how textures are sampled. Compare Vulkan against D3D12 with
  `-freeze-animation`; the mean difference should stay near the 0.55 out of 255 recorded for
  Phase 1.
- The gate is the `Compute test:` line on stdout from both backends.

## Decisions left to the owner

- Vsync defaults to on. Say so if the default should be off.
- Step 9 is in or out.
- The sampler table has four fixed entries. Phase 4's atlas needs point clamp, which is `s1`.

## Status

Done on 14 November 2026: "Phase 1 corrections" and "Phase 2" are committed on `Dev-2`, steps 1 to 9.
See "Phase 2 outcome" in `Docs/RendererPlan.md`. Vsync stays on by default, step 9 is in, and the
sampler table has the four entries above.
