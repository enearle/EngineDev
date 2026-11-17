# Handoff: Phase 5 (procedural skin on the character)

Written 16 November 2026 after reviewing the Phase 4 commits (`f58ed72`, `4f70b4c`, `8aeb21a`) on
`Dev-2`. Read these first, in this order:

1. `CodingStandard.md` (root). Follow it. Comments only for external API usage and complex spots.
2. `Docs/RendererPlan.md`. Keep its Status table, outcomes and open items current as you go.
3. The cache design linked from "Gaps against the cache system" in that file. Phase 5 builds it.
   Where this handoff differs from the design, the handoff says so and gives the reason.
4. This file.

Branch is `Dev-2`. Commit there. Do not push. One commit per group in "Order of work". The ground
rules in `Docs/Handoff-Phase0-Phase1.md` still apply: the layers stay separate, both backends hold
at each gate, and verification is by build output and captured stdout/stderr, not screenshots.
Every test window opens on the second display without taking focus (see Verification).

## Review of the work so far

Checked on 16 November 2026: the Debug solution builds, and fourteen runs pass on the second
display. Standalone and Editor `-play`, `-stress -novsync`, `-compute-test`, `-animation-test`,
`-crowd 100` and `-paging-test`, each on both backends. Every run exits 0. Vulkan stderr is empty.
D3D12 stderr holds only "DirectX debugging enabled." and the Noesis message.

- Phase 2 gate: 679 readbacks matched on Vulkan, 507 on D3D12.
- Phase 3 gate: bind pose identity for 144 joints, 2 clips from the file, 256 vertices matched.
- Phase 4 gate: 441 pages filled, 185 evicted, peak 256 resident, 300 verified, final view fully
  resident on Vulkan; 442, 186, 256 and 300 on D3D12.
- Debug `-stress -novsync` read 32 ms on Vulkan and 55 ms on D3D12, the Phase 3 figures. The 41 to
  79 ms recorded in "Phase 4 outcome" did not come back.

The Phase 4 code matches its outcome section. The surface interface, the single material set, the
hash page table in the frame ring and the feedback path all read correctly.

### What Phase 5 needs changed in earlier work

1. **A missing chain of pages fills one level per feedback round trip.** The service queues only
   the coarsest missing ancestor of each wanted page (`Renderer/Paging.cpp:344`, `:382`). A zoom
   that skips ten levels therefore takes ten round trips of about three frames each. The cache
   design bakes a whole chain, coarse first, in one frame.
2. **A producer cannot bake from its parent.** `Fill` runs with the atlas in `TransferDst`
   (`Paging.cpp:193`), and a compute bake that reads the parent's slot while writing the child's
   would read and write one image at once, which D3D12 forbids. There is also no image-to-image
   copy in the RHI.
3. **The shader samples one atlas image.** `Shaders/Paging.hlsli` declares a single `PagingAtlas`
   and returns one colour, although the paging set binds an image per atlas format.
4. **The camera cannot get close.** The near plane is 0.1 with standard depth and `LessEqual`
   (`Engine/Camera.h:25`, `Renderer/Passes/DeferredPasses.cpp:57`, `Shaders/ps_lighting.hlsl:153`).
   The character is scaled to the Derp's size, so a view 2 mm wide puts the camera a few
   thousandths of a unit from the skin.
5. **A vertex has one UV set** (`RHI/RHI/RHIStructures.h`, `Vertex`). Seam patches need a second
   set, a blend weight and a chart index.
6. **Paging limits are sized for one test texture.** 16 textures, constants for them in a 512-byte
   block, 16 fills a frame, 64 queued pages, 256 slots.
7. **Two things the atlas formats need are missing.** There is no `R16G16B16A16_UNORM`, and Vulkan
   does not enable `shaderStorageImageExtendedFormats`, which 16-bit `UNORM` storage images need.
8. **The G-buffer has no room marked for a shading model.** The alpha of the material target is
   written as 1 and never read (`Shaders/ps_gbuffer.hlsl`). Subsurface strength can live there.
9. **A material's parameter block is 256 bytes.** A band table does not fit in it.

### The character, measured

From the mesh assets in `TempAssets`, read with a script:

- The body is seven submeshes by material: Head, Body, Arms, Legs, Fingernails, Toenails and
  Mouth Cavity. Each has its own 0 to 1 UV square; no triangle crosses a square's edge. The four
  skin charts are Head, Body, Arms and Legs, 743,296 triangles in all.
