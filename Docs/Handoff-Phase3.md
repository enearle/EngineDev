# Handoff: Phase 3 (skeleton and animation)

Written 14 November 2026 after reviewing the "Phase 1 corrections" and "Phase 2" commits
(`f90aef3`, `f7542a4`) on `Dev-2`. Read these first, in this order:

1. `CodingStandard.md` (root). Follow it. Comments only for external API usage and complex spots.
2. `Docs/RendererPlan.md`. Keep its Status table, outcomes and open items current as you go.
3. This file.

Branch is `Dev-2`. Commit there. Do not push. Two commits: "Phase 2 corrections" (step 1 below) and
"Phase 3" (everything else). The ground rules in `Docs/Handoff-Phase0-Phase1.md` still apply: the
layers stay separate, both backends hold at each gate, and verification is by build output and
captured stdout/stderr, not screenshots.

## Review of the work so far

Checked on 14 November 2026: the Debug solution builds, and eight runs pass. Standalone and Editor
`-play` on both backends, `-stress -novsync` on both, `-compute-test` on both. Every run exits 0.
Vulkan stderr is empty. D3D12 stderr holds only "DirectX debugging enabled." and the Noesis message.

- Phase 1 gate: 10,014 objects in 14 batches per pass, 29 draw calls.
- Phase 2 gate: "825 readbacks matched, latency 1 to 3 frames" on Vulkan, "668 readbacks matched,
  latency 1 to 3 frames" on D3D12. Nine mip chains generated on both.
- All seven corrections from `Docs/Handoff-Phase2.md` are in the code as `RendererPlan.md` describes,
  and the shadow pass no longer uses multiview.

The Phase 2 code matches its outcome section. The findings below are from reading and from the
runs. None is a failure of a gate.

### Findings in the renderer

1. **Three RHI features from Phase 2 have no client.** `DescriptorType::ReadWriteStorageBuffer`,
   `TransitionBuffers` and `CopyBufferToImage` are not called anywhere outside the RHI. The gate
   test used images only. Compute skinning is their first use, so treat them as unverified on both
   backends.
2. **Buffer states stop short of drawing.** `BufferState` has shader read, shader write and the
   two copy states (`RHI/RHI/RHIStructures.h`). A buffer written by compute and then bound as a
   vertex buffer needs a vertex state: `VERTEX_AND_CONSTANT_BUFFER` on D3D12,
   `VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT` on Vulkan.
3. **`DrawIndexed` has no base vertex** (`RHI/RHI/PipelineExecutor.h:77`). Drawing one character's
   range of a shared skinned vertex buffer needs it.
4. **There is no buffer-to-buffer copy and `Readback` takes images only.** Checking skinned
   vertices against the CPU needs both. Phase 4's feedback buffer will too.
5. **Debug frame times on D3D12 include GPU-based validation** (`DirectX12/D3DCore.cpp:171`).
   Debug `-stress -novsync` reads 56 ms on D3D12 and 33 ms on Vulkan, with the same 30 ms of GPU
   passes on both. The Release numbers in `RendererPlan.md` are the comparable ones. Compare
   backends in Release only.

### Findings in the skinning and import code

This code predates Phase 0 and was carried through. Phase 3 replaces most of it, so fix these as
part of the replacement, not separately.

6. **No file in the repository contains an animation clip.** `Derp.fbx` is 28 KB with three
   skinned bones and no animation stack. `Test.fbx` and `shells.fbx` have no bones. The clip
   importer cannot be tested until a file with clips exists. See "Decisions left to the owner".
7. **Bone matrices use the opposite convention from every other matrix.** Node transforms are
   transposed on import into DirectXMath row-vector form (`Engine/Geometry/GeometryImport.cpp:20`).
   Bone offsets are not (`:149`). `DerpMover` then multiplies the two kinds together
   (`Engine/DerpMover.cpp:78`). The Derp looks right at rest only because the product is the
   identity there. This is also why its hand animation moves so far.
8. **`BoneTransforms` is never filled and is written to disk.** It is resized and left
   uninitialised (`GeometryImport.cpp:137`), then serialised (`Resources/Assets/MeshAsset.cpp:40`).
