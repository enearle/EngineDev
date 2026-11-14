# EngineDev: assessment and plan for the procedural material renderer

Written 12 November 2026. Moved into the repository on 13 November 2026; this file is now the
maintained copy and is updated as each phase lands.

## Status

| Phase | State | Notes |
| --- | --- | --- |
| 0. Stabilize and restructure | In progress | Branch `Dev-2` |
| 1. Frame structure | Not started | |
| 2. Compute and GPU data | Not started | |
| 3. Skeleton and animation | Not started | |
| 4. Materials and paging | Not started | |
| 5. Procedural skin on the character | Not started | |
| 6. Subdivision and deep zoom | Not started | Optional |

## Verdict

EngineDev is a viable starting point: keep it. The slow, unglamorous parts already work on both backends, and most of what the cache system needs is additive.

This is from reading the main branch at commit 0240868 (12 May 2026). It does not yet cover the NewArchitectureRough branch, which holds 19 later commits up to 30 August 2026: a Dear ImGui editor, an asset and serialization system, scene nodes and a start on per-material draw bins. The project is Windows-only, so I did not build or run it.

- **What carries over.** About 11,000 lines of RHI with Vulkan 1.3 and Direct3D 12 at parity, one HLSL source compiled for both, deferred PBR, variance shadow maps, GPU skinning, resize handling, input and a UI layer.
- **What is missing.** Compute, GPU-written textures, storage buffers on Vulkan, readback, and any per-frame structure. These are exactly what the cache system leans on.
- **What must be restructured, not added.** Two things. The frame is a fixed list of pipelines that own their attachments, and draws are registered once at load. Both block batching and the cache passes.
- **True of main only.** The UI layer is NoesisGUI and draw batching has not started. Both moved forward on NewArchitectureRough, so parts of this assessment and of phases 0 and 1 will change once that branch is read.

## What exists today

The RHI is real and symmetric across both backends; everything above it is placeholder.

| Area | State | Notes |
| --- | --- | --- |
| Backends | Working | Vulkan 1.3 with dynamic rendering, and Direct3D 12, chosen at startup by a setting. Windows only |
| Shaders | Working | One HLSL source compiled by DXC to DXIL and SPIR-V, through a hand-run batch file with a fixed list |
| Pipelines | Working | Described as data. Variants share a parent's targets, and a pipeline can take earlier pipelines' outputs as inputs. Hull and domain stages are wired but untested |
| Frame | Fixed | Shadow, G-buffer, lighting, then UI, in creation order. Each pipeline owns its attachments, and barriers are hard-coded around that |
| Draws | Retained | Registered once at load into per-variant bins. No removal, culling, sorting, instancing or batching |
| Resources | Partial | Buffers and images behind integer IDs. Images must be uploaded from CPU data, with 1 mip and 1 layer. Buffers are never freed at runtime, and each resource is its own device allocation |
| Descriptors | Partial | Sets per pipeline and slot. Dynamic uniform buffers work on both backends. Storage buffers and storage images are never written on the Vulkan side |
| Lighting | Working | Deferred PBR with 4 lights and variance shadow maps rendered with multiview |
| Skinning | Partial | Vertex-shader skinning with 4 weights and a fixed 128-bone uniform buffer. No joint hierarchy and no clips; DerpMover animates three bones by hand |
| Import | Working | Assimp for FBX, stb_image for textures. A material is one of two hard-coded texture layouts, found by file name |
| UI | Working | NoesisGUI on both backends, with Noesis-specific calls inside the RHI executor |
| Engine side | Placeholder | TempGameObject, a static active camera, and an input command system. About 1,600 lines |
| Build and repo | Fragile | Visual Studio solution only. One absolute user path in the Engine project. The Noesis SDK is 5,585 of the repo's 5,829 files (528 MB). Compiled shaders are committed. No README or tests |

Defects found while reading, all small and worth fixing first:

1. `Uploader::GetBufferAllocation` returns a reference to a temporary, so every caller reads a dangling reference.
2. Each draw's push constants are a raw pointer into `TempGameObject::ModelDataArray`, which reserves 100 entries. An object with more meshes reallocates the vector and leaves earlier draws pointing at freed memory.
3. The Vulkan `DrawIndexed` pushes constants with the vertex stage hard-coded, ignoring the stages the pipeline declared.
4. `GetDescriptorSet` and `GetBufferAllocation` return by value, so the draw loop copies a vector for every set bound on every draw.
5. A dynamic buffer has one mapped copy shared by every frame in flight, so the CPU can overwrite data a queued frame still reads.
6. `SceneNode` returns its children and meshes by value, so walking a model copies each subtree.
7. `CoreInitData::SwapchainMSAASamples` is declared `bool`.
8. `InputState.cpp` exists twice, in RHI and Engine, with identical contents.

## Gaps against the cache system

