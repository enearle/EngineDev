import glob, os

SOL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Solution")

GUIDS = {
    "RHI": "{83D3C7A0-2C62-4D85-A13C-38689288E782}",
    "Renderer": "{2E83FB28-9DF6-4380-8C60-99210DC6C99B}",
    "Engine": "{E00B027A-9A1C-4548-B840-0AF98014B134}",
    "Game": "{5F805604-D030-43AC-B6B4-FAC34043FF75}",
    "Editor": "{71A3F751-DD06-4525-BB08-D0C28568436C}",
    "Standalone": "{EF144C9A-5ADA-4048-9AB1-051611E0749B}",
}

IMGUI_SOURCES = [
    r"$(ImGuiDir)imgui.cpp",
    r"$(ImGuiDir)imgui_draw.cpp",
    r"$(ImGuiDir)imgui_tables.cpp",
    r"$(ImGuiDir)imgui_widgets.cpp",
    r"$(ImGuiDir)backends\imgui_impl_win32.cpp",
    r"$(ImGuiDir)backends\imgui_impl_dx12.cpp",
    r"$(ImGuiDir)backends\imgui_impl_vulkan.cpp",
]

SHADER_TARGET = r"""  <Target Name="CompileShaders" BeforeTargets="ClCompile" Inputs="%(ShaderSource.FullPath);@(ShaderInclude)" Outputs="$(ShaderOutDir)DXIL\%(ShaderSource.OutputName).cso;$(ShaderOutDir)SPIRV\%(ShaderSource.OutputName).spv">
    <MakeDir Directories="$(ShaderOutDir)DXIL;$(ShaderOutDir)SPIRV" />
    <Message Importance="high" Text="dxc %(ShaderSource.OutputName)" />
    <Exec Command="&quot;$(DxcExe)&quot; -nologo -T %(ShaderSource.Profile) -E main %(ShaderSource.Arguments) -Fo &quot;$(ShaderOutDir)DXIL\%(ShaderSource.OutputName).cso&quot; &quot;%(ShaderSource.FullPath)&quot;" />
    <Exec Command="&quot;$(DxcExe)&quot; -nologo -T %(ShaderSource.Profile) -E main %(ShaderSource.Arguments) -spirv -fspv-target-env=vulkan1.1 -Fo &quot;$(ShaderOutDir)SPIRV\%(ShaderSource.OutputName).spv&quot; &quot;%(ShaderSource.FullPath)&quot;" />
  </Target>
  <Target Name="CleanShaders" AfterTargets="Clean">
    <RemoveDir Directories="$(ShaderOutDir)" />
  </Target>
"""

RUNTIME_COPY_TARGET = r"""  <Target Name="CopyRuntimeLibraries" AfterTargets="Build">
    <ItemGroup>
      <RuntimeLibrary Include="$(AssimpDir)bin\assimp-vc143-mt.dll" />
      <RuntimeLibrary Include="$(NoesisSDKDir)Bin\windows_x86_64\Noesis*.dll" />
    </ItemGroup>
    <Copy SourceFiles="@(RuntimeLibrary)" DestinationFolder="$(OutDir)" SkipUnchangedFiles="true" />
  </Target>
"""