9. **Bone indices are per mesh.** Each `aiMesh` numbers its own bones. `DerpMover` merges offsets
   by index (`DerpMover.cpp:31`), and one palette serves every submesh of a component
   (`Scene/SceneExtraction.cpp:17`). That holds for one skinned mesh and no more.
10. **Skinned meshes are culled by their bind-pose sphere** (`Renderer/MeshRegistry.cpp:27`). A
    character that moves away from its bind pose will be culled while visible.
11. **Skinned normals use the upper 3 x 3 of the skin matrix**
    (`Renderer/Shaders/vs_pbr_skinned.hlsl:32`). That is exact for rotation and uniform scale and
    wrong for non-uniform scale. Phase 3 keeps the rule and says so.

### Carried, not part of Phase 3

- Every launch re-imports the FBX files with Assimp and rewrites 14 `.asset` files (about 14 MB)
  and `registry.bin` once per asset (`Resources/ResourceManager.cpp:103`). Both are gitignored.
  Cooked assets are the fix and belong with the asset work.
- The open and carried items already listed under "Phase 2 outcome".

## Phase 3 design

The plan's gate reads "see skinning and animation". Made concrete:

1. A character imported from a file, with a skeleton and at least two clips, plays one clip and
   cross-fades to the other.
2. Skinning runs once per character per frame in one compute pass. The shadow and G-buffer passes
   draw the result with the static pipelines. The skinned pipelines and vertex shaders are deleted.
3. `-animation-test` prints two self-checks on stdout: the bind-pose palette is the identity for
   every joint, and GPU-skinned vertices read back match CPU skinning of the same vertices.
4. On both backends with validation clean.

Change these decisions if you find a reason, and say why.

### One matrix convention

Row vectors on the CPU, as DirectXMath and `SceneNode` already use: `World = Local * ParentWorld`.
For joints: `Model[j] = Local[j] * Model[parent]`, `Skin[j] = InverseBind[j] * Model[j]`, and a
skinned vertex is `v * Skin`. Every Assimp matrix is transposed on import, bone offsets included.
HLSL reads these matrices as columns, so shaders keep writing `mul(M, v)`.

### RHI

- `BufferState::VertexBuffer` and `BufferState::IndexBuffer`.
- `DrawIndexed` takes a base vertex, default 0.
- `CopyBuffer(source, sourceOffset, destination, destinationOffset, size)` on the executor.
- A buffer bound as a storage buffer on Vulkan needs storage usage, so skinned source vertex
  buffers and the skinned vertex pool are created with `Usage.Storage`.

### Engine: `Engine/Animation`

- **`Skeleton`.** Joints in parent-first order, each with a name, parent index, bind-pose local
  translation, rotation (quaternion) and scale, and an inverse bind matrix. The joint set is every
  node that is a bone or an ancestor of a bone below the model root, so an armature node and its
  scale are inside the skeleton. No joint limit.
- **`Clip`.** Name, duration in seconds, and per joint a translation, rotation and scale track of
  timed keys. Sampling interpolates linearly, rotations by normalised lerp on the shorter arc. A
  joint with no track keeps its bind pose. Ticks convert with the file's rate, or 25 when it is 0.
- **`Pose`.** One translation, rotation and scale per joint. `Blend(a, b, weight)` and
  `BuildPalette(skeleton, pose, out)`.
- **`AnimatorComponent`** on the model's root `SceneNode`. It holds the skeleton, named states
  (clip, looping, speed, optional next state), the current and previous state with a fade timer,
  `Play(state, fadeSeconds)`, and the palette it produced. That is the whole state machine: named
  states, fades requested by code, and an automatic move to the next state when a clip ends. No
  blend trees.
- **`AnimationSystem::Update(dt)`** evaluates every enlisted animator. `GameRunFrame` calls it
  before `SceneExtraction::ExtractFrame`. `-freeze-animation` holds every animator at time 0.
