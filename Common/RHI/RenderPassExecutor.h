#pragma once
#include "Pipeline.h"
#include "../RHI/RHIStructures.h"

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
    
    // Bind a pipeline for the current pass
    virtual void BindPipeline(Pipeline* pipeline) = 0;
    
    // Insert barriers between passes
    virtual void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) = 0;
    virtual void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) = 0;
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
    void BindPipeline(Pipeline* pipeline) override;
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) override;
    
    
private:
    ID3D12GraphicsCommandList* GetCommandList();
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
    void BindPipeline(Pipeline* pipeline) override;
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) override;
    void InvalidateFramebuffers();

private:
    std::vector<VkFramebuffer> Framebuffers;
    VkCommandBuffer GetCommandBuffer();
};