PROJECTS = [
    dict(name="RHI", type="DynamicLibrary",
         defines="RHI_EXPORTS",
         includes=[],
         libdirs=[r"$(VULKAN_SDK)\Lib"], libs=["vulkan-1.lib"],
         refs=[]),
    dict(name="Renderer", type="DynamicLibrary",
         defines="RENDERER_EXPORTS",
         includes=[r"$(SolutionDir)RHI", r"$(SolutionDir)Renderer"],
         libdirs=[], libs=[],
         refs=["RHI"], shaders=True),
    dict(name="Engine", type="DynamicLibrary",
         defines="ENGINE_EXPORTS;IMGUI_API=__declspec(dllexport)",
         includes=[r"$(SolutionDir)RHI", r"$(SolutionDir)Renderer", r"$(SolutionDir)Engine", r"$(AssimpDir)include", r"$(ImGuiDir)", r"$(NoesisIncludes)"],
         libdirs=[r"$(VULKAN_SDK)\Lib"], libs=["vulkan-1.lib", "dxgi.lib", r"$(NoesisLibs)", r"$(AssimpDir)lib\Release\assimp-vc143-mt.lib"],
         refs=["RHI", "Renderer"], extra_sources=IMGUI_SOURCES, extra_targets=RUNTIME_COPY_TARGET),
    dict(name="Game", type="DynamicLibrary",
         defines="GAME_EXPORTS;IMGUI_API=__declspec(dllimport)",
         includes=[r"$(SolutionDir)RHI", r"$(SolutionDir)Renderer", r"$(SolutionDir)Engine", r"$(ImGuiDir)", r"$(NoesisIncludes)"],
         libdirs=[], libs=[r"$(NoesisLibs)"],
         refs=["RHI", "Renderer", "Engine"]),
    dict(name="Editor", type="Application",
         defines="_CONSOLE;IMGUI_API=__declspec(dllimport)",
         includes=[r"$(SolutionDir)RHI", r"$(SolutionDir)Renderer", r"$(SolutionDir)Engine", r"$(SolutionDir)Game", r"$(AssimpDir)include", r"$(ImGuiDir)", r"$(NoesisIncludes)"],
         libdirs=[], libs=[r"$(NoesisLibs)", r"$(AssimpDir)lib\Release\assimp-vc143-mt.lib"],
         refs=["RHI", "Renderer", "Engine", "Game"]),
    dict(name="Standalone", type="Application",
         defines="_CONSOLE;IMGUI_API=__declspec(dllimport)",
         includes=[r"$(SolutionDir)RHI", r"$(SolutionDir)Renderer", r"$(SolutionDir)Engine", r"$(SolutionDir)Game", r"$(ImGuiDir)"],
         libdirs=[], libs=[],
         refs=["RHI", "Renderer", "Engine", "Game"]),
]


def rel_files(project_dir, pattern):
    found = glob.glob(os.path.join(project_dir, "**", pattern), recursive=True)
    result = []
    for f in found:
        rel = os.path.relpath(f, project_dir)
        if rel.startswith("x64" + os.sep) or (os.sep + "x64" + os.sep) in rel or "Compiled" + os.sep in rel:
            continue
        result.append(rel)
    return sorted(result, key=str.lower)


