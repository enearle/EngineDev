@echo off
setlocal enabledelayedexpansion

set DX_OUTPUT_DIR=DirectX12\Shaders\CSO
set VULKAN_OUTPUT_DIR=Vulkan\Shaders\SPIRV
set HLSL_SOURCE_DIR=DirectX12\Shaders

if not exist "%DX_OUTPUT_DIR%" mkdir "%DX_OUTPUT_DIR%"
if not exist "%VULKAN_OUTPUT_DIR%" mkdir "%VULKAN_OUTPUT_DIR%"

echo Compiling for DirectX 12...

dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_pbr_skinned.cso" "%HLSL_SOURCE_DIR%\vs_pbr_skinned.hlsl"
dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_pbr.cso" "%HLSL_SOURCE_DIR%\vs_pbr.hlsl"
dxc -T ps_6_1 -E main -Fo "%DX_OUTPUT_DIR%\ps_pbr.cso" "%HLSL_SOURCE_DIR%\ps_pbr.hlsl"

dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_lighting.cso" "%HLSL_SOURCE_DIR%\vs_lighting.hlsl"
dxc -T ps_6_1 -E main -Fo "%DX_OUTPUT_DIR%\ps_lighting.cso" "%HLSL_SOURCE_DIR%\ps_lighting.hlsl"

dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_vsm_skinned.cso" "%HLSL_SOURCE_DIR%\vs_vsm_skinned.hlsl"
dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_vsm.cso" "%HLSL_SOURCE_DIR%\vs_vsm.hlsl"
dxc -T ps_6_1 -E main -Fo "%DX_OUTPUT_DIR%\ps_vsm.cso" "%HLSL_SOURCE_DIR%\ps_vsm.hlsl"

echo Compiling for Vulkan (HLSL to SPIR-V via DXC)...

dxc -T vs_6_1 -E main -spirv -fspv-target-env=vulkan1.1 -Fo "%VULKAN_OUTPUT_DIR%\vs_pbr_skinned.spv" "%HLSL_SOURCE_DIR%\vs_pbr_skinned.hlsl"
dxc -T vs_6_1 -E main -spirv -fspv-target-env=vulkan1.1 -Fo "%VULKAN_OUTPUT_DIR%\vs_pbr.spv" "%HLSL_SOURCE_DIR%\vs_pbr.hlsl"
dxc -T ps_6_1 -E main -spirv -fspv-target-env=vulkan1.1 -Fo "%VULKAN_OUTPUT_DIR%\ps_pbr.spv" "%HLSL_SOURCE_DIR%\ps_pbr.hlsl"

dxc -T vs_6_1 -E main -spirv -fspv-target-env=vulkan1.1 -Fo "%VULKAN_OUTPUT_DIR%\vs_lighting.spv" "%HLSL_SOURCE_DIR%\vs_lighting.hlsl"
dxc -T ps_6_1 -E main -spirv -fspv-target-env=vulkan1.1 -Fo "%VULKAN_OUTPUT_DIR%\ps_lighting.spv" "%HLSL_SOURCE_DIR%\ps_lighting.hlsl"

dxc -T vs_6_1 -E main -spirv -fspv-target-env=vulkan1.1 -Fo "%VULKAN_OUTPUT_DIR%\vs_vsm_skinned.spv" "%HLSL_SOURCE_DIR%\vs_vsm_skinned.hlsl"
dxc -T vs_6_1 -E main -spirv -fspv-target-env=vulkan1.1 -Fo "%VULKAN_OUTPUT_DIR%\vs_vsm.spv" "%HLSL_SOURCE_DIR%\vs_vsm.hlsl"
dxc -T ps_6_1 -E main -spirv -fspv-target-env=vulkan1.1 -Fo "%VULKAN_OUTPUT_DIR%\ps_vsm.spv" "%HLSL_SOURCE_DIR%\ps_vsm.hlsl"

echo Done!
pause