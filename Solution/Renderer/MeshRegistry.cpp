#include "MeshRegistry.h"

#include <cmath>
#include "RHI/BufferAllocator.h"
#include "RHI/RHIConstants.h"

using namespace DirectX;

static HandlePool<MeshRecord, MeshHandle> Meshes;

uint32_t MeshRegistry::GetVertexStride(VertexLayout layout)
{
    return layout == VertexLayout::Skinned ? sizeof(RHIStructures::SkinnedVertex) : sizeof(RHIStructures::Vertex);
}

MeshHandle MeshRegistry::Enlist(const void* vertices, uint32_t vertexCount, VertexLayout layout,
                                const uint32_t* indices, uint32_t indexCount)
{
    if (vertexCount == 0 || indexCount == 0)
        return {};

    MeshRecord record;
    record.Layout = layout;
    record.VertexStride = GetVertexStride(layout);
    record.IndexCount = indexCount;
    record.VertexCount = vertexCount;

    const uint8_t* vertexBytes = static_cast<const uint8_t*>(vertices);
    XMVECTOR minimum = XMVectorReplicate(FLT_MAX);
    XMVECTOR maximum = XMVectorReplicate(-FLT_MAX);
    for (uint32_t i = 0; i < vertexCount; i++)
    {
        XMVECTOR position = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(vertexBytes + i * record.VertexStride));
        minimum = XMVectorMin(minimum, position);
        maximum = XMVectorMax(maximum, position);
    }
    XMVECTOR center = (minimum + maximum) * 0.5f;
    XMStoreFloat3(&record.BoundsCenter, center);

    float radiusSquared = 0.0f;
    for (uint32_t i = 0; i < vertexCount; i++)
    {
        XMVECTOR position = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(vertexBytes + i * record.VertexStride));
        radiusSquared = (std::max)(radiusSquared, XMVectorGetX(XMVector3LengthSq(position - center)));
    }
    record.BoundsRadius = std::sqrt(radiusSquared);

    BufferAllocator* allocator = BufferAllocator::GetInstance();

    BufferDesc vertexDesc = RHIConstants::DefaultVertexBufferDesc;
    vertexDesc.Size = static_cast<uint64_t>(vertexCount) * record.VertexStride;
    vertexDesc.InitialData = vertices;
    vertexDesc.Usage.Storage = layout == VertexLayout::Skinned;
    record.VertexBuffer = allocator->CreateBuffer(vertexDesc);

    BufferDesc indexDesc = RHIConstants::DefaultIndexBufferDesc;
    indexDesc.Size = static_cast<uint64_t>(indexCount) * sizeof(uint32_t);
    indexDesc.InitialData = indices;
    record.IndexBuffer = allocator->CreateBuffer(indexDesc);

    return Meshes.Claim(record);
}

void MeshRegistry::Retire(MeshHandle mesh)
{
    if (!Meshes.IsLive(mesh))
        return;

    const MeshRecord& record = Meshes.Get(mesh);
    BufferAllocator::GetInstance()->DestroyBuffer(record.VertexBuffer);
    BufferAllocator::GetInstance()->DestroyBuffer(record.IndexBuffer);
    Meshes.Release(mesh);
}

bool MeshRegistry::IsLive(MeshHandle mesh)
{
    return Meshes.IsLive(mesh);
}

const MeshRecord& MeshRegistry::Get(MeshHandle mesh)
{
    return Meshes.Get(mesh);
}
