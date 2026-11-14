#pragma once
#include <DirectXMath.h>
#include "RENDERER_API_Macro.h"
#include "RenderScene.h"

struct MeshRecord
{
    RHIStructures::BufferHandle VertexBuffer = {};
    RHIStructures::BufferHandle IndexBuffer = {};
    uint32_t VertexStride = 0;
    uint32_t IndexCount = 0;
    VertexLayout Layout = VertexLayout::Static;
    DirectX::XMFLOAT3 BoundsCenter = {};
    float BoundsRadius = 0.0f;
};

// GPU copies of mesh geometry. One per program. Vertices start with a float3 position, which is
// read to compute the bounding sphere.
class RENDERER_API MeshRegistry
{
public:
    static MeshHandle Enlist(const void* vertices, uint32_t vertexCount, VertexLayout layout,
                             const uint32_t* indices, uint32_t indexCount);
    static void Retire(MeshHandle mesh);
    static bool IsLive(MeshHandle mesh);
    static const MeshRecord& Get(MeshHandle mesh);
    static uint32_t GetVertexStride(VertexLayout layout);
};
