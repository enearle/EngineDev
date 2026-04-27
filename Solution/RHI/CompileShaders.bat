@echo off
setlocal enabledelayedexpansion

set DX_OUTPUT_DIR=DirectX12\Shaders\CSO
set VULKAN_OUTPUT_DIR=Vulkan\Shaders\SPIRV

if not exist "%DX_OUTPUT_DIR%" mkdir "%DX_OUTPUT_DIR%"
if not exist "%VULKAN_OUTPUT_DIR%" mkdir "%VULKAN_OUTPUT_DIR%"

echo Compiling for DirectX 12...

dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_rainbow.cso" DirectX12\Shaders\vs_rainbow.hlsl
dxc -T ps_6_1 -E main -Fo "%DX_OUTPUT_DIR%\ps_rainbow.cso" DirectX12\Shaders\ps_rainbow.hlsl

dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_quad.cso" DirectX12\Shaders\vs_quad.hlsl
dxc -T ps_6_1 -E main -Fo "%DX_OUTPUT_DIR%\ps_quad.cso" DirectX12\Shaders\ps_quad.hlsl

dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_pbr_skinned.cso" DirectX12\Shaders\vs_pbr_skinned.hlsl
dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_pbr.cso" DirectX12\Shaders\vs_pbr.hlsl
dxc -T ps_6_1 -E main -Fo "%DX_OUTPUT_DIR%\ps_pbr.cso" DirectX12\Shaders\ps_pbr.hlsl

dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_lighting.cso" DirectX12\Shaders\vs_lighting.hlsl
dxc -T ps_6_1 -E main -Fo "%DX_OUTPUT_DIR%\ps_lighting.cso" DirectX12\Shaders\ps_lighting.hlsl

dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_vsm_skinned.cso" DirectX12\Shaders\vs_vsm_skinned.hlsl
dxc -T vs_6_1 -E main -Fo "%DX_OUTPUT_DIR%\vs_vsm.cso" DirectX12\Shaders\vs_vsm.hlsl
dxc -T ps_6_1 -E main -Fo "%DX_OUTPUT_DIR%\ps_vsm.cso" DirectX12\Shaders\ps_vsm.hlsl


echo Compiling for Vulkan...

glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_rainbow.spv" Vulkan\Shaders\vs_rainbow.glsl
glslangValidator -V -S frag -e main -o "%VULKAN_OUTPUT_DIR%\ps_rainbow.spv" Vulkan\Shaders\ps_rainbow.glsl

glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_quad.spv" Vulkan\Shaders\vs_quad.glsl
glslangValidator -V -S frag -e main -o "%VULKAN_OUTPUT_DIR%\ps_quad.spv" Vulkan\Shaders\ps_quad.glsl

glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_pbr_skinned.spv" Vulkan\Shaders\vs_pbr_skinned.glsl
glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_pbr.spv" Vulkan\Shaders\vs_pbr.glsl
glslangValidator -V -S frag -e main -o "%VULKAN_OUTPUT_DIR%\ps_pbr.spv" Vulkan\Shaders\ps_pbr.glsl

glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_lighting.spv" Vulkan\Shaders\vs_lighting.glsl
glslangValidator -V -S frag -e main -o "%VULKAN_OUTPUT_DIR%\ps_lighting.spv" Vulkan\Shaders\ps_lighting.glsl

glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_vsm_skinned.spv" Vulkan\Shaders\vs_vsm_skinned.glsl
glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_vsm.spv" Vulkan\Shaders\vs_vsm.glsl
glslangValidator -V -S frag -e main -o "%VULKAN_OUTPUT_DIR%\ps_vsm.spv" Vulkan\Shaders\ps_vsm.glsl

echo Done!
pause