- Mirrored triangles: 46, in Arms and Legs. Left and right limbs have their own UV space.
- Overlap inside a chart is negligible on Body, Arms and Legs and about 2% of occupied cells on
  Head.
- Units are centimetres. One UV unit is about 99 cm on Legs, 85 on Body, 76 on Arms and 46 on
  Head (medians). Within a chart the 95th to 5th percentile ratio is 1.4 to 1.9, and median
  anisotropy is 1.15 to 1.48.
- Every mesh has one UV layer and no vertex colours.

So the character satisfies the plan's requirement of clean, non-overlapping UVs, a single scale per
chart is right to within about 25%, and a pore-sized texel (about 8 micrometres) is level 10 on
Legs. 32-bit float UVs hold to about level 14, which is where Phase 6 takes over.

## Decisions taken with the owner

- **Seams.** Blending patches in a second UV set, so bands are always evaluated in a flat space
  and the skin looks the same from the same angle. The importer generates patches for a mesh with
  no second UV map; an authored second UV map wins.
- **Direction field.** Derived from the skeleton at import.
- **Bands.** All three primitives: noise, points and curves.
- **Shading.** Roughness from slope too fine to see, and a simple subsurface term, both in scope.

## Phase 5 design

Gate from the plan: zoom from full body to pores at fixed VRAM and fixed fragment cost. Made
concrete, `-skin-test` runs a scripted zoom on the character and prints these on stdout:

1. **Level invariance.** At each stop the same view is shaded twice, once from the wanted page
   level and once forced two levels coarser. Albedo and normal read back from the G-buffer agree
   within a stated tolerance. This is the design's central claim: baked plus live equals the full
   material whichever level is resident.
2. **Fixed memory.** The bytes of the atlas, scratch images, vector cache and chart maps, printed
   at the first and last stop, are equal.
3. **Fixed fragment cost.** G-buffer GPU milliseconds at every stop where skin fills the view
   differ by less than 25%, in Release with `-novsync`.
4. **Convergence.** The longest wait in frames from arriving at a stop to a fully resident view.
5. **Seams.** At sampled points on original UV seams, the step in albedo and normal across the
   seam is no larger than between neighbouring pixels elsewhere.
6. **Stability under animation.** With the camera still and a clip playing, fills per frame stay
   at zero once the view has converged, apart from pages whose footprint changed.

The last stop is a view 2 mm wide. On both backends with validation clean.

Change these decisions if you find a reason, and say why.

### Corrections (step 1)

- **Reversed depth** (finding 4). The camera projection maps near to 1 and infinity to 0. The
  G-buffer depth clears to 0 and tests `GreaterEqual`; the lighting shader treats 0 as sky. The
  near plane becomes settable, default 0.01, and `-skin-test` lowers it for the close stops.
  Shadow passes keep their own depth convention.
- **Whole chains** (finding 1). `QueueRequestedPages` queues every missing ancestor of a wanted
  page. `ScheduleFills` already places a parent before its child; the fill pass now runs fills in
  groups by level, with the transitions a child needs to read what its parent just wrote.
- **Producers that bake** (finding 2). A producer declares whether it copies or computes. For
  computing producers the service owns scratch images with the atlas formats (64 slots), keeps the
  atlas readable while the producer writes a scratch slot, then copies scratch to atlas. Add
  `CopyImage(source, sourceRegion, destination, destinationRegion)` to the executor. The service
  does every transition; delete the open item that makes producers do it.
- **Lookup and sampling** (finding 3). `Paging.hlsli` splits into `ResolvePage`, which returns the
  slot, the level hit and the atlas UV, and one sample call per atlas image.
  `SamplePagedTexture` stays as the pair for one image.
- **Limits** (finding 6). 64 textures. Per-texture data (level count, size in millimetres per
  axis, periodic flag, band table, chart map layer) moves out of the constant block into 16-byte
  entries in the frame ring, read through the page table binding. 64 fills a frame and a matching
  queue for producers that are ready at once; the file producer keeps 16. `-atlas-slots N`.
- **Formats** (finding 7). Add `R16G16B16A16_UNORM`. Enable `shaderStorageImageExtendedFormats`.
  If a backend cannot write it from compute, fall back to `R16G16B16A16_FLOAT` and say so.
