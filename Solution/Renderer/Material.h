#pragma once
#include <map>
#include <string>
#include <vector>

// A material as written in Engine/Materials/<name>.material, a text file of "key = value" lines
// where # starts a comment. "type" names the material type; every other key is a field of the type,
// whose value is its numbers, or a texture slot, whose value is one image name in Engine/Textures, or
// three whose first channels are packed into red, green and blue.
struct MaterialDesc
{
    std::string Type;
    std::map<std::string, std::vector<std::string>> Values;
};

namespace MaterialFiles
{
    // False when the file does not exist.
    bool Read(const std::string& name, MaterialDesc& desc);

    // The file-name convention that predates material files: Textures/<name>_diff, _norm, and
    // _metal, _rough and _ao, as a textured_pbr material. False when <name>_diff does not exist.
    bool ReadConvention(const std::string& name, MaterialDesc& desc);
}
