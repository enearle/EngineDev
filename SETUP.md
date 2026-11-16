# Setup: EngineDev on a fresh machine

Everything needed to go from a bare Windows machine to a building, running solution. The short form
is in [README.md](README.md); the plan and its status are in
[Docs/RendererPlan.md](Docs/RendererPlan.md).

Last verified 14 November 2026 on branch `Dev-2`: Debug and Release x64 with MSVC 14.44, Vulkan SDK
1.4.363 and NoesisGUI 4.0.1, running Standalone and Editor on both backends.

---

## 0. Checklist

- [ ] Visual Studio 2022 (17.9+) with **Desktop development with C++**, latest MSVC v143, Windows 11 SDK
- [ ] Vulkan SDK 1.4.313 or newer (LunarG). It sets `VULKAN_SDK` and supplies `dxc`
- [ ] NoesisGUI 4.0.1 Native SDK extracted locally, and `NoesisApp` built once from it (section 3a)
- [ ] `git clone https://github.com/enearle/EngineDev.git`
- [ ] Build `Solution/Engine.sln`, x64, Debug or Release
- [ ] Run with the project directory as the working directory (section 5)

---

## 1. Repository layout

```
EngineDev/
├── Docs/                     plan and handoff notes
├── External/                 vendored third-party code (Noesis is not committed, see section 3)
├── Tools/
│   ├── GenerateProjects.py   writes the .vcxproj files and Engine.sln
│   └── RunAndCapture.ps1     runs an executable, captures stdout, stderr and a screenshot
└── Solution/
    ├── Engine.sln            6 projects, x64 Debug and Release only
    ├── Build/Common.props    settings shared by every project
    ├── RHI/                  RHI.dll: device, resources, pipelines, command recording, both backends
    ├── Renderer/             Renderer.dll: pass graph, passes, registries, draw lists, HLSL shaders
    ├── Engine/               Engine.dll: scene, assets, importers, input, Noesis and ImGui layers
    ├── Game/                 Game.dll: game code and Assets/
    ├── Editor/               Editor.exe: Dear ImGui editor
    └── Standalone/           Standalone.exe: runtime host
```

Each layer depends only on the ones above it in this list. Neither NoesisGUI nor Dear ImGui is used
inside `RHI` or `Renderer`; UI libraries attach to the renderer through
`Renderer::EnlistExternalPass` and get native handles from `ForwardInterface`.

The `.vcxproj` files and `Engine.sln` are generated. After adding or removing source or shader
files, run:

```
python Tools/GenerateProjects.py
```

Project settings that matter:

- `PlatformToolset` v143, `LanguageStandard` `stdcpp23`, `WindowsTargetPlatformVersion` 10.0 (latest
  installed SDK).
