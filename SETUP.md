# Setup — EngineDev on a fresh machine

Everything needed to go from a bare Windows box to a building/running solution.
Last verified: 2026-08-30 (branch `NewArchitectureRough`, commit `8356f20`) by cloning the repo into
a clean directory and building it end to end.

---

## 0. TL;DR checklist

- [ ] Visual Studio 2022 (17.9+) with **Desktop development with C++**, latest MSVC v143 toolset, Windows 11 SDK
- [ ] Vulkan SDK 1.4.313.0 (LunarG) — sets `VULKAN_SDK`, and supplies `dxc`
- [ ] `git config --global core.longpaths true`, clone to a **short path** (e.g. `C:\Dev\EngineDev`)
- [ ] `git clone https://github.com/enearle/EngineDev.git`
- [ ] Build `Solution/Engine.sln`, **x64 only** (Debug or Release)
- [ ] Copy `Noesis.dll` + `NoesisApp.dll` into the output dir (see §5)

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

Everything the solution needs is committed — a clone builds with no extra downloads.

| Dependency | Version | Location | Notes |
|---|---|---|---|
| **Dear ImGui** | 1.92.8 (master branch, not docking) | `External/imgui/` | unmodified upstream extract — `imconfig.h` has no custom defines. `Engine` and `Editor` compile `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `backends/imgui_impl_dx12.cpp`, `backends/imgui_impl_win32.cpp` straight out of this folder; `Editor/imfilebrowser.h` includes `imgui.h`. To upgrade, replace the folder contents with a newer tag from <https://github.com/ocornut/imgui> |
| **assimp** | 5.4.3, prebuilt vc143 | `External/assimp/` | headers + `lib/Release/assimp-vc143-mt.lib` + `bin/assimp-vc143-mt.dll`; the DLL is auto-copied to `$(OutDir)` by an Engine post-build step |
| **NoesisGUI Native SDK** | 3.2.12 Indie (Windows) | `External/NoesisGUI-NativeSDK-win-3.2.12-Indie/` | full SDK including `Lib/windows_x86_64/*.lib` and `Bin/windows_x86_64/*.dll`; linked by Engine, Game, Editor, Standalone. DLLs are **not** auto-copied — see §5 |
| **ImGuizmo** | vendored snapshot | `External/ImGuizmo/` | present, but not referenced by any project yet |

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

1. **Noesis DLLs (manual, once per output dir).** Nothing copies them; on the old machine they had been
   copied by hand into `Solution\x64\Debug`:
   ```powershell
   $noesis = "..\External\NoesisGUI-NativeSDK-win-3.2.12-Indie\Bin\windows_x86_64"
   Copy-Item "$noesis\Noesis.dll","$noesis\NoesisApp.dll" .\x64\Debug\
   Copy-Item "$noesis\Noesis.dll","$noesis\NoesisApp.dll" .\x64\Release\
   ```
   `assimp-vc143-mt.dll` arrives on its own via the Engine post-build copy.

2. **Working directory matters.** Runtime paths are relative to the *project* directory, which is what
   VS and Rider use by default (`$(ProjectDir)`) — not the output directory:
   - shaders: `../RHI/DirectX12/Shaders/CSO/` and `../RHI/Vulkan/Shaders/SPIRV/`
   - editor asset browser: `../Game/Assets`
   - Noesis XAML: `../Engine/MyXAML/...`

   So launch from the IDE. Double-clicking `x64\Debug\Editor.exe` will not find its data.

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

- **`NewArchitectureRough` HEAD does not compile.** `RHI/Renderer.cpp` is mid-refactor against the new
  `RHIStructures::MaterialDraw` bin layout (`IndexedDrawBins` is now `vector<MaterialDraw>`, but
  `Renderer.cpp` still calls `push_back` / `empty` / `size` / `[]` on the bin elements — errors around
  lines 144, 219, 228 and 254–256). Pre-existing work in progress, not a setup problem; everything else
  in the solution compiles. **See §7 for what that refactor was heading toward.**
- **Hardcoded absolute paths from the old machine** survive in four per-file
  `AdditionalIncludeDirectories` overrides pointing at `C:\Users\SwaggyJ\Desktop\GameEnginesDev2\...`
  and `C:\Users\SwaggyJ\Documents\Vinyl\VulkanAPI\Include`:
  - `Engine/Engine.vcxproj` — `Input\InputEventSystem.cpp`, `Resources\ResourceManager.cpp`, `Resources\UUID.cpp`
  - `Editor/Editor.vcxproj` — `Modals\Importer.cpp`

  They *replace* rather than extend the project-level include list for those files, and unconditionally
  define `_DEBUG`. They do not break the build today, but delete them when convenient.
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
  added, and `PipelineFrameContext::IndexedDrawBins` changed from `vector<vector<IndexedDraw>>` to
  `vector<MaterialDraw>` (`RHI/RHI/RHIStructures.h:661`).
- Engine side started: `TempGameObject::AddMeshNode` → `CreateDrawForMeshNode`, plus
  `SceneNode::UploadToGPU` / `FreeGPUResources`.

**Not done — this is the compile break in §6.** `RHI/Renderer.cpp` still treats a bin as a flat vector
of draws:
- `AddIndexedDrawToContext` (line ~144) does `IndexedDrawBins[draw.PipelineVarientID].push_back(draw)`.
  It needs to find-or-create the `MaterialDraw` for the draw's material and push onto its `.Draws`.
- The render loop (lines ~217–262) calls `.empty()`, `.size()` and `[j]` on the bin. It needs to walk
  the `MaterialDraw`s, bind `PerMaterialDescriptors` once per material (a per-material bind point next
  to the existing `Executor->BindPipelineDescriptorSets` / `BindDrawDescriptorSets` pair), then loop
  that material's `Draws`.
- **Open design question:** bins are currently indexed *by pipeline variant* —
  `IndexedDrawBins.resize(pipeline->GetPipelineVariantCount() + 1)` at line 130, and index `i` maps to
  `mainPipeline->PipelineVariants[i - 1]` at line ~231. Material grouping has to nest inside that
  indexing, or the indexing scheme has to change. That decision was still open.
- Instanced draws are not started; `MaterialDraw` is the intended seam for them.

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
4. Rewrite `Renderer.cpp`'s two bin sites against `MaterialDraw` — that both fixes the build and lands
   the batching.
5. Then instanced draws on top of `MaterialDraw::Draws`.