def gen(p):
    name = p["name"]
    pdir = os.path.join(SOL, name)
    sources = rel_files(pdir, "*.cpp") + p.get("extra_sources", [])
    headers = rel_files(pdir, "*.h")
    out = []
    w = out.append
    w('<?xml version="1.0" encoding="utf-8"?>')
    w('<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">')
    w('  <ItemGroup Label="ProjectConfigurations">')
    for cfg in ("Debug", "Release"):
        w(f'    <ProjectConfiguration Include="{cfg}|x64">')
        w(f'      <Configuration>{cfg}</Configuration>')
        w('      <Platform>x64</Platform>')
        w('    </ProjectConfiguration>')
    w('  </ItemGroup>')
    w('  <PropertyGroup Label="Globals">')
    w('    <VCProjectVersion>17.0</VCProjectVersion>')
    w(f'    <ProjectGuid>{GUIDS[name]}</ProjectGuid>')
    w('    <Keyword>Win32Proj</Keyword>')
    w(f'    <RootNamespace>{name}</RootNamespace>')
    w('    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>')
    w('    <PreferredToolArchitecture>x64</PreferredToolArchitecture>')
    w('  </PropertyGroup>')
    w('  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />')
    for cfg in ("Debug", "Release"):
        w(f'  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'{cfg}|x64\'" Label="Configuration">')
        w(f'    <ConfigurationType>{p["type"]}</ConfigurationType>')
        w(f'    <UseDebugLibraries>{"true" if cfg == "Debug" else "false"}</UseDebugLibraries>')
        w('    <PlatformToolset>v143</PlatformToolset>')
        w('    <CharacterSet>Unicode</CharacterSet>')
        if cfg == "Release":
            w('    <WholeProgramOptimization>true</WholeProgramOptimization>')
        w('  </PropertyGroup>')
    w('  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />')
    w('  <ImportGroup Label="PropertySheets">')
    w('    <Import Project="$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props" Condition="exists(\'$(UserRootDir)\\Microsoft.Cpp.$(Platform).user.props\')" Label="LocalAppDataPlatform" />')
    w('    <Import Project="$(SolutionDir)Build\\Common.props" />')
    w('  </ImportGroup>')
    if p.get("shaders"):
        w('  <PropertyGroup>')
        w('    <ShaderOutDir>$(ProjectDir)Shaders\\Compiled\\</ShaderOutDir>')
        w('    <DxcExe>$(VULKAN_SDK)\\Bin\\dxc.exe</DxcExe>')
        w('  </PropertyGroup>')
    w('  <ItemDefinitionGroup>')
    w('    <ClCompile>')
    w(f'      <PreprocessorDefinitions>{p["defines"]};%(PreprocessorDefinitions)</PreprocessorDefinitions>')
    if p["includes"]:
        w(f'      <AdditionalIncludeDirectories>{";".join(p["includes"])};%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>')
    w('    </ClCompile>')
    if p["libs"] or p["libdirs"]:
        w('    <Link>')
        if p["libdirs"]:
            w(f'      <AdditionalLibraryDirectories>{";".join(p["libdirs"])};%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>')
        if p["libs"]:
            w(f'      <AdditionalDependencies>{";".join(p["libs"])};%(AdditionalDependencies)</AdditionalDependencies>')
        w('    </Link>')
    w('  </ItemDefinitionGroup>')
    w('  <ItemGroup>')
    for s in sources:
        w(f'    <ClCompile Include="{s}" />')
    w('  </ItemGroup>')
    w('  <ItemGroup>')
    for h in headers:
        w(f'    <ClInclude Include="{h}" />')
    w('  </ItemGroup>')
    if p.get("shaders"):
        w('  <ItemGroup>')
        surfaces = [os.path.splitext(os.path.basename(f))[0] for f in rel_files(pdir, os.path.join("Surfaces", "*.hlsli"))]
        for s in rel_files(pdir, "*.hlsl"):
            base = os.path.splitext(os.path.basename(s))[0]
            profile = {"vs": "vs_6_1", "ps": "ps_6_1", "cs": "cs_6_1"}[base[:2]]
            # ps_gbuffer is compiled once per material type's surface file.
            variants = [(f"{base}_{name}", f"-D SURFACE={name}") for name in surfaces] if base == "ps_gbuffer" else [(base, "")]
            for output, arguments in variants:
                w(f'    <ShaderSource Include="{s}">')
                w(f'      <Profile>{profile}</Profile>')
                w(f'      <OutputName>{output}</OutputName>')
                w(f'      <Arguments>{arguments}</Arguments>')
                w('    </ShaderSource>')
        for s in rel_files(pdir, "*.hlsli"):
            w(f'    <ShaderInclude Include="{s}" />')
        w('  </ItemGroup>')
    if p["refs"]:
        w('  <ItemGroup>')
        for r in p["refs"]:
            w(f'    <ProjectReference Include="..\\{r}\\{r}.vcxproj">')
            w(f'      <Project>{GUIDS[r].lower()}</Project>')
            w('    </ProjectReference>')
        w('  </ItemGroup>')
    w('  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />')
    if p.get("shaders"):
        out.append(SHADER_TARGET.rstrip("\n"))
    if p.get("extra_targets"):
        out.append(p["extra_targets"].rstrip("\n"))
    w('</Project>')
    path = os.path.join(pdir, name + ".vcxproj")
    with open(path, "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write("\n".join(out) + "\n")
    print(name, len(sources), "sources", len(headers), "headers")


def gen_sln():
    cpp = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}"
    lines = ["", "Microsoft Visual Studio Solution File, Format Version 12.00", "# Visual Studio Version 17",
             "VisualStudioVersion = 17.8.34330.188", "MinimumVisualStudioVersion = 10.0.40219.1"]
    for p in PROJECTS:
        n = p["name"]
        lines.append(f'Project("{cpp}") = "{n}", "{n}\\{n}.vcxproj", "{GUIDS[n]}"')
        lines.append("EndProject")
    lines += ["Global", "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution",
              "\t\tDebug|x64 = Debug|x64", "\t\tRelease|x64 = Release|x64", "\tEndGlobalSection",
              "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution"]
    for p in PROJECTS:
        g = GUIDS[p["name"]]
        for cfg in ("Debug", "Release"):
            lines.append(f"\t\t{g}.{cfg}|x64.ActiveCfg = {cfg}|x64")
            lines.append(f"\t\t{g}.{cfg}|x64.Build.0 = {cfg}|x64")
    lines += ["\tEndGlobalSection", "\tGlobalSection(SolutionProperties) = preSolution", "\t\tHideSolutionNode = FALSE",
              "\tEndGlobalSection", "\tGlobalSection(ExtensibilityGlobals) = postSolution",
              "\t\tSolutionGuid = {C1FCFA8E-4A5F-4F23-8CA8-0707D04B8DE0}", "\tEndGlobalSection", "EndGlobal", ""]
    with open(os.path.join(SOL, "Engine.sln"), "w", encoding="utf-8-sig", newline="\r\n") as f:
        f.write("\n".join(lines))


for p in PROJECTS:
    gen(p)
gen_sln()
