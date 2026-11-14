#pragma once
#include <span>
#include "Pipeline.h"
#include "RHIStructures.h"

class Window;

// Records one frame of commands. Images are addressed by ImageHandle throughout, including the
// swapchain image, which GetBackBuffer returns for the frame being recorded.
class RHI_API PipelineExecutor
{
protected:
    std::vector<ImageHandle> BackBuffers;
    Format BackBufferFormat = Format::Unknown;
    Pipeline* CurrentPipeline = nullptr;
    BufferHandle BoundVertexBuffer = {};
    BufferHandle BoundIndexBuffer = {};

public:
    // Drops the swapchain image records. Call before the BufferAllocator is destroyed.
    void EvictBackBuffers();

    static PipelineExecutor* Create(Window* window, CoreInitData data);
    virtual ~PipelineExecutor() = default;

    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Wait() = 0;
    virtual uint64_t GetFrameNumber() const = 0;
    virtual ImageHandle GetBackBuffer() const = 0;
    Format GetBackBufferFormat() const { return BackBufferFormat; }

    virtual void TransitionImages(std::span<const ImageTransition> transitions) = 0;

    virtual void BeginRendering(const RenderingDesc& desc) = 0;
    virtual void EndRendering() = 0;

    virtual void BindPipeline(Pipeline* pipeline) = 0;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

    // Dynamic offsets are in binding-slot order, one per dynamic uniform buffer in the set.
    virtual void BindDescriptorSet(DescriptorSetHandle set, std::span<const uint32_t> dynamicOffsets = {}) = 0;
    virtual void PushConstants(const void* data, uint32_t size) = 0;

    virtual void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexStride, BufferHandle indexBuffer,
                             uint32_t indexCount, uint32_t instanceCount) = 0;
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount) = 0;

    // The overlay pass draws straight onto the back buffer, keeping its colour and starting from a
    // cleared stencil. Code outside the RHI (UI libraries) records into it with its own native calls.
    // The back buffer must be in the ColorAttachment layout.
    virtual void BeginOverlayPass(uint32_t width, uint32_t height) = 0;
    virtual void EndOverlayPass() = 0;

    virtual void ResetWindow() = 0;
};

class D3DPipelineExecutor : public PipelineExecutor
{
public:
    D3DPipelineExecutor(Window* window, CoreInitData data);
    ~D3DPipelineExecutor() override;

    void BeginFrame() override;
    void EndFrame() override;
    void Wait() override;
    uint64_t GetFrameNumber() const override;
    ImageHandle GetBackBuffer() const override;

    void TransitionImages(std::span<const ImageTransition> transitions) override;
    void BeginRendering(const RenderingDesc& desc) override;
    void EndRendering() override;
    void BindPipeline(Pipeline* pipeline) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void BindDescriptorSet(DescriptorSetHandle set, std::span<const uint32_t> dynamicOffsets) override;
    void PushConstants(const void* data, uint32_t size) override;
    void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexStride, BufferHandle indexBuffer,
                     uint32_t indexCount, uint32_t instanceCount) override;
    void Draw(uint32_t vertexCount, uint32_t instanceCount) override;
    void BeginOverlayPass(uint32_t width, uint32_t height) override;
    void EndOverlayPass() override;
    void ResetWindow() override;

private:
    ID3D12GraphicsCommandList* GetCommandList();
    void EnlistBackBuffers();
};

class VulkanPipelineExecutor : public PipelineExecutor
{
public:
    VulkanPipelineExecutor(Window* window, CoreInitData data);
    ~VulkanPipelineExecutor() override;

    void BeginFrame() override;
    void EndFrame() override;
    void Wait() override;
    uint64_t GetFrameNumber() const override;
    ImageHandle GetBackBuffer() const override;

    void TransitionImages(std::span<const ImageTransition> transitions) override;
    void BeginRendering(const RenderingDesc& desc) override;
    void EndRendering() override;
    void BindPipeline(Pipeline* pipeline) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void BindDescriptorSet(DescriptorSetHandle set, std::span<const uint32_t> dynamicOffsets) override;
    void PushConstants(const void* data, uint32_t size) override;
    void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexStride, BufferHandle indexBuffer,
                     uint32_t indexCount, uint32_t instanceCount) override;
    void Draw(uint32_t vertexCount, uint32_t instanceCount) override;
    void BeginOverlayPass(uint32_t width, uint32_t height) override;
    void EndOverlayPass() override;
    void ResetWindow() override;

private:
    VkCommandBuffer GetCommandBuffer();
    void EnlistBackBuffers();
};
