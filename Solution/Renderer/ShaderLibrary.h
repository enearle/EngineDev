#pragma once
#include <string>
#include "RENDERER_API_Macro.h"
#include "RHI/RHIStructures.h"

// Loads a shader compiled by the Renderer project's build step for the active backend.
RENDERER_API RHIStructures::ShaderStage LoadShader(const std::string& name, const char* entryPoint = "main");
