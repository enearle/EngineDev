#include "MaterialRegistry.h"

#include <unordered_map>
#include "Material.h"
#include "MipGenerator.h"
#include "Renderer.h"
#include "RHI/BufferAllocator.h"
#include "RHI/PipelineLayout.h"

struct MaterialRecord
{
    std::string Name;
    LoadedMaterial Loaded;
    uint32_t References = 0;
};

static HandlePool<MaterialRecord, MaterialHandle> Materials;
static std::unordered_map<std::string, MaterialHandle> MaterialsByName;

MaterialHandle MaterialRegistry::Enlist(const std::string& name)
{
    auto found = MaterialsByName.find(name);
    if (found != MaterialsByName.end())
    {
        Materials.Get(found->second).References++;
        return found->second;
    }

    MaterialRecord record;
    record.Name = name;
    record.Loaded = Material(name, Material::PBR).LoadMaterial(Renderer::GetSceneLayout()->GetID(), MaterialSetIndex);
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

    BufferAllocator* allocator = BufferAllocator::GetInstance();
    allocator->FreeDescriptorSet(record.Loaded.Set);
    for (ImageHandle image : record.Loaded.Images)
    {
        MipGenerator::Cancel(image);
        allocator->DestroyImage(image);
    }

    MaterialsByName.erase(record.Name);
    Materials.Release(material);
}

bool MaterialRegistry::IsLive(MaterialHandle material)
{
    return Materials.IsLive(material);
}

RHIStructures::DescriptorSetHandle MaterialRegistry::GetDescriptorSet(MaterialHandle material)
{
    return Materials.Get(material).Loaded.Set;
}
