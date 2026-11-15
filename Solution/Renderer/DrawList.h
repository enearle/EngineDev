#pragma once
#include <span>
#include <string>
#include <vector>
#include "RENDERER_API_Macro.h"
#include "RenderScene.h"

class Pipeline;
class PipelineExecutor;

// One scene object made ready for drawing this frame: its instance data and world bounding sphere.
struct PreparedObject
{
    InstanceData Instance;
    DirectX::XMFLOAT4 Sphere;           // xyz centre, w radius, in world space
    uint64_t BatchKey = 0;              // Objects with equal keys draw in one instanced call
    MeshHandle Mesh = {};
    MaterialHandle Material = {};
    SkinnedMeshHandle SkinnedMesh = {};
    bool CastsShadows = true;
};

struct Frustum
{
    DirectX::XMFLOAT4 Planes[6];

    static Frustum FromViewProjection(const DirectX::XMFLOAT4X4& viewProjection);
    bool IntersectsSphere(const DirectX::XMFLOAT4& sphere) const;
};

// A skinned mesh draws alone, from its range of the skinning pool.
struct DrawBatch
{
    MaterialHandle Material = {};
    MeshHandle Mesh = {};
    SkinnedMeshHandle SkinnedMesh = {};
    uint32_t FirstInstance = 0;
    uint32_t InstanceCount = 0;
};

// The draws of one view, rebuilt every frame: cull, sort by material and mesh, merge equal
// neighbours into instanced draws, and write their instance data to the frame ring.
class RENDERER_API DrawList
{
    std::vector<uint64_t> SortKeys;
    std::vector<DrawBatch> Batches;
    uint32_t InstanceCount = 0;

public:
    // An object is kept when it intersects any of the frusta. No frusta keeps everything.
    void Build(std::span<const Frustum> frusta, bool isShadowCastersOnly);

    void Record(PipelineExecutor* executor, Pipeline* pipeline, bool isMaterialBound, const std::string& passName,
                uint32_t viewIndex = 0) const;

    const std::vector<DrawBatch>& GetBatches() const { return Batches; }
    uint32_t GetInstanceCount() const { return InstanceCount; }
};
