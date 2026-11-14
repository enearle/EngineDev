#pragma once
#include <string>
#include "RENDERER_API_Macro.h"
#include "RHI/RHIStructures.h"

// Loads a shader compiled by the Renderer project's build step for the active backend.
RENDERER_API RHIStructures::ShaderStage LoadShader(const std::string& name, const char* entryPoint = "main");

// Frees the bytecode once every pipeline that uses it has been created.
RENDERER_API void ReleaseShader(RHIStructures::ShaderStage& shader);
