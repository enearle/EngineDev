#include "MaterialRegistry.h"

#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include "Material.h"
#include "MipGenerator.h"
#include "Renderer.h"
#include "Image/ImageImport.h"
#include "RHI/BufferAllocator.h"
#include "RHI/PipelineExecutor.h"
#include "RHI/PipelineLayout.h"
#include "RHI/RHIConstants.h"

using namespace RHIStructures;

struct MaterialRecord
{
    std::string Name;
    MaterialTypeID Type = 0;
    std::array<uint8_t, MaterialTypes::ParameterBlockSize> Parameters = {};
    std::vector<ImageHandle> Images;
    DescriptorSetHandle Set = {};
    uint32_t References = 0;
    uint64_t ParameterFrame = UINT64_MAX;
    uint32_t ParameterOffset = 0;
};

static HandlePool<MaterialRecord, MaterialHandle> Materials;
static std::unordered_map<std::string, MaterialHandle> MaterialsByName;
static ImageHandle DefaultImage = {};

std::vector<DescriptorBinding> MaterialRegistry::GetMaterialSetBindings()
{
    std::vector<DescriptorBinding> bindings = {
        { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = MaterialSetIndex, .Count = 1, .Size = MaterialTypes::ParameterBlockSize },
    };
    for (uint32_t slot = 0; slot < MaterialTypes::MaxTextures; slot++)
        bindings.push_back({ .Type = DescriptorType::SampledImage, .Slot = FirstTextureSlot + slot, .Set = MaterialSetIndex, .Count = 1 });
    return bindings;
}

void MaterialRegistry::Start()
{
    static const uint8_t white[4] = { 255, 255, 255, 255 };
    ImageDesc desc = RHIConstants::DefaultTextureDesc;
    desc.Width = 1;
    desc.Height = 1;
    desc.Size = sizeof(white);
    desc.InitialData = white;
    DefaultImage = BufferAllocator::GetInstance()->CreateImage(desc);
}

static void ReleaseRecord(MaterialRecord& record)
{
    BufferAllocator* allocator = BufferAllocator::GetInstance();
    allocator->FreeDescriptorSet(record.Set);
    for (ImageHandle image : record.Images)
    {
        MipGenerator::Cancel(image);
        allocator->DestroyImage(image);
    }
}

void MaterialRegistry::Shutdown()
{
    Materials.ForEachLive([](MaterialRecord& record) { ReleaseRecord(record); });
    Materials.Clear();
    MaterialsByName.clear();
    if (DefaultImage.IsValid())
        BufferAllocator::GetInstance()->DestroyImage(DefaultImage);
    DefaultImage = {};
}

static ImageHandle LoadTexture(const std::vector<std::string>& names, bool isColour)
{
    std::unique_ptr<ImageImport> import;
    if (names.size() == 1)
        import = std::make_unique<ImageImport>("Textures/" + names[0]);
    else if (names.size() == 3)
        import = std::make_unique<ImageImport>(std::vector<std::string>{ "Textures/" + names[0], "Textures/" + names[1], "Textures/" + names[2] },
                                               RHIConstants::DefaultMetalnessRoughnessOcclusion);
    else
        throw std::runtime_error("A texture slot names one image or three, not " + std::to_string(names.size()));

    const ImageData* data = import->GetData();
    ImageDesc desc = RHIConstants::DefaultTextureDesc;
    desc.Width = data->Width;
    desc.Height = data->Height;
    desc.Size = data->TotalSize;
    desc.InitialData = data->Pixels;
    desc.MipLevels = MipGenerator::FullMipCount(desc.Width, desc.Height);
    desc.Usage.Storage = true;

    ImageHandle image = BufferAllocator::GetInstance()->CreateImage(desc);
    MipGenerator::Enqueue(image, isColour);
    return image;
}

static void WriteParameter(MaterialRecord& record, const MaterialField& field, std::span<const float> values)
{
    if (values.size() != field.Components)
        throw std::runtime_error("Material " + record.Name + ": " + field.Name + " takes " + std::to_string(field.Components) + " numbers");
    std::memcpy(record.Parameters.data() + field.Offset, values.data(), values.size() * sizeof(float));
}

