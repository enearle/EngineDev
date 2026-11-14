#include "ShaderLibrary.h"

static const char* COMPILED_SHADER_DIRECTORY = "../Renderer/Shaders/Compiled/";

RHIStructures::ShaderStage LoadShader(const std::string& name, const char* entryPoint)
{
    return RHIStructures::ImportShader(COMPILED_SHADER_DIRECTORY, name, entryPoint);
}
