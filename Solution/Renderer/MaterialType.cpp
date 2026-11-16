#include "MaterialType.h"

#include <stdexcept>

static std::vector<MaterialTypeDesc> Types;

MaterialTypeID MaterialTypes::Register(MaterialTypeDesc desc)
{
    if (Types.size() >= MaxTypes)
        throw std::runtime_error("Too many material types for the batch key: " + desc.Name);
    if (desc.Textures.size() > MaxTextures)
        throw std::runtime_error("Material type " + desc.Name + " has more than " + std::to_string(MaxTextures) + " textures");

    // HLSL packs a cbuffer in 16-byte rows; a vector never straddles two.
    uint32_t offset = 0;
    for (MaterialField& field : desc.Fields)
    {
        if (field.Components < 1 || field.Components > 4 || (!field.Default.empty() && field.Default.size() != field.Components))
            throw std::runtime_error("Material type " + desc.Name + " has a bad field: " + field.Name);
        uint32_t size = field.Components * 4;
        if (offset % 16 + size > 16)
            offset = (offset + 15) / 16 * 16;
        field.Offset = offset;
        offset += size;
    }
    if (offset > ParameterBlockSize)
        throw std::runtime_error("Material type " + desc.Name + " has more parameters than fit in " + std::to_string(ParameterBlockSize) + " bytes");

    Types.push_back(std::move(desc));
    return static_cast<MaterialTypeID>(Types.size() - 1);
}

void MaterialTypes::RegisterBuiltInTypes()
{
    Register({ .Name = "textured_pbr", .Textures = {
        { "albedo", true },
        { "normal", false },
        { "metallic_roughness_occlusion", false },
    } });

    Register({ .Name = "flat", .Fields = {
        { "albedo", 3, { 0.7f, 0.7f, 0.7f } },
        { "metallic", 1, { 0.0f } },
        { "roughness", 1, { 0.6f } },
    } });

    // The texture is a Paging texture ID, set at runtime once the texture is enlisted.
    Register({ .Name = "paged_albedo", .Fields = {
        { "paged_texture", 1, { 0.0f } },
        { "metallic", 1, { 0.0f } },
        { "roughness", 1, { 0.8f } },
    } });
}

void MaterialTypes::Clear()
{
    Types.clear();
}

uint32_t MaterialTypes::GetCount()
{
    return static_cast<uint32_t>(Types.size());
}

const MaterialTypeDesc& MaterialTypes::Get(MaterialTypeID type)
{
    return Types[type];
}

bool MaterialTypes::Find(const std::string& name, MaterialTypeID& type)
{
    for (MaterialTypeID i = 0; i < Types.size(); i++)
        if (Types[i].Name == name)
        {
            type = i;
            return true;
        }
    return false;
}