static MaterialRecord BuildRecord(const std::string& name, const MaterialDesc& desc)
{
    MaterialRecord record;
    record.Name = name;
    if (!MaterialTypes::Find(desc.Type, record.Type))
        throw std::runtime_error("Material " + name + " has an unknown type: " + desc.Type);
    const MaterialTypeDesc& type = MaterialTypes::Get(record.Type);

    for (const MaterialField& field : type.Fields)
        if (!field.Default.empty())
            WriteParameter(record, field, field.Default);

    std::vector<std::vector<std::string>> textureNames(type.Textures.size());
    for (const auto& [key, words] : desc.Values)
    {
        bool isKnown = false;
        for (const MaterialField& field : type.Fields)
            if (field.Name == key)
            {
                std::vector<float> values;
                for (const std::string& word : words)
                    values.push_back(std::stof(word));
                WriteParameter(record, field, values);
                isKnown = true;
            }
        for (size_t slot = 0; slot < type.Textures.size(); slot++)
            if (type.Textures[slot].Name == key)
            {
                textureNames[slot] = words;
                isKnown = true;
            }
        if (!isKnown)
            throw std::runtime_error("Material " + name + ": " + key + " is not a field or texture of " + type.Name);
    }

    std::vector<DescriptorSetBinding> bindings = { DescriptorSetBinding::ForBuffer(0, BufferAllocator::GetInstance()->GetFrameRing()) };
    for (uint32_t slot = 0; slot < MaterialTypes::MaxTextures; slot++)
    {
        ImageHandle image = DefaultImage;
        if (slot < textureNames.size() && !textureNames[slot].empty())
        {
            image = LoadTexture(textureNames[slot], type.Textures[slot].IsColour);
            record.Images.push_back(image);
        }
        bindings.push_back(DescriptorSetBinding::ForImage(MaterialRegistry::FirstTextureSlot + slot, image));
    }
    record.Set = BufferAllocator::GetInstance()->AllocateDescriptorSet(Renderer::GetSceneLayout()->GetID(), MaterialRegistry::MaterialSetIndex, bindings);
    return record;
}

bool MaterialRegistry::FindFileType(const std::string& name, std::string& type)
{
    MaterialDesc desc;
    if (!MaterialFiles::Read(name, desc))
        return false;
    type = desc.Type;
    return true;
}

MaterialHandle MaterialRegistry::Enlist(const std::string& name)
{
    auto found = MaterialsByName.find(name);
    if (found != MaterialsByName.end())
    {
        Materials.Get(found->second).References++;
        return found->second;
    }

    MaterialDesc desc;
    if (!MaterialFiles::Read(name, desc) && !MaterialFiles::ReadConvention(name, desc))
    {
        std::cout << "Material " << name << ": no material file or textures, so flat grey" << std::endl;
        desc = { .Type = "flat" };
    }

    MaterialRecord record = BuildRecord(name, desc);
    record.References = 1;
    MaterialHandle handle = Materials.Claim(std::move(record));
    MaterialsByName[name] = handle;
    return handle;
}

void MaterialRegistry::Retire(MaterialHandle material)
{
    if (!Materials.IsLive(material))
        return;

    MaterialRecord& record = Materials.Get(material);
    if (--record.References > 0)
        return;

    ReleaseRecord(record);
    MaterialsByName.erase(record.Name);
    Materials.Release(material);
}

bool MaterialRegistry::IsLive(MaterialHandle material)
{
    return Materials.IsLive(material);
}

MaterialTypeID MaterialRegistry::GetType(MaterialHandle material)
{
    return Materials.Get(material).Type;
}

void MaterialRegistry::SetParameter(MaterialHandle material, const std::string& field, std::span<const float> values)
{
    MaterialRecord& record = Materials.Get(material);
    for (const MaterialField& each : MaterialTypes::Get(record.Type).Fields)
        if (each.Name == field)
        {
            WriteParameter(record, each, values);
            record.ParameterFrame = UINT64_MAX;
            return;
        }
    throw std::runtime_error("Material " + record.Name + " has no field " + field);
}

void MaterialRegistry::Bind(PipelineExecutor* executor, MaterialHandle material)
{
    MaterialRecord& record = Materials.Get(material);
    if (record.ParameterFrame != executor->GetFrameNumber())
    {
        FrameAllocation allocation = BufferAllocator::GetInstance()->AllocateFrameData(MaterialTypes::ParameterBlockSize, BufferAllocator::UniformAlignment);
        std::memcpy(allocation.Address, record.Parameters.data(), MaterialTypes::ParameterBlockSize);
        record.ParameterOffset = static_cast<uint32_t>(allocation.Offset);
        record.ParameterFrame = executor->GetFrameNumber();
    }
    uint32_t offsets[] = { record.ParameterOffset };
    executor->BindDescriptorSet(record.Set, offsets);
}
