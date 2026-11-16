#pragma once
#include "RENDERER_API_Macro.h"
#include "PassGraph.h"
#include "RenderScene.h"

// Skins characters on the GPU once per frame. Each enlisted skinned mesh owns a vertex range in one
// pool buffer of static Vertex layout; the skinning pass writes the range from the mesh's source
// vertices and a palette, and the shadow and G-buffer passes draw it with the static pipelines.
// One per program.
//
// The pool is rewritten every frame, so growing it creates a larger buffer without copying. One pool
// serves every frame in flight because the GPU runs frames in submission order.
class RENDERER_API Skinning
{
public:
    // D3D12 buffer views hold at most 2^27 elements, and the pool is viewed as 4-byte words of
    // 56-byte vertices.
    static constexpr uint32_t MaxPoolVertices = static_cast<uint32_t>((1ull << 29) / 56);

    static void Start();
    static void Shutdown();

    // The source mesh must have the Skinned vertex layout. Throws when the pool cannot hold it.
    static SkinnedMeshHandle Enlist(MeshHandle sourceMesh);
    static void Retire(SkinnedMeshHandle mesh);
    static bool IsLive(SkinnedMeshHandle mesh);
    static MeshHandle GetSourceMesh(SkinnedMeshHandle mesh);
    static uint32_t GetBaseVertex(SkinnedMeshHandle mesh);

    // Skins the mesh this frame with the palette starting at paletteIndex (in matrices) in the frame ring.
    static void Request(SkinnedMeshHandle mesh, uint32_t paletteIndex);

    static GraphBufferID GetPool();
    static uint32_t GetSkinnedVertexCount();    // In the last frame recorded
};
