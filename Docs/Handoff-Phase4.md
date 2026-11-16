# Handoff: Phase 4 (materials and paging)

Written 16 November 2026 after reviewing the Phase 3 commits (`049c42d`, `afd3572`, `fb74a83`,
`1b3862e`) on `Dev-2`. Read these first, in this order:

1. `CodingStandard.md` (root). Follow it. Comments only for external API usage and complex spots.
2. `Docs/RendererPlan.md`. Keep its Status table, outcomes and open items current as you go.
3. The cache design linked from "Gaps against the cache system" in that file. Phase 4 builds the
   generic half of it; Phase 5 builds the procedural half on top.
4. This file.

Branch is `Dev-2`. Commit there. Do not push. Three commits: "Phase 3 corrections" (step 1),
"Phase 4: materials" (steps 2 and 3) and "Phase 4: paging" (steps 4 to 7). The ground rules in
`Docs/Handoff-Phase0-Phase1.md` still apply: the layers stay separate, both backends hold at each
gate, and verification is by build output and captured stdout/stderr, not screenshots.

## Review of the work so far

Checked on 16 November 2026: the Debug solution builds, and twelve runs pass. Standalone and
Editor `-play`, `-stress -novsync`, `-compute-test`, `-animation-test` and `-crowd 100`, each on
both backends, with the character in `TempAssets`. Every run exits 0. Vulkan stderr is empty.
D3D12 stderr holds only "DirectX debugging enabled." and the Noesis message.

- Phase 1 gate: 10,032 objects in 32 batches per pass.
- Phase 2 gate: 760 readbacks matched on Vulkan, 579 on D3D12, latency 1 to 3 frames.
- Phase 3 gate: "bind pose identity for 144 joints" and "256 vertices matched", largest error
  4.6e-05, on both. `-crowd 100` runs 21 copies and cross-fades on both.

The Phase 3 code matches its outcome section. The skeleton import, the single matrix convention,
the pass-graph buffers and the skinning pool read correctly, and the three RHI features that had
no client now have one.

### Findings

1. **Noesis draws into the last pass's viewport on Vulkan.** This is the bug the owner saw in
   `-compute-test`, confirmed from a capture: the whole Noesis view is squeezed into the 256 x 256
   test square. `VulkanPipelineExecutor::BeginOverlayPass` sets no viewport
   (`RHI/RHI/PipelineExecutor.cpp:901`); the D3D12 one does (`:447`). The Vulkan branch of
   `NoesisRenderOnscreen` sets none either (`Engine/UI/NoesisUILayer.cpp:158`); the D3D12 branch
   does (`:176`). So Noesis inherits whatever the previous raster pass left. That is normally the
   lighting pass, which is why it looked right, and why Noesis lands in the Editor's game viewport
   on Vulkan. "Compute test view" runs after lighting and leaves its corner viewport behind.
   Dear ImGui sets its own viewport and is unaffected.
2. **The process is not DPI aware.** Standalone reports a 1280 x 720 swapchain while its client
   area measures 1600 x 900 on this 125% display, so Windows stretches every frame. Nothing in
   the solution declares DPI awareness. Texture sharpness cannot be judged through a 1.25x
   stretch, and Phases 4 and 5 are about texture sharpness.
3. **`CopyBufferToImage` still has no client.** Page uploads are its first use. Copying from the
   frame ring also needs transfer-source usage on the ring, which it lacks
   (`RHI/RHI/BufferAllocator.cpp:103`).
4. **Vulkan does not enable `fragmentStoresAndAtomics`** (`Vulkan/VulkanCore.cpp:175`). Writing
   the feedback buffer from a pixel shader needs it.
5. **`PassGraph::ImportImage` assumes the image is already in its resting layout**
   (`Renderer/PassGraph.cpp:50`). An atlas created empty is `Undefined`.
6. **A material is three fixed textures found by file name.** `Material.cpp` builds one of two
   hard-coded layouts, the scene layout fixes set 2 at three sampled images
   (`Renderer/Renderer.cpp`), and the importer ignores `aiMaterial`, so game code assigns
   materials by list order (`Game/Game.cpp:203`). The character therefore wears the Derp's
   material on all seven meshes.
7. **Lighting runs on encoded colour.** Albedo textures are sampled as `UNORM`, the lighting
   shader works on those values and clamps, and the result goes to a `UNORM` back buffer with no
   encoding. `RendererPlan.md` left this for Phase 4 to decide.
8. **The committed outcome and the "What exists today" table disagree about clip import.** The
   outcome says the gate ran with two clips from a scratch export; the table says clip import
   from a file is untested. The default `CharBlend.fbx` has no clips and the scratch export is
   not kept, so today's runs use the clips built in code. Keep the scratch export in `TempAssets`
   or correct the table.