- `MeshComponent::BonePalette` goes away. A skinned mesh component finds the animator on an
  ancestor once at load, and extraction takes the palette from there.
- `DerpMover` is deleted. Its motion becomes a clip built in code on the Derp's joints, so the
  runtime can be tested before a file with clips exists.

### Engine: import

- Set `AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS` to false, or Assimp splits each FBX joint into
  helper nodes and clip channels no longer name the joints.
- Build the skeleton from the node tree and the union of every mesh's bones, matched by name.
- Bake each skinned mesh's vertices into model space, and rewrite its bone indices as skeleton
  joint indices. The inverse bind then belongs to the joint, not to the mesh, and one palette
  serves the whole character. Warn if two meshes disagree about a joint's inverse bind.
- A skinned mesh is drawn with the world matrix of its animator's node, because its vertices are
  in model space.
- Joint nodes do not become `SceneNode`s. A non-skinned mesh parented under a joint is not
  supported yet: warn and attach it to the model root.
- Store, per joint, the largest distance from the joint to any vertex it influences. The
  character's bounding sphere each frame encloses the joint positions grown by those radii
  (finding 10).
- Clips: one `Clip` per `aiAnimation`, channels matched to joints by name, unmatched channels
  warned about.
- Assets: `ResourceType::Skeleton` and `ResourceType::AnimationClip` with `Serialize` and
  `Deserialize`, written beside the mesh assets. `MeshAsset` drops both bone arrays and gains a
  skeleton field (findings 8 and 9).

### Renderer

- **Buffers in the pass graph**, shaped by this one client. `ImportBuffer(handle)` returns a
  `GraphBufferID`; `SetBuffer(id, handle)` swaps the buffer when it is reallocated. `PassDesc`
  gains `BufferWrites` and `BufferReads`, the latter with the state wanted (shader read or vertex
  buffer). Writers run before readers, as for images, and the graph emits the transitions.
  D3D12 buffers decay to `COMMON` after each `ExecuteCommandLists`, so the graph treats every
  buffer as `Undefined` at the start of a frame. Check that against the debug layer in the first
  skinning run; if it objects, keep the state across frames instead.
- **`Renderer/Skinning`.** `Enlist(sourceMesh)` returns a `SkinnedMeshHandle` and claims a vertex
  range in one pool buffer of static `Vertex` layout (vertex and storage usage, device memory).
  `Retire` returns the range. The pool is rewritten every frame, so growing it means creating a
  larger buffer and destroying the old one, with no copy. One pool is enough for all frames in
  flight because the GPU runs frames in submission order.
- **Skinning pass.** One compute pass with a low `Order`, after mip generation, with the pool as a
  buffer write. For each skinned object: bind the source vertices, the palette in the frame ring
  and the pool, push the vertex count, destination base vertex and palette offset, and dispatch.
  Positions use the full skin matrix; normal, tangent and bitangent use its 3 x 3 and are
  normalised (finding 11). The shadow and G-buffer passes declare the pool as a vertex-buffer read.
- **Vertex data in the compute shader is bound as 32-bit words**, a storage buffer with a stride
  of 4, not as a struct. DXC can lay out a struct of `float3` fields differently for SPIR-V than
  the tightly packed C++ `Vertex` and `SkinnedVertex`, and words remove the question on both
  backends. Add `static_assert`s for the size and field offsets the shader assumes (56 and 88
  bytes today).
- **Drawing.** `RenderObject` gains a `SkinnedMeshHandle` and a world-space bounding sphere that
  overrides the mesh's. A skinned object draws the pool with its base vertex, its own index buffer
  and the static pipeline, as a batch of one.
- **Deleted:** `vs_pbr_skinned.hlsl`, `vs_vsm_skinned.hlsl`, `SkinMatrix` and the `Bones` binding
  in `Common.hlsli` and the scene set, `InstanceData::BoneOffset`, the per-layout pipeline arrays
  in `DeferredPasses`, and the vertex layout bits of the batch key. `MeshRecord::Layout` stays, as
  the layout of the source vertices.
