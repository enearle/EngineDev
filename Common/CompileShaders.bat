@echo off
setlocal enabledelayedexpansion

set DX_OUTPUT_DIR=DirectX12\Shaders\CSO
set VULKAN_OUTPUT_DIR=Vulkan\Shaders\SPIRV

if not exist "%DX_OUTPUT_DIR%" mkdir "%DX_OUTPUT_DIR%"
if not exist "%VULKAN_OUTPUT_DIR%" mkdir "%VULKAN_OUTPUT_DIR%"

echo Compiling for DirectX 12...
fxc /T vs_5_1 /Fo "%DX_OUTPUT_DIR%\vs_rainbow.cso" DirectX12\Shaders\vs_rainbow.hlsl
fxc /T ps_5_1 /Fo "%DX_OUTPUT_DIR%\ps_rainbow.cso" DirectX12\Shaders\ps_rainbow.hlsl
fxc /T vs_5_1 /Fo "%DX_OUTPUT_DIR%\vs_quad.cso" DirectX12\Shaders\vs_quad.hlsl
fxc /T ps_5_1 /Fo "%DX_OUTPUT_DIR%\ps_quad.cso" DirectX12\Shaders\ps_quad.hlsl

echo Compiling for Vulkan...
glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_rainbow.spv" Vulkan\Shaders\vs_rainbow.glsl
glslangValidator -V -S frag -e main -o "%VULKAN_OUTPUT_DIR%\ps_rainbow.spv" Vulkan\Shaders\ps_rainbow.glsl
glslangValidator -V -S vert -e main -o "%VULKAN_OUTPUT_DIR%\vs_quad.spv" Vulkan\Shaders\vs_quad.glsl
glslangValidator -V -S frag -e main -o "%VULKAN_OUTPUT_DIR%\ps_quad.spv" Vulkan\Shaders\ps_quad.glsl

echo Done!
pause
