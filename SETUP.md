# Setup — EngineDev on a fresh machine

Everything needed to go from a bare Windows box to a building/running solution.
Last verified: 2026-11-12 (branch `NewArchitectureRough`) by building Debug|x64 with MSVC 14.44, the
Vulkan SDK 1.4.363 and NoesisGUI 4.0.1, then running the Editor (DX12) and Standalone (Vulkan).

---

## 0. TL;DR checklist

- [ ] Visual Studio 2022 (17.9+) with **Desktop development with C++**, latest MSVC v143 toolset, Windows 11 SDK
- [ ] Vulkan SDK 1.4.313.0 (LunarG) — sets `VULKAN_SDK`, and supplies `dxc`
- [ ] `git config --global core.longpaths true`, clone to a **short path** (e.g. `C:\Dev\EngineDev`)
- [ ] `git clone https://github.com/enearle/EngineDev.git`
- [ ] Build `NoesisApp.dll`/`.lib` from the NoesisGUI 4.0.1 SDK sources, once (see §3a)
- [ ] Build `Solution/Engine.sln`, **x64 Debug** (RHI's Release|x64 is mis-set, see §6)
- [ ] Copy `Noesis.dll` + `NoesisApp.dll` + `NoesisEditor.dll` into the output dir (see §5)

---

## 1. Repo layout

```
EngineDev/
├── External/                 vendored third-party (mostly committed, see §3)
└── Solution/
    ├── Engine.sln            5 projects; x64 is the only maintained platform
    ├── RHI/                  RHI.dll   — DX12 + Vulkan backends, Win32 windowing, shaders
    ├── Engine/               Engine.dll — scene, resources, importers, Noesis UI layer
    ├── Game/                 Game.dll  — game code + Assets/
    ├── Editor/               Editor.exe — ImGui editor (DX12 backend)
    └── Standalone/           Standalone.exe — runtime host
```

Project settings worth knowing:
- `PlatformToolset` = `v143`, `WindowsTargetPlatformVersion` = `10.0` (means "latest installed SDK")
- `LanguageStandard` = `stdcpp23` everywhere except Editor Debug|Win32 (`stdcpp20`)
- Output dir `Solution\x64\<Config>\`, intermediates `<Project>\x64\<Config>\`

---

## 2. Tools to install

### Visual Studio 2022 (required)
Community edition: <https://visualstudio.microsoft.com/downloads/>

Workload: **Desktop development with C++**. Confirm these components:
- *MSVC v143 — VS 2022 C++ x64/x86 build tools (Latest)* — **must be ≥ 14.39**. The code compiles as
  C++23 and uses `std::source_location`, `std::format`, `std::ranges`. If an older side-by-side v143
  toolset (e.g. 14.38) gets selected, `stdcpp23` is not understood and the compiler silently falls
  back to C++14, and RHI fails with `C7525 inline variables require at least '/std:c++17'` and
  `'source_location': is not a member of 'std'`. Cure: install/select the latest v143, or pass
  `-p:VCToolsVersion=<latest>` to msbuild.
- *Windows 11 SDK* (10.0.22621 or 10.0.26100 — anything current)
- *C++ CMake tools* — optional, only for building ImGuizmo / node-editor samples

Verified-good combination on the old machine: MSBuild 17.14.40, MSVC **14.44.35207**, Windows SDK 10.0.26100.

### Vulkan SDK (required)
<https://vulkan.lunarg.com/sdk/home#windows> — install **1.4.313.0** or newer. The installer sets the
`VULKAN_SDK` environment variable, which every project depends on:
- `RHI` links `vulkan-1.lib` from `$(VULKAN_SDK)\Lib` and includes `$(VULKAN_SDK)\Include`
- `Engine`, `Game`, `Editor`, `Standalone` all add `$(VULKAN_SDK)\Include`

Verify in a fresh shell: `echo %VULKAN_SDK%` → e.g. `C:\VulkanSDK\1.4.313.0`.
The SDK also ships `dxc.exe` (`%VULKAN_SDK%\Bin`), needed by `Solution\RHI\CompileShaders.bat`.

> On the old machine `VULKAN_SDK` pointed at a non-default location
> (`C:\Users\SwaggyJ\Documents\Vinyl\VulkanAPI`). Any path works as long as the variable is set — but
> see §6 about four files that hardcode that old path.

### Git (required)
The repo contains paths up to **151 characters** (the NoesisGUI SDK). Before cloning:

```powershell
git config --global core.longpaths true
```

and enable Windows long paths once, from an elevated PowerShell:

```powershell
New-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem' `
  -Name LongPathsEnabled -Value 1 -PropertyType DWORD -Force
```

Then **clone to a short root path** — `C:\Dev\EngineDev` is safe. Cloning into a deep directory
(~130 chars) fails partway with `error: unable to create file ...: Filename too long` followed by
`warning: Clone succeeded, but checkout failed.`

### JetBrains Rider (optional)
The old machine used Rider alongside VS. `Solution/.idea/` and `*.DotSettings.user` are gitignored, so
personal layout and inspection settings do **not** travel with the repo. Rider opens `Engine.sln` directly.

### Hardware
A DirectX 12 capable GPU (the editor's ImGui backend is DX12 only) and a Vulkan 1.1+ driver for the
Vulkan backend.

---

## 3. Third-party dependencies

Everything the solution needs is committed, except `NoesisApp.dll`/`.lib`, which you build once from
the committed Noesis sources (§3a).

| Dependency | Version | Location | Notes |
|---|---|---|---|
| **Dear ImGui** | 1.92.8 (master branch, not docking) | `External/imgui/` | unmodified upstream extract — `imconfig.h` has no custom defines. `Engine` and `Editor` compile `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `backends/imgui_impl_dx12.cpp`, `backends/imgui_impl_win32.cpp` straight out of this folder; `Editor/imfilebrowser.h` includes `imgui.h`. To upgrade, replace the folder contents with a newer tag from <https://github.com/ocornut/imgui> |
| **assimp** | 5.4.3, prebuilt vc143 | `External/assimp/` | headers + `lib/Release/assimp-vc143-mt.lib` + `bin/assimp-vc143-mt.dll`; the DLL is auto-copied to `$(OutDir)` by an Engine post-build step |
| **NoesisGUI Native SDK** | 4.0.1 Indie (Windows) | `External/NoesisGUI-NativeSDK-win-4.0.1-Indie/` | full SDK. `Noesis.lib`/`.dll` ship prebuilt; `NoesisApp` must be built from source (§3a). Engine also includes headers from `Src/Packages/{App,Render}/*/Include`. Linked by Engine, Game, Editor, Standalone. DLLs are **not** auto-copied — see §5 |
| **ImGuizmo** | vendored snapshot | `External/ImGuizmo/` | present, but not referenced by any project yet |

### 3a. NoesisGUI 4.0.1 and building NoesisApp

The project used NoesisGUI 3.2.12 until 2026-11. That SDK is no longer downloadable, and the copy in
the repo was incomplete (see the `.gitignore` note below), so the project moved to **4.0.1**. The
zip from noesisengine.com is named `NoesisGUI-NativeSDK-win-4.0.0-Indie.zip`, but its `version.txt`
says 4.0.1. It is extracted to `External/NoesisGUI-NativeSDK-win-4.0.1-Indie/`.

4.0 ships **no prebuilt `NoesisApp.lib`/`.dll`**. Build it once from the SDK's own project, from a
Developer PowerShell (Release is fine even for a Debug engine build):

```powershell
msbuild External\NoesisGUI-NativeSDK-win-4.0.1-Indie\Src\Projects\NoesisApp\windows_x86_64\NoesisApp-vs2022.vcxproj `
  -m -p:Configuration=Release -p:Platform=x64
```

It writes `Bin\windows_x86_64\NoesisApp.dll` and `Lib\windows_x86_64\NoesisApp.lib` inside the SDK
folder. Its intermediates (`Src/Projects/**/build.vs*/`) are gitignored.

API changes from 3.2 that touched `Engine/NoesisUILayer.cpp`:
- `D3D12Factory::CreateDevice` no longer takes a fence.
- `D3D12Factory::SetCommandList` takes the last completed fence value plus the fence value of the frame
  being recorded (`D3D12CompletedFrame()` derives the first from the frames-in-flight count).
- The interactivity core moved into `NsGui`: `NsGui/Interaction.h`, `NsGui/BehaviorCollection.h`,
  `NsGui/TriggerBaseCollection.h` (replaces `NsApp/TriggerCollection.h`). Behaviors and actions such
  as `MouseDragElementBehavior` and `ControlStoryboardAction` are still in `NsApp`
  (`Src/Packages/App/Interactivity`).

**`.gitignore` fix.** The stock Visual Studio rule `**/[Pp]ackages/*` matched the Noesis SDK's
`Src/Packages/`, so the 3.2.12 commit silently left out headers the Engine includes (`NsApp/Launcher.h`
and others), and fresh clones could not build. `.gitignore` now re-includes
`External/NoesisGUI-NativeSDK-win-4.0.1-Indie/Src/Packages/*`. If you ever upgrade Noesis again,
update that path too.

### Removed: `External/imgui-node-editor`
It used to be recorded as a submodule gitlink with no `.gitmodules`, so clones got an empty folder that
`git submodule update --init` could not resolve. Nothing in the solution used it, so the gitlink was
dropped. If you want it back (0.9.4 was the version that had been vendored), clone it as plain files
like the other dependencies:

```powershell
git clone --depth 1 https://github.com/thedmd/imgui-node-editor.git External\imgui-node-editor
Remove-Item -Recurse -Force External\imgui-node-editor\.git
```

---

## 4. Clone and build

```powershell
cd C:\Dev
git clone https://github.com/enearle/EngineDev.git
cd EngineDev
git checkout NewArchitectureRough      # active branch; main is older
```

Build from the IDE (open `Solution\Engine.sln`, pick **x64 / Debug**, Build Solution) or from a
Developer PowerShell:

```powershell
cd C:\Dev\EngineDev\Solution
msbuild Engine.sln -m -p:Configuration=Debug -p:Platform=x64
```

**Only x64 is maintained** — the Win32 configurations are stale leftovers and are not expected to build.

---

## 5. After the first build — runtime bits

1. **Noesis DLLs (manual, once per output dir).** Nothing copies them. From `Solution\`:
   ```powershell
   $noesis = "..\External\NoesisGUI-NativeSDK-win-4.0.1-Indie\Bin\windows_x86_64"
   Copy-Item "$noesis\Noesis.dll","$noesis\NoesisApp.dll","$noesis\NoesisEditor.dll" .\x64\Debug\
   ```
   `assimp-vc143-mt.dll` arrives on its own via the Engine post-build copy.

2. **Working directory matters.** Runtime paths are relative to the *project* directory, which is what
   VS and Rider use by default (`$(ProjectDir)`) — not the output directory:
   - shaders: `../RHI/DirectX12/Shaders/CSO/` and `../RHI/Vulkan/Shaders/SPIRV/`
   - editor asset browser: `../Game/Assets`
   - Noesis XAML: `../Engine/MyXAML/...`

   So launch from the IDE. Double-clicking `x64\Debug\Editor.exe` will not find its data. From a
   shell, set the working directory to `Solution\Editor` (or `Solution\Standalone`).

   **Backends:** the Editor always runs on **DX12** (`GRAPHICS_SETTINGS.SetEditorMode(true)`, and its
   ImGui backend is DX12-only). Standalone runs on **Vulkan**. Noesis is initialized in `GameInit()`,
   so in the Editor it only starts when you press Play.

3. **Shaders are checked in precompiled** (`RHI/DirectX12/Shaders/CSO/*.cso` and
   `RHI/Vulkan/Shaders/SPIRV/*.spv`); no shader step runs during a normal build. After editing any
   `.hlsl`, regenerate both sets:
   ```powershell
   cd Solution\RHI
   .\CompileShaders.bat        # needs dxc.exe on PATH (Vulkan SDK Bin, or Windows SDK)
   ```

4. `Solution\Editor\registry.bin` is a generated asset-registry cache and is not in the repo — the
   editor recreates it.

---

## 6. Known issues to expect on the new machine

- **Hardcoded paths from the old machine are gone.** The per-file `AdditionalIncludeDirectories`
  overrides that pointed at `C:\Users\SwaggyJ\...` now use `$(SolutionDir)` / `$(VULKAN_SDK)`. They
  still *replace* the project-level include list for those files and define `_DEBUG` unconditionally.
- **Mesh rendering is broken at runtime (fallout from the `.asset` refactor, §7b).** On Vulkan, every
  mesh draw binds a 256-byte uniform buffer as its vertex/index buffer. The validation layer reports
  `vkCmdBindVertexBuffers` / `vkCmdBindIndexBuffer` usage-flag errors and
  `vkCmdDrawIndexed ... greater than the index buffer size`. The meshes' `VertexBufferID`/`IndexBufferID`
  are never set: `TempGameObject::UploadToGPU()` is never called. Calling it before
  `CreateDrawForMeshNode` was tried and did not fix this, so the cache or IDs are lost somewhere in
  `CreateAsset`/`MeshAsset`. The apps still run and exit cleanly.
- **Loading the demo meshes writes `.asset` files and `registry.bin`.** `GameInit()` imports
  `shells.fbx`/`Derp.fbx` through `GeometryImport::CreateMeshGroup`, which writes `*.asset` files next to
  the FBX in `Engine/Meshes/` and a `registry.bin` in the working directory. Neither is gitignored.
- Noesis logs `Image not found '../Engine/MyXAML/Assets/tarkov.png'` for `MainPage.xaml`. The image
  is not in the repo. This is harmless.
- **`RHI` Release|x64 is `ConfigurationType=Application`** while Debug|x64 is `DynamicLibrary`, so a
  Release build of RHI produces an exe instead of the DLL the rest of the solution expects. Fix before
  relying on Release.
- `Solution/Editor/registry.bin` (the asset-registry cache) is untracked and not covered by
  `.gitignore`, so it reappears in `git status` every time you run the editor. Ignore it, or add a rule.

---

## 7. Where the work was left (2026-06-03, commit `8356f20`)

Two threads were in flight, and they meet in the middle: the asset system needs to upload materials
to the GPU, and the renderer needs to group draws by those materials. The authoritative scratch list
is the TODO in `Engine/Resources/Assets/ImageAsset.cpp:33`:

> - Finish Image upload and import
> - Finish Material upload
> - Finish converting RHI to use Material bin

### 7a. Draw-call batching by material

**Goal** (commit `38978ea`): draw every mesh sharing a material before moving to the next material,
as the groundwork for instanced draws.

**Done:**
- `RHIStructures::MaterialDraw { std::vector<uint64_t> PerMaterialDescriptors; std::vector<IndexedDraw> Draws; }`
  added in `8356f20`.
- Engine side: `TempGameObject::AddMeshNode` → `CreateDrawForMeshNode`, plus
  `SceneNode::UploadToGPU` / `FreeGPUResources`.
- **Batching finished (2026-11).** Material grouping nests *inside* the existing pipeline-variant
  indexing. A variant is its own pipeline bind, so material binds have to sit within it:
  - `PipelineFrameContext::IndexedDrawBins` is `vector<vector<MaterialDraw>>`, indexed
    `[pipeline variant][material bucket]`. Index 0 is still the main pipeline and `i` is still
    `PipelineVariants[i - 1]`.
  - `Renderer::AddIndexedDrawToContext(context, draw, perMaterialDescriptors = {})` finds or creates
    the bucket whose `PerMaterialDescriptors` equal the ones passed, then appends to its `Draws`.
  - The render loop binds per-frame sets once per variant, then each bucket's `PerMaterialDescriptors`
    once, then each draw's `PerDrawDescriptors` before `DrawIndexed`. It reuses `BindDrawDescriptorSets`
    for the material bind. Every descriptor set carries its own set index (`SetKey`), so the
    material, per-draw and per-frame sets never overwrite each other.
  - `TempGameObject` passes the material's set as the bucket key, and only per-mesh sets (bones) go in
    `PerDrawDescriptors`. Shadow-pass draws have no material, so they share one empty-key bucket.
  - Verified on Vulkan: the shells split into 2 material buckets (12 + 1 meshes) and the skinned Derp
    gets its own bucket in variant 1. The DX12 draw path only runs when you press Play in the Editor
    and was not exercised.

**Not done:**
- Instanced draws are not started; `MaterialDraw::Draws` is the intended seam for them.
- Buckets are keyed by raw descriptor-set IDs. Once `MaterialAsset::LoadedMaterials` exists (§7b), keying
  by material asset would let two loads of the same material share a bucket.

### 7b. Asset management (`.asset` pipeline)

**Done:**
- Fully migrated off `.meta` sidecars to `.asset` files (commit `a172382`).
- Registry: `ResourceManager` keeps `AssetID → {ResourceType, FilePath}`, persisted to
  `registry.bin` (untracked, so a fresh clone starts empty — assets must be re-imported through the
  editor).
- Mesh / skinned-mesh import works end to end: `ResourceManager::Import` →
  `GeometryImport::CreateMeshGroup` → `CreateAsset(root, ResourceType::SceneNode, destDir)`, persisting
  the `SceneNode` hierarchy alongside per-node `MeshAsset`s.
- Import-time type sniffing in `Editor/Modals/Importer.cpp`: `ParsePNGFile` reads the PNG header and
  maps bit depth × channels onto `Texture{1..4}CH{8,16}`; `ParseFBXFile` distinguishes Mesh from
  MeshSkinned.
- `ImageAsset::Serialize` / `Deserialize` are written.

**Not done:**
- `ResourceManager::Import` (`ResourceManager.cpp:137`) returns an empty `AssetID` for everything except
  Mesh/MeshSkinned — *"Textures and other types not yet implemented under the new flow."* So the PNG
  sniffing above has no import path behind it yet.
- `ImageAsset::UploadToGPU` only does `delete CachedImage` — no GPU upload. `FreeGPUResources` is empty.
- `ImageAsset::Serialize` writes only the header (`Is16Bit`, `Width`, `Height`, `Channels`, `TotalSize`)
  and never `ImageData::Pixels`, so a written `.asset` currently carries no pixel payload.
  `ImageImport` (`RHI/RHI/Image/ImageImport.h`) has the loaders (`LoadImage_8Bit`, `LoadImage_16Bit`,
  `LoadImageSideBySide`) ready to feed it.
- `MaterialAsset::UploadToGPU` validates that every `Field` has a valid ID, then stops. The
  `LoadedMaterials` map (`AssetID → uint64_t` GPU handle) is declared but never populated, and
  `CastsShadows` is unused. The header records the intent: one instance of a material loaded at a time,
  instanced draw calls batched by material.
- `ResourceManager::ReadMetaFile` is still present, marked deprecated "until editor migrates off .meta
  sidecars".

### 7c. Suggested order when picking this back up

1. Finish `ImageAsset` (pixel payload in Serialize/Deserialize, real `UploadToGPU`/`FreeGPUResources`).
2. Open the texture branch of `ResourceManager::Import` so PNG import actually produces assets.
3. Fill in `MaterialAsset::UploadToGPU` + `LoadedMaterials`, giving materials a GPU handle and
   descriptor set.
4. ~~Rewrite `Renderer.cpp`'s two bin sites against `MaterialDraw`~~ — done, see §7a.
5. Then instanced draws on top of `MaterialDraw::Draws`.