- **Vertex** (finding 5). `Vertex` and `SkinnedVertex` gain `TexCoord1` (two floats),
  `PatchWeight` and `PatchChart` (one float each): 72 and 104 bytes. Update the word counts in
  `cs_skinning.hlsl`, the `static_assert`s in `Skinning.cpp`, the vertex attributes and
  `Skinning::MaxPoolVertices`. The pool then holds 17 copies of the character, not 21.
- **Subsurface slot** (finding 8). `Surface` gains `Subsurface`, written to the material target's
  alpha. Every existing type writes 0.

Nothing visible changes in this step. Frozen captures match the ones taken before it.

### Atlas contents

Two images share the slot grid, 12 bytes a texel, sampled with two filtered fetches:

- `R16G16B16A16_UNORM`: height, slope along U, slope along V, spare.
- `R8G8B8A8_UNORM`: three pigment fields and a spare. File-backed pages keep using this image for
  colour.

Fields are deviations about zero, since bands only add; base tone comes from material parameters.

This differs from the design in one way. Height uses one fixed range per material, not a range per
page. 16 bits over 4 mm resolve 60 nanometres, shading takes its normal from the stored slope and
not from height differences, and a per-page range would need the GPU bake to report a minimum and
maximum to the CPU table. Phase 6 needs the per-page range for displacement and should add it then.

At 256 slots the atlas is 57 MB and the scratch images 14 MB.

### Units, charts and maps (step 2)

- **Units.** Band wavelengths are in millimetres on the bind pose. The importer reads the file's
  unit (centimetres for this FBX) and never uses world units, because the character is rescaled
  in the scene.
- **Chart.** One per skin submesh, and later one per seam patch. A chart is a paged texture plus
  its size in millimetres along U and V, taken from the median of the per-triangle scale. Pages
  are keyed by chart UV, so every copy of a character shares them and deformation never
  invalidates them.