- Output in `Solution\x64\<Config>\`, intermediates in `<Project>\x64\<Config>\<Project>\`.
- `RHI`, `Renderer`, `Engine` and `Game` are DLLs in both configurations.

---

## 2. Tools

### Visual Studio 2022 (required)

Workload **Desktop development with C++**, with:

- *MSVC v143, VS 2022 C++ x64/x86 build tools (Latest)*, version 14.39 or newer. The code is C++23
  and uses `std::source_location`, `std::format` and `std::ranges`. If an older side-by-side v143
  toolset is selected, `stdcpp23` is not understood and the compiler falls back to C++14 with
  errors such as `C7525 inline variables require at least '/std:c++17'`. Install the latest v143
  or pass `-p:VCToolsVersion=<latest>` to msbuild.
- *Windows 11 SDK* (10.0.22621 or 10.0.26100).

### Vulkan SDK (required)

<https://vulkan.lunarg.com/sdk/home#windows>, 1.4.313 or newer. The installer sets `VULKAN_SDK`,
which every project uses for headers and `vulkan-1.lib`. The `Renderer` project also runs
`%VULKAN_SDK%\Bin\dxc.exe` to compile shaders (section 4).

### Git

`git config --global core.longpaths true` is still a good idea; the deepest committed paths are
under `External/`.

### Hardware

A Direct3D 12 GPU with Shader Model 6.1 and a Vulkan 1.3 driver with multiview.

---

## 3. Third-party dependencies

| Dependency | Version | Location | Notes |
| --- | --- | --- | --- |
| Dear ImGui | 1.92.8 (master) | `External/imgui/` | Unmodified. Compiled once into `Engine.dll` and exported with `IMGUI_API`; `Game`, `Editor` and `Standalone` import it |
| assimp | 5.4.3, prebuilt vc143 | `External/assimp/` | `assimp-vc143-mt.dll` is copied to the output directory by an Engine post-build step |
| NoesisGUI Native SDK | 4.0.1 Indie | not committed | See 3a. The Noesis DLLs are copied to the output directory by the same post-build step |
| ImGuizmo | snapshot | `External/ImGuizmo/` | Not referenced by any project yet |

### 3a. NoesisGUI

The SDK's licence does not allow redistributing it, so it is not in the repository. Download the
4.0 Native SDK for Windows from noesisengine.com (the zip is named `...-4.0.0-Indie.zip`, its
`version.txt` says 4.0.1) and either:

- extract it to `External/NoesisGUI-NativeSDK-win-4.0.1-Indie/` (the default in `Common.props`), or
- extract it anywhere and set the `NOESIS_SDK` environment variable to its root.

4.0 ships no prebuilt `NoesisApp`. Build it once from a Developer PowerShell; Release is fine for a
Debug engine build:

```
msbuild <NoesisSDK>\Src\Projects\NoesisApp\windows_x86_64\NoesisApp-vs2022.vcxproj -m -p:Configuration=Release -p:Platform=x64
```

It writes `Bin\windows_x86_64\NoesisApp.dll` and `Lib\windows_x86_64\NoesisApp.lib` inside the SDK.

API changes from 3.2 that touched `Engine/UI/NoesisUILayer.cpp`:

- `D3D12Factory::CreateDevice` no longer takes a fence.
- `D3D12Factory::SetCommandList` takes the last completed fence value and the fence value of the
  frame being recorded.
- The interactivity core moved into `NsGui` (`NsGui/Interaction.h` and friends). Behaviors and
  actions are still in `NsApp`.

---

## 4. Build

From the IDE: open `Solution\Engine.sln`, pick x64 and Debug or Release, Build Solution. From a
shell:

```
msbuild Solution\Engine.sln -m -p:Configuration=Debug -p:Platform=x64
```

Shaders are compiled by a build step of the `Renderer` project. Every `Renderer/Shaders/*.hlsl` is
compiled with `dxc` to DXIL and to SPIR-V in `Renderer/Shaders/Compiled/{DXIL,SPIRV}/`, which is
gitignored. A shader is rebuilt when its source changes. Code loads them by name with
`LoadShader(name)` from `Renderer/ShaderLibrary.h`.

---

## 5. Running

**Working directory.** Runtime paths are relative to the project directory, which is what Visual
Studio and Rider use by default. From a shell, set the working directory to `Solution\Standalone`
or `Solution\Editor`:

- shaders: `../Renderer/Shaders/Compiled/`
- meshes and textures: `../Engine/Meshes/` and `../Engine/Textures/`
- editor asset browser: `../Game/Assets`
- Noesis XAML: `../Engine/MyXAML/`

**Backends.** Standalone defaults to Vulkan and the Editor to Direct3D 12. `-vulkan` or `-dx12`
overrides either. The Editor runs the game when you press Play, or at once with `-play`. `-stress`
adds 10,000 objects, `-freeze-animation` holds the animated mesh still, `-novsync` turns vsync
off (it is on by default), and `-compute-test` runs the Phase 2 gate test, which prints
`Compute test: N readbacks matched, latency A to B frames` on exit. `-animation-test` runs the
Phase 3 checks (the bind pose and GPU skinning against the CPU), and `-crowd N` adds N animated
copies of the character, 100 when N is left out.

**Character.** The animated character is licensed and is not in the repository. Put it at
`TempAssets/CharBlend.fbx` (ignored by git), exported from Blender, or point `-character path` at
another file. Without it the Derp stands in. The game also prints the last frame's statistics,
GPU times per pass and animation figures on exit.

**Validation.** Debug builds enable the Vulkan validation layer and the Direct3D 12 debug layer; both
print to stderr. `Tools/RunAndCapture.ps1` runs an executable for a few seconds and saves stdout,
stderr, the exit code and a screenshot, which is the quickest way to check both backends.

**Generated files.** Loading the demo meshes writes `*.asset` files next to the FBX files in
`Engine/Meshes/` and a `registry.bin` in the working directory. Both are gitignored.

---

## 6. Known issues

- On Direct3D 12 the debug layer reports one message from Noesis's own offscreen render targets
  (a render target "must be initialized with a Discard/Clear/Copy" operation). It does not appear
  when Noesis is not started. It is treated as third-party.
- Noesis logs `Image not found '../Engine/MyXAML/Assets/tarkov.png'` for `MainPage.xaml`. The image
  is not in the repository; this is harmless.
- Meshes and textures upload synchronously: each upload waits for the GPU.
- `ImageAsset` and `MaterialAsset` are stubs from unfinished asset work: textures are still loaded
  by file name through `Material`, and `ResourceManager::Import` only handles meshes.
