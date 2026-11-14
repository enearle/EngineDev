#pragma once
#include "Pipeline.h"
#include "../RHI/RHIStructures.h"

namespace DirectX { struct XMFLOAT4; }
class Window;

class RHI_API PipelineExecutor
{
public:
    static PipelineExecutor* Create(Window* window, CoreInitData data);
    virtual ~PipelineExecutor() = default;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void GetSwapChainRenderTargets(void*& outBackBufferView, void*& outBackBuffer) = 0;
    virtual void Wait() = 0;
    virtual uint64_t GetFrameNumber() const = 0;

    virtual void BeginPipeline(Pipeline* pipeline,
                               const std::vector<void*>& colorViews,
                               void* depthView,
                               uint32_t width, uint32_t height, bool isVariant = false, bool isFirstInContext = false) = 0;

    virtual void EndPipeline(bool isLastInContext = false) = 0;

    virtual void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) = 0;
    virtual void IssueImageMemoryBarrier(const std::vector<ImageMemoryBarrier>& barriers) = 0;

    virtual void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexCount, BufferHandle indexBuffer, uint32_t indexCount,
        const void* pushConstants = nullptr, size_t pushConstantsSize = 0, uint32_t vertexStride = sizeof(Vertex)) = 0;
    virtual void DrawFSQuad() = 0;

    virtual void BindDrawDescriptorSets(const std::vector<DescriptorSetHandle>* descriptorSets = nullptr) = 0;
    virtual void BindPipelineDescriptorSets(const std::vector<DescriptorSetHandle>* descriptorSets = nullptr) = 0;

    // The overlay pass draws straight onto the back buffer, keeping its colour and starting from a
    // cleared stencil. Code outside the RHI (UI libraries) records into it with its own native calls.
    virtual void BeginOverlayPass(uint32_t width, uint32_t height) = 0;
    virtual void EndOverlayPass() = 0;

    virtual void ClearBackBuffer(const DirectX::XMFLOAT4& colour, uint32_t width, uint32_t height) = 0;
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
    uint64_t GetFrameNumber() const override;

    void BeginPipeline(Pipeline* pipeline,
                       const std::vector<void*>& colorViews,
                       void* depthView,
                       uint32_t width, uint32_t height, bool isVariant, bool isFirstInContext) override;
    void EndPipeline(bool isLastInContext = false) override;

    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const std::vector<ImageMemoryBarrier>& barriers) override;

    void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexCount, BufferHandle indexBuffer, uint32_t indexCount,
        const void* pushConstants, size_t pushConstantsSize, uint32_t vertexStride) override;
    void DrawFSQuad() override;

    void BindDrawDescriptorSets(const std::vector<DescriptorSetHandle>* descriptorSets) override;
    void BindPipelineDescriptorSets(const std::vector<DescriptorSetHandle>* descriptorSets) override;

    void BeginOverlayPass(uint32_t width, uint32_t height) override;
    void EndOverlayPass() override;

    void ClearBackBuffer(const DirectX::XMFLOAT4& colour, uint32_t width, uint32_t height) override;
    void OverrideViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;

    void ResetWindow() override;

private:
    ID3D12GraphicsCommandList* GetCommandList();
    void BindDescriptorSet(DescriptorSetHandle set);
    D3DPipeline* CurrentPipeline = nullptr;
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
    uint64_t GetFrameNumber() const override;

    void BeginPipeline(Pipeline* pipeline,
                       const std::vector<void*>& colorViews,
                       void* depthView,
                       uint32_t width, uint32_t height, bool isVariant, bool isFirstInContext) override;
    void EndPipeline(bool isLastInContext = false) override;

    void IssueMemoryBarrier(const RHIStructures::MemoryBarrier& barrier) override;
    void IssueImageMemoryBarrier(const std::vector<ImageMemoryBarrier>& barriers) override;

    void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexCount, BufferHandle indexBuffer, uint32_t indexCount,
        const void* pushConstants, size_t pushConstantsSize, uint32_t vertexStride) override;
    void DrawFSQuad() override;

    void BindDrawDescriptorSets(const std::vector<DescriptorSetHandle>* descriptorSets) override;
    void BindPipelineDescriptorSets(const std::vector<DescriptorSetHandle>* descriptorSets) override;

    void BeginOverlayPass(uint32_t width, uint32_t height) override;
    void EndOverlayPass() override;

    void ClearBackBuffer(const DirectX::XMFLOAT4& colour, uint32_t width, uint32_t height) override;
    void OverrideViewport(uint32_t x, uint32_t y, uint32_t w, uint32_t h) override;

    void ResetWindow() override;

private:
    VkCommandBuffer GetCommandBuffer();
    void BindDescriptorSet(DescriptorSetHandle set);
    VulkanPipeline* CurrentPipeline = nullptr;
};