- **Chart map.** One 512 x 512 `R8G8B8A8_UNORM` layer per chart in a single array image: the
  direction field (two channels, in the chart's UV frame), local scale relative to the chart
  median (one channel), and a coverage mask. Rasterised on the CPU at import and dilated a few
  texels past island borders. Local scale corrects feature widths; lattice density stays one value
  per chart, so features vary in spacing by up to the 25% measured above. Anisotropy is not
  handled. Record both.
- **Direction field.** For each vertex, take its most influential joint and the bind-pose axis
  from that joint to its child (or from its parent, for a joint with no single child). The
  direction is the surface tangent perpendicular to that axis, so lines circle limbs and torso.
- Import stores chart sizes and map layers in the mesh asset. The renderer side is
  `Renderer/Skin`; the engine hands it descriptions and never the other way round.

### Band table

- `Engine/Materials/<name>.bands`, one band per line: name, primitive, wavelength in millimetres,
  amplitude, target field, seed, and for points and curves the density, profile, width, the band
  it depends on, and stretch along the direction field. Sorted coarse to fine, at most 32 bands.
- The file's time is checked once a second. On a change the table is reparsed, every page and
  vector cell of the charts that use it is dropped, and level 0 is requeued.
- The GPU copy is written into the frame ring each frame, with each band's slope variance and the
  running sum of variance over all finer bands.
- **Weights**, as the design defines them. A band's bake weight at a level is 0 when its
  wavelength spans 2 page texels or fewer, 1 at 4 or more, smooth between. Its pixel weight is the
  same curve on the pixel footprint. A live band is added with pixel weight minus bake weight at
  the level hit.
- **Lattice.** A band has a whole number of cells across a chart: the chart's size divided by the
  wavelength, rounded. One HLSL function turns a page key and a position inside the page into a
  cell index and a fraction, in 32-bit integer maths with a split multiply. The bake and the
  material shader both call it, which is what makes baked and live parts identical. No 64-bit
  shader integers. On a periodic chart the cell index wraps along U.

### Noise bands, bake and live (step 3)

- **Noise.** Gradient noise on the band lattice with an analytic derivative. Tiling exemplars are
  left out; record it.
- **Bake** (`SkinBakeProducer`, a computing producer; `cs_skin_bake`). For each texel of the
  136 x 136 slot, gutter included: sample the parent's quadrant with a 16-tap Catmull-Rom filter
  (the 4-texel gutter is exactly the support needed), or start from zero at level 0; add each band
  whose bake weight changed between the parent's level and this one, scaled by the change; write
  height, analytic slope and pigments.
- **Refinement error**, the design's open question, is what the level-invariance check measures.
  If it fails on the band nearest the parent's limit, re-add that band in full.
- **Material type `procedural_skin`.** Resolve the page, sample both images, add at most three
  live bands, coarsest first. The normal comes from the summed slope in the tangent frame, scaled
  by the chart's size. Albedo comes from the pigment fields through an analytic absorption curve
  (base melanin, base hemoglobin and a tint in the parameter block); a lookup texture can replace
  it later. Roughness is a base value plus the slope variance of every band too fine for the
  pixel, read from the running sum. It writes feedback like `paged_albedo`.
- **Test.** `-skin-test plane` puts the material on the paging-test plane as a chart 1 m wide and
  prints the invariance, memory, cost and convergence lines.

### Point features and the vector cache (step 4)

- **Vector cache** (`Renderer/Skin/FeatureCache`). A device buffer of 16,384 cells, each up to 32
  primitives of 8 bytes in cell-local 16-bit coordinates: 4 MB. A band stores its cells at the
  grid where they are about half full, so a cell is keyed by chart, band, x and y and stays valid
  at every deeper zoom. The CPU owns a hash from key to cell, writes it into the frame ring each
  frame like the page table, and evicts the least recently used.
- **Requests** need no second feedback path. The CPU derives the cells from pages: those under a
  page about to be baked, for the bands that bake adds, and those under each wanted page, for the
  bands that can be live at that footprint. A live band whose cell is missing adds nothing that
  frame.
- **Feature pass** (`cs_skin_features`), before the bake in the same frame. For each missing cell:
  seed a hash from the integer key; place points on a jittered grid; where a band depends on a
  coarser one, read that band's cells and place against them; clip to the cell plus a margin, so a
  texel needs one cell; quantise and write. Cells of a coarser band are generated first, using the
  same level grouping as page fills.
- **Pores at furrow junctions.** A junction is the circumcentre of three furrow points whose
  circle holds no other point.
- **Bake and live.** Loop over the cell's primitives and turn distance into a profile: a dip for
  pores and freckles, the gap between the nearest and second-nearest point for furrow and plate
  networks. Widen every profile to at least one texel or pixel. Distances use the direction field
  and the band's stretch.
- The skin table then has tone mottling, freckles, the furrow network, pores, secondary lines,
  plates and grain. `-skin-test plane` adds the cells generated and the cache's peak to its output.

### Curves (step 5)

- Curve seeds sit on the band lattice. The feature pass traces each seed both ways along the chart
  map's direction in fixed steps, stops at a coarser curve or a maximum length of a few cells, and
  writes quadratic segments (two primitive slots each) into every cell the curve touches. A cell
  job retraces every seed within reach, so neighbouring cells agree.
- Bake and live measure distance to a segment as two or three straight pieces.
- If tracing proves awkward in compute, generate cells on a CPU thread and upload them with
  `CopyBuffer`, and say why. Points stay on the GPU.
- Wrinkles and creases complete the design's eight bands.

### The character (step 6)

- Head, Body, Arms and Legs get `procedural_skin` materials that share `skin.bands`, each bound to
  its chart at load. Nails, mouth, eyes, teeth, lashes and brows stay flat.
- `-skin-test` defaults to the character: full body, torso, a forearm at 20 cm, 2 cm and 2 mm,
  with a clip playing at the middle stop for the stability line.

### Seam patches (steps 7 and 8)

A fragment near a seam evaluates two charts and blends them. `TexCoord1` and `PatchChart` name the
patch chart and `PatchWeight` is 1 on the original seam and 0 at the patch edge. Because fields
are deviations about zero, the blend keeps their contrast:
`(lerp(primary, patch, w)) / sqrt((1 - w)^2 + w^2)`. Slopes are turned into the surface's tangent
plane before blending; the patch's frame comes from screen derivatives of position and
`TexCoord1`. Where the weight is 0 the patch is not evaluated. A seam fragment costs up to twice a
plain one, and feedback alternates between its two charts by frame.

- **Weights** are computed at import for both kinds of patch: distance to the nearest original
  seam and distance to the patch edge, by a shortest-path sweep over the welded mesh.
- **Generated patches** (step 7), for a mesh with no second UV map:
  1. Weld vertices by position. A seam edge is one whose two sides differ in UV or in skin
     material. Edges with one face are openings, not seams.
  2. Chain seam edges into curves, splitting where three or more meet.
  3. Each curve gets a strip: the faces within 30 mm, parameterised by distance along the curve
     and signed distance from it. Faces are assigned to the nearest curve, and vertices on a
     strip's border are duplicated so a triangle never mixes two patch charts.
  4. Each strip is its own chart. A closed curve, such as the neck, makes a chart that is periodic
     along U, so it has no seam of its own.
- **Known limit of generated patches.** Where a curve ends inside a chart, or where curves meet,
  no two-layer blend is continuous. Fade the patch weight to 0 over a quarter of the strip width
  around those points and record the residual break.
- **Authored patches** (step 8). A mesh with a second UV map uses it as one patch chart and
  generates nothing. An authored patch around a closed seam has to be cut somewhere; tell the
  owner to hide the cut or leave that mesh to the generator. This step waits on a re-export from
  the owner; without one, finish the rest and record it as open.

### Shading (step 9)

- **Subsurface.** The lighting shader reads strength from the material target's alpha. For a
  non-zero strength the diffuse term wraps per colour channel, red furthest, normalised to
  conserve energy. Specular is unchanged. The scatter colour is one constant for now.
- This is a single-normal approximation: fine pores still shade the diffuse term at full
  sharpness. A screen-space diffusion pass and a floating-point target are the proper fix; record
  them.

### Overlay (step 10)

A "Skin" panel: charts, pages baked and cells generated this frame, vector cache occupancy, GPU
milliseconds of the feature and bake passes. `-skin-view` selects a debug output: level hit, live
band count, patch weight, or residency.

## Order of work

Build and run the cases under Verification after each step.

1. Corrections. Commit as "Phase 4 corrections".
2. Units, charts and chart maps at import.
3. Band table, noise, bake, live bands, `procedural_skin`, `-skin-test plane`. Commit steps 2 and
   3 as "Phase 5: baked and live bands".
4. Vector cache, feature pass, point bands.
5. Curves. Commit steps 4 and 5 as "Phase 5: vector features".
6. The character. Commit as "Phase 5: the character".
7. Patch blending, weights and generated patches.
8. Authored patches. Commit steps 7 and 8 as "Phase 5: seam patches".
9. Subsurface.
10. Overlay. Commit steps 9 and 10 as "Phase 5: skin shading".
11. `RendererPlan.md`: status, "Phase 5 outcome", open items, the answers to the cache design's
    open questions, and the rows of "What exists today" that changed. `README.md` and `SETUP.md`:
    new switches, the bands file and how to author a patch UV map.

## Verification

- Build as in `Docs/Handoff-Phase0-Phase1.md`.
- Every run passes `-monitor 1 -no-focus`, and `Tools/RunAndCapture.ps1` is called without
  `-Foreground`. The owner works on the primary display. Captures compare only with captures
  taken the same way.
- Run after each step: Standalone and Editor `-play` on both backends, and `-stress -novsync`,
  `-compute-test`, `-animation-test`, `-crowd 100` and `-paging-test` on both. From step 3 add
  `-skin-test plane` on both, and from step 6 `-skin-test`.
- Pass condition: exit 0, and stderr empty apart from "DirectX debugging enabled." and the Noesis
  message.
- Steps 1 and 2 must not change the image.
- The gate is the six `-skin-test` lines from both backends on the character.
- Record Release `-skin-test -novsync` on both backends: frame time and G-buffer, bake and feature
  GPU milliseconds at each stop, pages baked and cells generated, and the longest wait.

## Not in Phase 5

- Displacement, fixed-point UVs, camera-relative transforms and zoom below a 2 mm view: Phase 6.
- Bands driven by pose, such as expression wrinkles. No asset has morph targets.
- Tiling exemplars, a per-page height range and anisotropic chart stretch.
- Textures for eyes, mouth, nails and hair.

## Status

Done on 17 November 2026 apart from two gate lines. "Phase 4 corrections", "Phase 5: baked and live
bands", "Phase 5: vector features", "Phase 5: the character", "Phase 5: seam patches" and "Phase 5:
skin shading" are committed on `Dev-2`. Level invariance fails at the 2 mm view on both backends,
and fixed fragment cost fails on Vulkan in Release. Authored patches (step 8) wait on an export
with a second UV map. The decisions changed from this plan, with reasons, are under "Phase 5
outcome" in `Docs/RendererPlan.md`.
