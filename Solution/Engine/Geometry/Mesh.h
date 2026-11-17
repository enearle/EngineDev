#pragma once
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLFW_INCLUDE_VULKAN

#include <DirectXMath.h>
#include <string>
#include <vector>
#include "RHI/RHIStructures.h"
#include "../ENGINE_API_Macro.h"
#include "RenderScene.h"

struct aiScene;
struct DirectX::XMMATRIX;

struct SkinnedVertexCache
{
    std::vector<RHIStructures::SkinnedVertex> Vertices;
    std::vector<uint32_t> Indices;
};

// A mesh's UV chart for procedural surfaces: one UV unit spans MillimetresU and MillimetresV (the
// median over its triangles). A skinned mesh also has a MapSize x MapSize R8G8B8A8 chart map: the
// direction field as a doubled angle (cos, sin) in the chart's millimetre frame, mapped to 0..1;
// local scale against the median as 0.5 + log2(scale) / 4; and coverage, 255 inside an island.
struct MeshChart
{
    static constexpr uint32_t MapSize = 512;
    float MillimetresU = 0.0f;
    float MillimetresV = 0.0f;
    float MillimetresPerUnit = 0.0f;    // Of the mesh's positions
    std::vector<uint8_t> Map;
};

struct VertexCache
{
    std::vector<RHIStructures::Vertex> Vertices;
    std::vector<uint32_t> Indices;
};

class ENGINE_API Mesh
{
public:
    Mesh();
    Mesh(VertexCache* vertexCache, uint32_t LocalMaterialIndex);
    // Skinned vertices are in model space and their bone indices are joints of the model's skeleton.
    Mesh(SkinnedVertexCache* skinnedVertexCache, uint32_t LocalMaterialIndex);
    ~Mesh();

    // Move-only: cache pointers are owned exclusively; copy would double-free.
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    uint32_t GetVertexCount() const                     { return VertexCount; }
    uint32_t GetIndexCount() const                      { return IndexCount; }
    uint32_t GetLocalMaterialIndex() const              { return LocalMaterialIndex; }
    const std::string& GetMaterialName() const          { return MaterialName; }
    void SetMaterialName(const std::string& name)       { MaterialName = name; }
    const MeshChart& GetChart() const                   { return Chart; }
    // Before upload: the skinned vertices and indices, which import steps may rewrite; then
    // RefreshCounts.
    SkinnedVertexCache* GetSkinnedCache()               { return TempSkinnedVertexCache; }
    void RefreshCounts();
    bool HasAuthoredPatch() const                       { return IsPatchAuthored; }
    void SetAuthoredPatch(bool isAuthored)              { IsPatchAuthored = isAuthored; }
    void SetChart(MeshChart chart)                      { Chart = std::move(chart); }
    bool IsSkinned() const                              { return bIsSkinned; }
    MeshHandle GetGPUMesh() const                       { return GPUMesh; }
    void UploadToGPU();
    void FreeGPUResources();
private:

    bool bIsSkinned = false;
    uint32_t VertexCount;
    uint32_t IndexCount;
    uint32_t LocalMaterialIndex;
    std::string MaterialName;
    MeshChart Chart;
    bool IsPatchAuthored = false;

    MeshHandle GPUMesh = {};
    VertexCache* TempVertexCache = nullptr;
    SkinnedVertexCache* TempSkinnedVertexCache = nullptr;

public:
    void* GetCachedIndexData() const;
    void* GetCachedVertexData() const;
    void WipeCachedData();

};