### Carried, not part of Phase 4

- Vulkan device selection requires a discrete GPU (`Vulkan/VulkanCore.cpp:820`).
- Every character is animated and skinned whether or not a view sees it, and the skinning pool
  caps a crowd of this character at 21.
- Every launch re-imports the FBX files and rewrites their `.asset` files. The page file in step
  6 is built once and reused, which is the pattern cooked assets should follow.
- `ImageAsset` and `MaterialAsset` are still stubs. Phase 4 does not finish them.
- Flexible vertex formats, a second UV set and non-overlapping UVs for the character: Phase 5.
- The open and carried items under "Phase 2 outcome" and "Phase 3 outcome".

## Phase 4 design

Gate from the plan: a large texture streams in page by page as the camera approaches, inside a
fixed atlas. Made concrete:

1. Material types are data: a surface shader, a parameter block and texture bindings. Adding a
   type needs no edits to the G-buffer pass, the draw list or `Renderer`. Three types exist:
   today's textured PBR, a flat one with parameters only, and one whose albedo is a paged texture.
2. `-paging-test` flies a scripted camera at a plane textured with a 16,384 x 16,384 page file
   through a 256-slot atlas and prints one line on stdout: pages filled and evicted, peak
   resident pages (never above the slot count), the longest wait from request to resident, the
   number of filled pages whose atlas texels matched the file on readback, and whether the final
   view was fully resident.
3. On both backends with validation clean.

Change these decisions if you find a reason, and say why.

### Corrections (step 1)

- **Overlay viewport** (finding 1). `VulkanPipelineExecutor::BeginOverlayPass` sets the full
  viewport after it begins the render pass, as D3D12 does. The Vulkan branch of
  `NoesisRenderOnscreen` then sets the game viewport when one is set, as its D3D12 branch does,
  or the Editor regresses. Use the executor's `SetViewport`: it flips Y on Vulkan, and Noesis
  draws correctly with that flip today.
- **DPI** (finding 2). Declare per-monitor DPI awareness before the window is created in
  Standalone and Editor. The window is then 1280 x 720 physical pixels and looks smaller at
  125%; scale the Dear ImGui font by the monitor's factor.
- **Ring and feature** (findings 3 and 4). Transfer-source usage on the frame ring. Require and
  enable `fragmentStoresAndAtomics` on Vulkan.
- **`ImportImage`** (finding 5) takes the layout the image is in now as well as its resting
  layout.

### Materials

- **Surface interface.** `Shaders/Surface.hlsli` declares `SurfaceInput` (world position, normal,
  tangent, bitangent, UV, pixel position) and `Surface` (linear albedo, world normal, metallic,
  roughness, occlusion), and one function a type must define:
  `Surface EvaluateSurface(SurfaceInput input)`. `ps_gbuffer.hlsl` includes the type's file,
  calls it and packs the G-buffer. `ps_pbr.hlsl` goes away.
- **Build.** Each file in `Shaders/Surfaces` compiles through `ps_gbuffer.hlsl` with a define
  naming it, into `ps_gbuffer_<name>`. Teach `Tools/GenerateProjects.py` this and rerun it.
- **Material type.** A name, its G-buffer pipeline, the named fields of its parameter block and
  its named texture slots. Types are registered at startup; they are content of the build.
- **One material set for every type**, so the scene layout stays shared by the shadow and
  G-buffer pipelines. Set 2 is one dynamic uniform buffer of 256 bytes (slot 0) and eight sampled
  images (slots 1 to 8). Unused image slots hold a 1 x 1 default image. The parameter block is
  copied into the frame ring for each material used in a frame and bound with its offset, so
  parameters can change at any time with no synchronisation.
- **Material.** A type, parameter values and textures, read from `Engine/Materials/<name>.material`,
  a text file of `key = value` lines. With no file, `MaterialRegistry::Enlist(name)` falls back to
  today's file-name convention as a textured PBR material, so existing content keeps working.
  `Material::MaterialFormat`, `PreBufferCache` and the duplicated loading code are deleted.
- **Draw list.** The batch key puts the material type above the material, so batches sort by
  pipeline first, and the G-buffer pass binds a pipeline when the type changes.
- **Import** (finding 6). `MeshAsset` stores each submesh's material name from `aiMaterial`, and
  `MeshComponent` can assign materials by name. The character gets flat materials until its
  textures are supplied.
- **Types.** `textured_pbr` must reproduce today's image exactly. `flat` has albedo, metallic and
  roughness parameters and no textures. `paged_albedo` arrives in step 4.

