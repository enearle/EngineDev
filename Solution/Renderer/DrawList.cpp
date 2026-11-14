#include "DrawList.h"

#include <algorithm>
#include <stdexcept>
#include "MaterialRegistry.h"
#include "MeshRegistry.h"
#include "Renderer.h"
#include "RHI/BufferAllocator.h"
#include "RHI/PipelineExecutor.h"

using namespace DirectX;

static constexpr uint64_t ObjectIndexMask = (1ull << 20) - 1;

Frustum Frustum::FromViewProjection(const XMFLOAT4X4& viewProjection)
{
    // Row-vector convention (clip = position * M), so planes come from the matrix columns.
    XMMATRIX columns = XMMatrixTranspose(XMLoadFloat4x4(&viewProjection));
    XMVECTOR planes[6] = {
        columns.r[3] + columns.r[0],
        columns.r[3] - columns.r[0],
        columns.r[3] + columns.r[1],
        columns.r[3] - columns.r[1],
        columns.r[2],
        columns.r[3] - columns.r[2],
    };

    Frustum frustum;
    for (int i = 0; i < 6; i++)
        XMStoreFloat4(&frustum.Planes[i], XMPlaneNormalize(planes[i]));
    return frustum;
}

bool Frustum::IntersectsSphere(const XMFLOAT4& sphere) const
{
    for (const XMFLOAT4& plane : Planes)
        if (plane.x * sphere.x + plane.y * sphere.y + plane.z * sphere.z + plane.w < -sphere.w)
            return false;
    return true;
}

void DrawList::Build(std::span<const Frustum> frusta, bool isShadowCastersOnly)
{
    const std::vector<PreparedObject>& objects = Renderer::GetPreparedObjects();
    if (objects.size() > ObjectIndexMask)
        throw std::runtime_error("Too many objects for the draw list sort key");

    SortKeys.clear();
    Batches.clear();
    InstanceCount = 0;

    for (uint32_t i = 0; i < objects.size(); i++)
    {
        const PreparedObject& object = objects[i];
        if (isShadowCastersOnly && !object.CastsShadows)
            continue;

        bool isVisible = frusta.empty();
        for (const Frustum& frustum : frusta)
            if (frustum.IntersectsSphere(object.Sphere))
            {
                isVisible = true;
                break;
            }

        if (isVisible)
            SortKeys.push_back(object.BatchKey | i);
    }

    if (SortKeys.empty())
        return;

    std::sort(SortKeys.begin(), SortKeys.end());

    FrameAllocation allocation = BufferAllocator::GetInstance()->AllocateFrameData(SortKeys.size() * sizeof(InstanceData), sizeof(InstanceData));
    InstanceData* instances = static_cast<InstanceData*>(allocation.Address);
    uint32_t firstInstance = static_cast<uint32_t>(allocation.Offset / sizeof(InstanceData));

    for (size_t i = 0; i < SortKeys.size(); i++)
    {
        const PreparedObject& object = objects[SortKeys[i] & ObjectIndexMask];
        instances[i] = object.Instance;

        bool isSameBatch = i > 0 && (SortKeys[i] & ~ObjectIndexMask) == (SortKeys[i - 1] & ~ObjectIndexMask);
        if (isSameBatch)
            Batches.back().InstanceCount++;
        else
            Batches.push_back({ object.Layout, object.Material, object.Mesh, firstInstance + static_cast<uint32_t>(i), 1 });
    }
    InstanceCount = static_cast<uint32_t>(SortKeys.size());
}

void DrawList::Record(PipelineExecutor* executor, std::span<Pipeline* const> pipelines, bool isMaterialBound, const std::string& passName) const
{
    Pipeline* boundPipeline = nullptr;
    MaterialHandle boundMaterial = {};
    uint32_t drawCalls = 0;

    for (const DrawBatch& batch : Batches)
    {
        Pipeline* pipeline = pipelines[static_cast<size_t>(batch.Layout)];
        if (!pipeline || !MeshRegistry::IsLive(batch.Mesh))
            continue;
        if (isMaterialBound && !MaterialRegistry::IsLive(batch.Material))
            continue;

        if (pipeline != boundPipeline)
        {
            executor->BindPipeline(pipeline);
            Renderer::BindSceneSet(executor);
            boundPipeline = pipeline;
            boundMaterial = {};
        }

        if (isMaterialBound && batch.Material != boundMaterial)
        {
            executor->BindDescriptorSet(MaterialRegistry::GetDescriptorSet(batch.Material));
            boundMaterial = batch.Material;
        }

        DrawConstants constants = { batch.FirstInstance, { 0, 0, 0 } };
        executor->PushConstants(&constants, sizeof(constants));

        const MeshRecord& mesh = MeshRegistry::Get(batch.Mesh);
        executor->DrawIndexed(mesh.VertexBuffer, mesh.VertexStride, mesh.IndexBuffer, mesh.IndexCount, batch.InstanceCount);
        drawCalls++;
    }

    Renderer::ReportPassStats(passName, InstanceCount, static_cast<uint32_t>(Batches.size()), drawCalls);
}
