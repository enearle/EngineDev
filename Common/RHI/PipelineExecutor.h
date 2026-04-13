#pragma once
#include "Pipeline.h"
#include "../RHI/RHIStructures.h"
#include "Geometry/Mesh.h"

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
    
    // Begin a rendering operation with a pipeline and its attachments
    virtual void BeginPipeline(Pipeline* pipeline,
                      const std::vector<void*>& colorViews,
                      void* depthView,
                      uint32_t width, uint32_t height) = 0;
    
    // End current rendering operation
    virtual void EndPipeline() = 0;

    virtual void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) = 0;
    virtual void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) = 0;

    virtual void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount, 
        void* pushConstants = nullptr, size_t pushConstantsSize = 0) = 0;
    virtual void DrawFSQuad() = 0;
    
    virtual void BindDrawDescriptorSets(std::vector<uint64_t>* descriptorSets = nullptr, uint32_t numPipelineSets = 0) = 0;
    virtual void BindPipelineDescriptorSets(std::vector<uint64_t>* descriptorSets = nullptr) = 0;
    
    virtual void StartNoesisContext(uint32_t width, uint32_t height) = 0;
    virtual void EndNoesisContext() = 0;
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
               uint32_t width, uint32_t height) override;
    void EndPipeline() override;
    
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) override;

    void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount, 
        void* pushConstant, size_t pushConstantSize) override;
    void DrawFSQuad() override;
    
    void BindDrawDescriptorSets(std::vector<uint64_t>* descriptorSets, uint32_t numPipelineSets) override;
    void BindPipelineDescriptorSets(std::vector<uint64_t>* descriptorSets) override;
    
    void StartNoesisContext(uint32_t width, uint32_t height) override;
    void EndNoesisContext() override;
    
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
               uint32_t width, uint32_t height) override;
    void EndPipeline() override;
    
    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const ImageMemoryBarrier& barrier) override;

    void DrawIndexed(uint64_t vertBufferID, uint32_t vertCount, uint64_t indexBufferID, uint32_t indexCount, 
        void* pushConstant, size_t pushConstantSize) override;
    void DrawFSQuad() override;
    
    void BindDrawDescriptorSets(std::vector<uint64_t>* descriptorSets, uint32_t numPipelineSets) override;
    void BindPipelineDescriptorSets(std::vector<uint64_t>* descriptorSets) override;
    
    void StartNoesisContext(uint32_t width, uint32_t height) override;
    void EndNoesisContext() override;
    
private:
    
    VulkanPipeline* CurrentPipeline;
    VkCommandBuffer GetCommandBuffer();
};