### Colour (step 3)

Lighting moves to linear colour, after step 2 has been shown to leave the image unchanged.

- `Surface::Albedo` is linear. `textured_pbr` decodes its albedo texture with the sRGB curve.
- The G-buffer keeps albedo encoded in 8 bits: `ps_gbuffer` encodes, the lighting shader decodes.
- The lighting shader encodes its result for the `UNORM` back buffer. It still clamps; a
  floating-point target and tone mapping stay open.
- `cs_downsample` decodes, averages and encodes when a texture is marked as colour.

This changes the image, so take new reference captures on both backends afterwards.

### Paged textures: `Renderer/Paging`

One service, started by `Renderer` like `Skinning` and `Readback`. Producers plug into it.

- **Addressing.** A paged texture is a quadtree of 128 x 128 pages. Level 0 is one page covering
  the whole texture; level L has 2^L pages a side. A key is texture, level, x and y, with x and y
  32 bits wide so Phase 5 can go as deep as it likes.
- **Atlas.** A grid of slots, each a page plus a 4-texel gutter on every side, so bilinear
  filtering never reads a neighbour. The atlas takes a list of image formats that share the slot
  grid; Phase 4 uses one `R8G8B8A8_UNORM` image, and Phase 5 adds its height, slope and pigment
  images without changing the service. Default 256 slots (16 x 16, 2176 x 2176, 19 MB).
- **Page table.** This settles the cache design's open question in favour of a hash, because
  Phase 5 needs unbounded depth and several close-up regions at once. The CPU owns the table. Each
  frame it writes a GPU copy into the frame ring: open addressing, four times as many entries as
  slots, an entry of four words (x, y, texture and level, slot), and a guarantee from the CPU that
  every key sits within four steps of its hash. A page that cannot be placed within four steps
  waits a frame. Writing the copy every frame means each frame in flight reads a table that
  matches the atlas contents it will see. A second array holds two user words per slot, for
  Phase 5's height range.
- **Lookup.** `Shaders/Paging.hlsli` holds the only code that knows the table's form:
  `SamplePagedTexture(texture, uv, ddx, ddy)`. It takes the wanted level from the pixel
  footprint (texels covering one to two pixels), probes that level, then parents, for at most
  four levels, and samples the atlas once with bilinear filtering (`s2`). A windowed table can
  replace the hash later by changing this file.
- **Feedback.** A device-local read-write buffer of two words per 8 x 8 pixel block. The G-buffer
  pixel shader writes the wanted key for one pixel of each block, chosen by a jitter that changes
  every frame. A small compute pass clears it first. `Readback::RequestBuffer` returns it one to
  three frames later. There is no separate feedback pass: the stress scene is bound by vertex
  work (G-buffer 16 ms), and a second geometry pass would nearly double that.
  - A pixel shader that writes a buffer loses early depth testing unless it asks for it, so
    surface types that write feedback are compiled with `[earlydepthstencil]`. Without it hidden
    surfaces would also request pages.
  - The page table, the feedback buffer and the atlas images form set 3 of the scene layout, the
    "paging set", allocated by the service and bound by the G-buffer pass with the scene set.
  - `Renderer` exposes the graph images and buffers every surface pass must declare. The service
    fills that list; `DeferredPasses` reads it at `Enlist` and stays ignorant of paging.
- **Each frame on the CPU**, after `Readback::DeliverCompleted`:
  1. Decode the feedback into request counts per key. Stamp resident pages and their ancestors
     as used.
  2. For each missing key, schedule the coarsest missing ancestor first. A page is filled only
     when its parent is resident, and level 0 of every texture is pinned. Phase 5 bakes from the
     parent, so this is a guarantee, not an optimisation.
  3. Fill at most a budget of pages per frame (default 16).
  4. When no slot is free, evict the least recently used page that is not pinned, has no resident
     child and was not used this frame. If there is none, fill nothing.
  5. Write the GPU table.
- **Producers.** `PageProducer` has four calls: `Begin(job)` starts work and may be asynchronous,
  `IsReady(job)`, `Fill(executor, job, slot)` runs inside the service's fill pass and either
  copies texels or dispatches compute, and `Cancel(job)`. The fill pass is one compute pass before
  the G-buffer that declares the atlas as a storage write and does its own copy transitions.
