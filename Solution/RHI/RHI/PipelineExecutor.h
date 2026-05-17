#pragma once
#include "Pipeline.h"
#include "../RHI/RHIStructures.h"

namespace DirectX { struct XMFLOAT4; }
class Window;

class PipelineExecutor
{
public:
    static PipelineExecutor* Create(Window* window, CoreInitData data);
    virtual ~PipelineExecutor() = default;
    
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void GetSwapChainRenderTargets(void*& outBackBufferView, void*& outBackBuffer) = 0;
    virtual void Wait() = 0;
    
    virtual void BeginPipeline(Pipeline* pipeline,
                               const std::vector<void*>& colorViews,
                               void* depthView,
                               uint32_t width, uint32_t height, bool isVariant = false, bool isFirstInContext = false) = 0;
    
    virtual void EndPipeline(bool isLastInContext = false) = 0;
    
    virtual void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) = 0;
    virtual void IssueImageMemoryBarrier(const std::vector<ImageMemoryBarrier>& barriers) = 0;

    virtual void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount, 
        void* pushConstants = nullptr, size_t pushConstantsSize = 0, uint32_t vertexStride = sizeof(Vertex)) = 0;
    virtual void DrawFSQuad() = 0;
    
    virtual void BindDrawDescriptorSets(std::vector<uint64_t>* descriptorSets = nullptr, uint32_t numPipelineSets = 0) = 0;
    virtual void BindPipelineDescriptorSets(std::vector<uint64_t>* descriptorSets = nullptr) = 0;
    
    virtual void StartNoesisContext(uint32_t width, uint32_t height) = 0;
    virtual void EndNoesisContext() = 0;

    virtual void OverrideViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) = 0;

    virtual void ResetWindow() = 0;
};

class D3DPipelineExecutor : public PipelineExecutor
{
public:
    D3DPipelineExecutor(Window* window, CoreInitData data);
    ~D3DPipelineExecutor() override;
    
    void StartRender(Window* window, CoreInitData data);
    void EndRender();
    void BeginFrame() override;
    void EndFrame() override;
    void GetSwapChainRenderTargets(void*& outBackBufferView, void*& outBackBuffer) override;
    void Wait() override;
    
    void BeginPipeline(Pipeline* pipeline,
                       const std::vector<void*>& colorViews,
                       void* depthView,
                       uint32_t width, uint32_t height, bool isVariant, bool isFirstInContext) override;
    void EndPipeline(bool isLastInContext = false) override;
    
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const std::vector<ImageMemoryBarrier>& barriers) override;

    void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount,
        void* pushConstant, size_t pushConstantSize, uint32_t vertexStride) override;
    void DrawFSQuad() override;

    void BindDrawDescriptorSets(std::vector<uint64_t>* descriptorSets, uint32_t numPipelineSets) override;
    void BindPipelineDescriptorSets(std::vector<uint64_t>* descriptorSets) override;

    void StartNoesisContext(uint32_t width, uint32_t height) override;
    void EndNoesisContext() override;

    void OverrideViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;

    void ResetWindow() override;

private:
    ID3D12GraphicsCommandList* GetCommandList();
    D3DPipeline* CurrentPipeline;
};

class VulkanPipelineExecutor : public PipelineExecutor
{
public:
    VulkanPipelineExecutor(Window* window, CoreInitData data);
    ~VulkanPipelineExecutor() override;
    
    void StartRender(Window* window, CoreInitData data);
    void EndRender();
    void BeginFrame() override;
    void EndFrame() override;
    void GetSwapChainRenderTargets(void*& outBackBufferView, void*& outBackBuffer) override;
    void Wait() override;
    
    void BeginPipeline(Pipeline* pipeline,
                       const std::vector<void*>& colorViews,
                       void* depthView,
                       uint32_t width, uint32_t height, bool isVariant, bool isFirstInContext) override;
    void EndPipeline(bool isLastInContext = false) override;
    
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const std::vector<ImageMemoryBarrier>& barriers) override;

    void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount,
        void* pushConstant, size_t pushConstantSize, uint32_t vertexStride) override;
    void DrawFSQuad() override;

    void BindDrawDescriptorSets(std::vector<uint64_t>* descriptorSets, uint32_t numPipelineSets) override;
    void BindPipelineDescriptorSets(std::vector<uint64_t>* descriptorSets) override;

    void StartNoesisContext(uint32_t width, uint32_t height) override;
    void EndNoesisContext() override;

    void OverrideViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;

    void ResetWindow() override;

private:
    VulkanPipeline* CurrentPipeline;
    VkCommandBuffer GetCommandBuffer();
};
