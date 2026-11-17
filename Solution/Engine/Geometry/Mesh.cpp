#include "Mesh.h"
#include <stdexcept>
#include "RHI/RHIConstants.h"
#include "MeshRegistry.h"

using namespace RHIStructures;

Mesh::Mesh()
{
    VertexCount = 0;
    IndexCount = 0;
    LocalMaterialIndex = 0;
}

Mesh::Mesh(VertexCache* vertexCache, uint32_t LocalMaterialIndex) :
    LocalMaterialIndex(LocalMaterialIndex)
{
    using namespace RHIStructures;

    VertexCount = vertexCache->Vertices.size();
    IndexCount = vertexCache->Indices.size();
    TempVertexCache = vertexCache;
}

Mesh::Mesh(SkinnedVertexCache* skinnedVertexCache, uint32_t LocalMaterialIndex) :
    LocalMaterialIndex(LocalMaterialIndex), bIsSkinned(true)
{
    using namespace RHIStructures;

    VertexCount = skinnedVertexCache->Vertices.size();
    IndexCount = skinnedVertexCache->Indices.size();
    TempSkinnedVertexCache = skinnedVertexCache;
}

Mesh::~Mesh()
{
    WipeCachedData();
}

Mesh::Mesh(Mesh&& other) noexcept
    : bIsSkinned(other.bIsSkinned),
      VertexCount(other.VertexCount),
      IndexCount(other.IndexCount),
      LocalMaterialIndex(other.LocalMaterialIndex),
      MaterialName(std::move(other.MaterialName)),
      Chart(std::move(other.Chart)),
      GPUMesh(other.GPUMesh),
      TempVertexCache(other.TempVertexCache),
      TempSkinnedVertexCache(other.TempSkinnedVertexCache)
{
    other.TempVertexCache = nullptr;
    other.TempSkinnedVertexCache = nullptr;
    other.GPUMesh = {};
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        WipeCachedData();
        bIsSkinned = other.bIsSkinned;
        VertexCount = other.VertexCount;
        IndexCount = other.IndexCount;
        LocalMaterialIndex = other.LocalMaterialIndex;
        MaterialName = std::move(other.MaterialName);
        Chart = std::move(other.Chart);
        GPUMesh = other.GPUMesh;
        TempVertexCache = other.TempVertexCache;
        TempSkinnedVertexCache = other.TempSkinnedVertexCache;

        other.TempVertexCache = nullptr;
        other.TempSkinnedVertexCache = nullptr;
        other.GPUMesh = {};
    }
    return *this;
}

void Mesh::UploadToGPU()
{
    if (GPUMesh.IsValid())
        return;

    if (!bIsSkinned && TempVertexCache)
        GPUMesh = MeshRegistry::Enlist(TempVertexCache->Vertices.data(), VertexCount, VertexLayout::Static,
                                       TempVertexCache->Indices.data(), IndexCount);
    else if (bIsSkinned && TempSkinnedVertexCache)
        GPUMesh = MeshRegistry::Enlist(TempSkinnedVertexCache->Vertices.data(), VertexCount, VertexLayout::Skinned,
                                       TempSkinnedVertexCache->Indices.data(), IndexCount);
}

void Mesh::FreeGPUResources()
{
    MeshRegistry::Retire(GPUMesh);
    GPUMesh = {};
}

void* Mesh::GetCachedIndexData() const
{
    if (!bIsSkinned &&TempVertexCache)
        return TempVertexCache->Indices.data();

    if (bIsSkinned && TempSkinnedVertexCache)
        return TempSkinnedVertexCache->Indices.data();

    throw std::runtime_error("No cached index data available.");
}

void* Mesh::GetCachedVertexData() const
{
    if (!bIsSkinned && TempVertexCache)
        return TempVertexCache->Vertices.data();

    if (bIsSkinned && TempSkinnedVertexCache)
        return TempSkinnedVertexCache->Vertices.data();

    throw std::runtime_error("No cached vertex data available.");
}

void Mesh::WipeCachedData()
{
    delete TempSkinnedVertexCache; TempSkinnedVertexCache = nullptr;
    delete TempVertexCache;        TempVertexCache        = nullptr;
}