- **File producer.** A page file is a header (page size, gutter, format, level count), a level
  directory, and tiles of 136 x 136 raw texels, coarsest level first. One IO thread reads tiles
  into pooled buffers. `Fill` copies a tile into a ring allocation and calls `CopyBufferToImage`
  with `CopyRowPitch` and `CopyPlacementAlignment`. `PageFile::Build` cuts any image `stb_image`
  can load, with box-filtered mips; `PageFile::BuildTestPattern` writes a 16,384 x 16,384 pattern
  whose texels encode level, x and y, tile by tile, so it never holds the image in memory. Both
  write into a gitignored folder and are skipped when the file is already there and newer than
  its source. The test file is about 1.6 GB; say so in `SETUP.md`.
- **`paged_albedo` type.** Albedo from a paged texture, metallic and roughness from parameters.
- **Overlay.** A "Paging" panel: textures, resident pages against slots, requests, fills and
  evictions this frame, IO queue length, pages that could not be placed, feedback latency, and
  GPU milliseconds of the fill pass. `-paging-tint` colours each pixel by the level it hit.

Known limits to record as open items: bilinear filtering of one level only, so no anisotropic
filtering and a visible step when a finer page arrives; no fade-in of new pages; one feedback
sample per block per frame, so a material with several paged textures takes turns.

### Tests

- **`-paging-test`.** Builds the test file if it is missing, shows it on a plane, and runs a
  scripted path: the whole plane in view, a dolly to a close-up, a pan, and a pull back. For up to
  300 fills it reads the slot back from the atlas and compares it with the tile. At exit it prints
  `Paging test: 16384 x 16384, 21845 pages, 256 slots; F filled, E evicted, peak P resident;
  longest wait W frames; V pages verified; final view fully resident` or the first failure.
- The paging service does nothing when no paged texture is enlisted: the fill and clear passes
  return at once.

## Order of work

Build and run the cases under Verification after each step.

1. Corrections. Commit as "Phase 3 corrections".
2. Surface interface, material types, the material set, material files, the batch key, material
   names from import, `textured_pbr` and `flat`. The image must not change.
3. Colour, then new reference captures. Commit steps 2 and 3 as "Phase 4: materials".
4. Paging core: atlas, CPU table, GPU table in the ring, `Paging.hlsli`, pinned level 0, the
   paging set, and `paged_albedo` fed by a producer that fills pages from memory.
5. Feedback: the buffer, its clear pass, the write in the G-buffer, readback, requests, budget
   and eviction.
6. The page file, its two builders, the IO thread and the file producer.
7. `-paging-test`, the overlay panel and the tint. Commit steps 4 to 7 as "Phase 4: paging".
8. `RendererPlan.md`: status, "Phase 4 outcome", open items, finding 8, the page table decision
   under "Risks and open questions", and the "Import" and "Lighting" rows of "What exists today".
   `README.md` and `SETUP.md`: new switches and the page file.

## Verification

- Build and capture as in `Docs/Handoff-Phase0-Phase1.md`.
- Run after each step: Standalone and Editor `-play` on both backends, and `-stress`,
  `-compute-test`, `-animation-test` and `-crowd 100` on both. From step 4 add `-paging-test` on
  both.
- Pass condition: exit 0, and stderr empty apart from "DirectX debugging enabled." and the Noesis
  message.
- Step 1: after the fix, Vulkan and D3D12 `-compute-test` captures match to the usual tolerance.
  Before it they differ across the whole UI.
- Step 2: frozen captures match the ones taken before the step on each backend.
- The gate is the `Paging test:` line from both backends, with peak resident at or below 256.
- Record Release `-paging-test -novsync` on both backends: frame time, CPU milliseconds for the
  paging update, GPU milliseconds for the fill pass, and the longest wait.

## Decisions left to the owner

- **Colour.** Step 3 changes how every frame looks. Say so if it should wait.
- **DPI.** The window becomes physically smaller on a scaled display.
- **The character's materials.** No textures for it are in `TempAssets`, so it gets flat
  materials. Supply the textures to give it `textured_pbr` ones.
- **The test texture.** The plan generates a pattern. A real image of 8,192 texels a side or more
  can replace it through `PageFile::Build`.
- **The scratch export with two clips** (finding 8): keep it in `TempAssets`, or leave clip
  import without a test file.

## Status

Done on 16 November 2026. "Phase 3 corrections" (step 1), "Phase 4: materials" (steps 2 and 3) and
"Phase 4: paging" (steps 4 to 8) are committed on `Dev-2`. The owner chose to do the colour change
now, to declare DPI awareness, to keep the scratch export with two clips, and the generated pattern.
Beyond the plan: input now counts only while the window is in front, `-monitor N` and `-no-focus`
let test runs use a second display, and `-paging-image` exercises `PageFile::Build`. See "Phase 4
outcome" in `Docs/RendererPlan.md`.
