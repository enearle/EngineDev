#pragma once
#include "Pipeline.h"
#include "../RHI/RHIStructures.h"
#include "Geometry/Mesh.h"

namespace DirectX { struct XMFLOAT4; }
class RenderPassExecutor
{
public:
    static RenderPassExecutor* Create();
    
    virtual ~RenderPassExecutor() = default;
    
    // Begin a rendering operation with a pipeline and its attachments
    virtual void Begin(Pipeline* pipeline,
                      const std::vector<void*>& colorViews,
                      void* depthView,
                      uint32_t width, uint32_t height,
                      const std::vector<DirectX::XMFLOAT4>& clearColors,
                      float clearDepth) = 0;
    
    // End current rendering operation
    virtual void End() = 0;

    virtual void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) = 0;
    virtual void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) = 0;
    
    virtual void DrawSceneNode(const SceneNode& node, std::vector<uint64_t>& perItemDrawSets, const DirectX::XMFLOAT4X4& camera) = 0;
    virtual void DrawQuad(std::vector<uint64_t>* descriptorSets = nullptr) = 0;
};

class D3DRenderPassExecutor : public RenderPassExecutor
{
public:
    D3DRenderPassExecutor();
    ~D3DRenderPassExecutor() override;
    
    void Begin(Pipeline* pipeline,
               const std::vector<void*>& colorViews,
               void* depthView,
               uint32_t width, uint32_t height,
               const std::vector<DirectX::XMFLOAT4>& clearColors,
               float clearDepth) override;
    void End() override;
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) override;
    void DrawSceneNode(const SceneNode& node, std::vector<uint64_t>& perItemDrawSets, const DirectX::XMFLOAT4X4& camera) override;
    void DrawQuad(std::vector<uint64_t>* descriptorSets = nullptr) override;
    
private:
    ID3D12GraphicsCommandList* GetCommandList();
    D3DPipeline* CurrentPipeline;
};

class VulkanRenderPassExecutor : public RenderPassExecutor
{
public:
    VulkanRenderPassExecutor();
    ~VulkanRenderPassExecutor() override;
    
    void Begin(Pipeline* pipeline,
               const std::vector<void*>& colorViews,
               void* depthView,
               uint32_t width, uint32_t height,
               const std::vector<DirectX::XMFLOAT4>& clearColors,
               float clearDepth) override;
    void End() override;
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) override;
    void DrawSceneNode(const SceneNode& node, std::vector<uint64_t>& perItemDrawSets, const DirectX::XMFLOAT4X4& camera) override;
    void DrawQuad(std::vector<uint64_t>* descriptorSets = nullptr) override;
    void BindDescriptorSets(std::vector<uint64_t>* descriptorSets);

private:
    
    VulkanPipeline* CurrentPipeline;
    VkCommandBuffer GetCommandBuffer();
};