Every pass in the [cache design](https://claude.ai/artifact/KGxq9yzHtA7DHYSkKvGDLZ) depends on a renderer feature that does not exist yet. Each one is general-purpose, so none needs to be written as cache-specific code.

| Need | Used by | Today | Work |
| --- | --- | --- | --- |
| Compute pipelines and dispatch | Feature pass, page bake, later skinning | Only enum bits exist | New pipeline type on both backends, plus compute passes in the frame |
| Images created empty and writable from compute | Texel page atlas | Images require CPU data; no storage path | Extend image creation; add 16-bit and integer formats |
| Storage buffers in any stage | Page table, vector lists, bone palettes | Layout type exists; Vulkan never writes it | Finish on Vulkan, verify on Direct3D 12 |
| GPU to CPU readback | Feedback buffer | None | Copy to a mapped buffer, read a few frames later |
| Ordered, dependent passes | Bake coarse levels before fine, before shading | Fixed pipeline list | Pass graph |
| Per-frame draw lists | Culling, batching, subdivision | Retained bins | Rebuild each frame from the scene |
| Per-frame constant data | UV anchors, bone palettes, bake lists | One shared mapped buffer | Ring allocator per frame in flight |
| Material as a shader interface | Material shader | Two hard-coded texture layouts | Surface interface; procedural skin is one implementation |
| Flexible vertex formats | Fixed-point UVs, a second UV set | Two hard-coded structs | Vertex layout stored per mesh |
| Sampler objects | Atlas needs clamp and point filtering | Two global samplers | Sampler descriptions |
| GPU timing | Checking the budgets | None | Timestamp queries shown in the overlay |
| Tessellation or mesh shaders | Optional subdivision | Stages wired, untested | Test the path, choose a method |
| Skeleton and clips | The character | Hand-animated bones | See skinning and animation |

One existing cost works against the VRAM goal. The G-buffer stores world position and normals in 32-bit float targets, which is 162 MB with depth at 2560 × 1440. That is more than the 88 MB planned for the whole texel cache. Rebuilding position from depth and packing normals into two channels brings it to about 59 MB.

## Roadmap

| Phase | Size | Gate |
| --- | --- | --- |
| 0. Stabilize and restructure | Small | The same image as today, validation layers clean, frame time on screen |
| 1. Frame structure | Large | 10,000 objects batched, and a new pass needs no edits to core code |
| 2. Compute and GPU data | Medium | A compute pass writes a texture, a raster pass samples it, the CPU reads it back two frames later |
| 3. Skeleton and animation | Medium | See skinning and animation |
| 4. Materials and paging | Large | A large texture streams in page by page inside a fixed atlas |
| 5. Procedural skin on the character | Large | Zoom from full body to pores at fixed VRAM and fixed fragment cost |
| 6. Subdivision and deep zoom | Medium, optional | A displaced silhouette, and stable UVs below 1/500 of a chart |

The engine layer (scene, extraction, loop, assets) grows alongside these phases; see "Toward an engine".

Phases 0 to 2 are shared groundwork. Animation and the material system can then proceed in either order, and both must be done before the procedural skin lands on the character.

## Phase detail

Each phase adds general renderer capability and ends at a gate that must hold on both backends. Sizes are relative judgments, with the two-backend bring-up you already did counting as large.

**Phase 0. Stabilize and restructure (small).**

- Split the RHI library into three layers. RHI: device, resources, pipelines, command recording. Renderer: passes, materials, mesh and texture assets. Engine: scene, animation, logic. Mesh, Material, GeometryImport and Renderer leave the RHI.
- Replace the Noesis calls in the executor with one generic external-pass hook. Noesis and Dear ImGui both attach through it.
- Add Dear ImGui as the debug overlay. Every later phase reports through it.
- Use typed, generation-checked handles in place of raw integers and void pointers, with a deferred-free queue keyed by frame. Fix the defects listed above.
- Repo: move the Noesis SDK out of the tree, remove the absolute path, make shader compilation a build step, add a README with build steps.
- Gate: the same image as today, validation layers clean, frame time on screen.

**Phase 1. Frame structure (large).** This is the unfinished batching work, done at the level where it holds.

- A minimal scene of entities with a transform, mesh and material, which the renderer reads each frame. TempGameObject goes away.
- Per-frame draw lists: cull, build a sort key from pipeline, material and mesh, and merge equal neighbours into instanced draws.
- Object data moves from push constants to a per-frame storage buffer indexed per instance. This needs storage buffers finished on Vulkan.
- A ring allocator per frame in flight for all per-frame constants.
- A pass graph. Passes declare what they read and write; the graph orders them, owns transient attachments, derives barriers and handles resize. Pipelines stop owning attachments.
- Slim the G-buffer while the attachments are being moved.
- Gate: 10,000 objects batched, and a new pass needs no edits to core code.

**Phase 2. Compute and GPU data (medium).**

- Compute pipelines and dispatch, with compute passes in the graph.
- Images created empty, with mips, array layers, storage access, sub-rectangle copies and more formats.
- Sampler objects, delayed readback, GPU timestamp queries.
- First client: mip generation for loaded textures, which have no mips today.
- Gate: a compute pass writes a texture, a raster pass samples it, and the CPU reads it back two frames later.

**Phase 3. Skeleton and animation (medium).** Covered in the next section.

**Phase 4. Materials and paging (large).**

- Materials become data: a shader, a parameter block and bindings. Each implements one surface interface that fills the G-buffer inputs. Today's textured PBR is the first implementation.
- A generic paged-texture service: page table, atlas, feedback pass with readback, least-recently-used eviction and a per-frame fill budget. Page producers plug in.
- First producer: file-backed pages. It proves the service with no procedural code involved.
- Gate: a large texture streams in page by page as the camera approaches, inside a fixed atlas.

**Phase 5. Procedural skin on the character (large).**

- Second producer: the feature pass and page bake from the cache design.
- The procedural surface material: page lookup, capped live bands, normal from slope, albedo from the pigment lookup.
- The band table as a hot-reloaded asset, with overlay views for page residency, level hit and bake cost.
- A static test mesh first, then the skinned character.
- Gate: zoom from full body to pores at fixed VRAM and fixed fragment cost.

**Phase 6. Subdivision and deep zoom (medium, optional).**

- Tessellation or mesh shaders, with displacement read from the page atlas.
- Fixed-point UV anchors and camera-relative transforms.
- Gate: a displaced silhouette, and stable UVs when the view covers less than 1/500 of a chart.

## Skinning and animation

The importer needs a real skeleton before any clip can play, and skinning should move from the vertex shader to one compute pass per frame.

**What exists.**

- An 88-byte skinned vertex with 4 weights, and inverse bind matrices imported per mesh.
- A 128-matrix uniform buffer per object, filled by DerpMover for three bones by hand.
- The joint hierarchy and clips are never read from the file. `BoneTransforms` is sized but never filled.
- Every pass re-skins: four shadow views plus the G-buffer. A comment in the skinned shader marks its normal transform as doubtful.

**Plan (phase 3).**

1. Import a skeleton from the Assimp node tree: joints with a parent index, local bind transform, inverse bind matrix and name. Import clips as translation, rotation and scale tracks per joint.
2. Evaluate poses at runtime: sample clips, blend, walk parents to model space, multiply by the inverse bind. The result is one matrix palette per character per frame.
3. Write palettes to the per-frame storage buffer, with an offset per instance. The 128-bone limit goes away.
4. Add a compute skinning pass that writes skinned position, normal and tangent to a vertex buffer per instance. Every later pass draws that buffer with the static vertex layout, so the skinned pipeline variants are deleted.
5. Apply morph targets in the same pass, before the bones, if the character's face needs them.
6. Add an animator component in the engine layer: clip playback, cross-fades and a small state machine.

Compute skinning costs one extra vertex buffer per skinned instance, about 4 MB per 100,000 vertices. In return the character is skinned once per frame instead of five times, materials never multiply into skinned variants, and subdivision can read skinned positions before the vertex stage.

**Where it meets the cache.**

- Pages are keyed by chart UV, so deformation never invalidates them.
- The mesh needs one UV set with no overlaps. Mirrored UVs are split or given a second set at import.
- Band wavelengths are world units measured at bind pose, so the importer stores a UV-to-world scale per chart.
- Stretch under animation changes the pixel footprint, which the material shader already measures. Page requests therefore follow the deformed surface.
- Pose-driven detail, such as expression wrinkles, is a band whose amplitude comes from a joint angle or morph weight. It stays live, or invalidates only its own pages.
- Displacement, if used, pushes along the skinned normal from the compute pass.

## Toward an engine

Game code talks to a scene, and the renderer reads a snapshot of it. Nothing outside the renderer submits draws.

- **Scene.** Entities with a transform hierarchy and components: mesh, skinned mesh, light, camera, animator. This replaces TempGameObject, DerpMover and the static active camera.
- **Extraction.** Once per frame the renderer copies what it needs from the scene. This boundary is what later allows a render thread.
- **Loop.** A fixed timestep for logic and a variable one for rendering. The existing input command system carries over.
- **Logic.** Native C++ systems first, behind a narrow API. A scripting language is a later decision.
- **Assets.** Cooked formats and handles, so Assimp and stb_image leave the runtime. Hot reload for shaders and band tables.
- **UI.** Noesis for game UI and Dear ImGui for tools, both through the external-pass hook.
- **Later.** Physics, audio, scene files and an editor.

## Risks and open questions

- **Two backends.** Every RHI feature in phases 1, 2 and 6 is built twice. I suggest leading with Vulkan, the current default, and reaching parity at each gate. Dropping Direct3D 12 would shorten those phases; that is your call.
- **Not built here.** The assessment is from reading. Validation-layer state and real frame cost are unknown until it runs.
- **Noesis licence.** The SDK's LICENSE.txt restricts distributing its source, and the full SDK, including Src, is in the public repo. It is worth reading before the next push.
- **Pass graph scope.** It is easy to over-build. The target is ordering, transient attachments and barriers, with no automatic pass merging or async compute.
- **Character asset.** Phase 5 needs a humanoid with clean, non-overlapping UVs and at least two clips. Which character do you intend to use?
- **Carried over from the cache design.** Page table form, chart seams and the subdivision method are still open there.