- **Buffer readback.** `Readback::RequestBuffer(buffer, offset, size, callback)`, recorded in the
  existing readback pass with `CopyBuffer`, delivered the same way as image readbacks.
- **Overlay.** An "Animation" panel: characters, joints, skinned vertices, CPU milliseconds for
  animation, GPU milliseconds for the skinning pass.

### Tests

- **`-animation-test`.** At load, build the palette from the bind pose and check every matrix
  against the identity. Each frame, read back the first 256 skinned vertices of one character and
  compare them with CPU skinning of the same source vertices and the palette of the frame that was
  requested. At shutdown print `Animation test: bind pose identity for J joints` and
  `Skinning test: N vertices matched on F frames, largest error E`, or the first failure.
- **`-crowd N`.** N copies of the character on a grid, default 100, each starting its clip at a
  different time and cross-fading on a timer.

### Not in Phase 3

- Morph targets. No asset has them. The skinning shader applies them before the bones when one
  does; record it as an open item.
- Skipping characters that no view can see, a fixed timestep for logic, and attaching objects to
  joints.
- The UV and chart work listed under "Where it meets the cache", which belongs to Phase 5.

## Order of work

Build and run the cases under Verification after each step.

1. RHI additions: vertex and index buffer states, base vertex, `CopyBuffer`, and buffer readback.
   Nothing visible changes. Commit as "Phase 2 corrections".
2. Skeleton: data model, import with model-space vertices and joint indices, the convention
   change, the bind-pose check. `DerpMover` is deleted and the Derp renders at rest through the
   existing vertex-shader path with a palette from the skeleton.
3. Clips, poses, the animator and the animation system, driven by the clip built in code. The
   Derp animates again.
4. Buffers in the pass graph, the skinning pool and pass, static-pipeline drawing, the skinning
   check, then the deletions.
5. Bounds for culling, and the overlay panel.
6. Clip import from a file, cross-fades between imported clips, and `-crowd`. This step needs the
   character from "Decisions left to the owner".
7. `RendererPlan.md` (status, "Phase 3 outcome", open items, and the "What exists today" skinning
   row), `README.md` and `SETUP.md` (new switches).

Steps 1 to 5 do not depend on the character. If it has not arrived, finish them, record step 6 as
open, and leave Phase 3 marked in progress.

## Verification

- Build and capture as in `Docs/Handoff-Phase0-Phase1.md`.
- Run after each step: Standalone Vulkan, Standalone `-dx12`, Editor `-play`,
  Editor `-play -vulkan`, Standalone `-stress` on both, and `-compute-test` on both. From step 2
  add `-animation-test` on both, and from step 6 `-crowd 100` on both.
- Pass condition: exit 0, and stderr empty apart from "DirectX debugging enabled." and the Noesis
  message.
- Steps 2 and 4 must not change the Derp at rest. Compare Vulkan against D3D12 with
  `-freeze-animation`, in the same run state, against the 0.23 out of 255 recorded for Phase 2.
- The gate is the two `-animation-test` lines from both backends, and the cross-fade running in
  `-crowd` on both.
- Record Release `-crowd 100 -novsync` on both backends: frame time, CPU milliseconds for
  animation, GPU milliseconds for skinning.

## Decisions left to the owner

- **The test character.** Step 6 needs a file with a skeleton and at least two clips. Either
  supply the character meant for Phase 5, or name a sample to add to the repository. Check the
  licence of any sample before it is committed, since the repository is public.
- **Skeleton and clip assets.** The design writes them as `.asset` files in the existing pattern.
  Say so if they should stay in memory until the cooked-asset work.
- **Joints are not scene nodes.** Attaching an object to a joint then needs a later feature.

## Status

In progress on 14 November 2026. "Phase 2 corrections" (step 1) and "Phase 3" (steps 2 to 5 and 7)
are committed on `Dev-2`. Step 6 waits for a character with at least two clips; until then the Derp
runs two clips built in code, and `-crowd` uses it. Skeleton and clip assets are written as `.asset`
files, and joints are not scene nodes, as proposed above. See "Phase 3 outcome" in
`Docs/RendererPlan.md`.
