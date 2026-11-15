#pragma once
#include <span>
#include <string>
#include "Pipeline.h"
#include "RHIStructures.h"

class Window;

// Records one frame of commands. Images are addressed by ImageHandle throughout, including the
// swapchain image, which GetBackBuffer returns for the frame being recorded.
struct TimerResult
{
    std::string Name;
    double Milliseconds = 0.0;
};

class RHI_API PipelineExecutor
{
protected:
    static constexpr uint32_t MaxTimersPerFrame = 64;

    // One set of timestamp queries per frame slot. Each slot's results are read when the slot begins
    // again, so they describe the frame recorded FramesInFlight frames earlier.
    struct TimerSlot
    {
        std::vector<std::string> Names;
    };
    std::vector<TimerSlot> TimerSlots;
    std::vector<TimerResult> CompletedTimers;

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

    // False when no frame can be recorded now (the swapchain was recreated); skip the frame.
    virtual bool BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void Wait() = 0;
    virtual uint64_t GetFrameNumber() const = 0;
    // The newest frame the GPU has finished, without waiting.
    virtual uint64_t GetCompletedFrameNumber() const = 0;

    // Outside rendering scopes only. Returns the timer's index within the frame.
    virtual uint32_t BeginTimer(const std::string& name) = 0;
    virtual void EndTimer(uint32_t timer) = 0;
    const std::vector<TimerResult>& GetCompletedTimers() const { return CompletedTimers; }
    virtual ImageHandle GetBackBuffer() const = 0;
    Format GetBackBufferFormat() const { return BackBufferFormat; }

    virtual void TransitionImages(std::span<const ImageTransition> transitions) = 0;
    virtual void TransitionBuffers(std::span<const BufferTransition> transitions) = 0;

    // The image must be in TransferDst (to the image) or TransferSrc (from it). Row pitch comes from
    // CopyRowPitch and buffer offsets are multiples of CopyPlacementAlignment.
    virtual void CopyBufferToImage(BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch, ImageHandle image, const ImageRegion& region) = 0;
    virtual void CopyImageToBuffer(ImageHandle image, const ImageRegion& region, BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch) = 0;
    // The source must be in CopySource and the destination in CopyDestination.
    virtual void CopyBuffer(BufferHandle source, uint64_t sourceOffset, BufferHandle destination, uint64_t destinationOffset, uint64_t size) = 0;

    virtual void BeginRendering(const RenderingDesc& desc) = 0;
    virtual void EndRendering() = 0;

    virtual void BindPipeline(Pipeline* pipeline) = 0;
    virtual void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) = 0;

    // Dynamic offsets are in binding-slot order, one per dynamic uniform buffer in the set.
    virtual void BindDescriptorSet(DescriptorSetHandle set, std::span<const uint32_t> dynamicOffsets = {}) = 0;
    virtual void PushConstants(const void* data, uint32_t size) = 0;

    // baseVertex is added to every index before the vertex is fetched.
    virtual void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexStride, BufferHandle indexBuffer,
                             uint32_t indexCount, uint32_t instanceCount, uint32_t baseVertex = 0) = 0;
    virtual void Draw(uint32_t vertexCount, uint32_t instanceCount) = 0;
    // Outside rendering scopes, with a compute pipeline bound.
    virtual void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) = 0;

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

    bool BeginFrame() override;
    void EndFrame() override;
    void Wait() override;
    uint64_t GetFrameNumber() const override;
    uint64_t GetCompletedFrameNumber() const override;
    uint32_t BeginTimer(const std::string& name) override;
    void EndTimer(uint32_t timer) override;
    ImageHandle GetBackBuffer() const override;

    void TransitionImages(std::span<const ImageTransition> transitions) override;
    void TransitionBuffers(std::span<const BufferTransition> transitions) override;
    void CopyBufferToImage(BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch, ImageHandle image, const ImageRegion& region) override;
    void CopyImageToBuffer(ImageHandle image, const ImageRegion& region, BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch) override;
    void CopyBuffer(BufferHandle source, uint64_t sourceOffset, BufferHandle destination, uint64_t destinationOffset, uint64_t size) override;
    void BeginRendering(const RenderingDesc& desc) override;
    void EndRendering() override;
    void BindPipeline(Pipeline* pipeline) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void BindDescriptorSet(DescriptorSetHandle set, std::span<const uint32_t> dynamicOffsets) override;
    void PushConstants(const void* data, uint32_t size) override;
    void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexStride, BufferHandle indexBuffer,
                     uint32_t indexCount, uint32_t instanceCount, uint32_t baseVertex) override;
    void Draw(uint32_t vertexCount, uint32_t instanceCount) override;
    void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
    void BeginOverlayPass(uint32_t width, uint32_t height) override;
    void EndOverlayPass() override;
    void ResetWindow() override;

private:
    ComPtr<ID3D12QueryHeap> TimestampHeap;
    ComPtr<ID3D12Resource> TimestampReadback;
    double TimestampTicksPerMillisecond = 1.0;

    ID3D12GraphicsCommandList* GetCommandList();
    void EnlistBackBuffers();
};

class VulkanPipelineExecutor : public PipelineExecutor
{
public:
    VulkanPipelineExecutor(Window* window, CoreInitData data);
    ~VulkanPipelineExecutor() override;

    bool BeginFrame() override;
    void EndFrame() override;
    void Wait() override;
    uint64_t GetFrameNumber() const override;
    uint64_t GetCompletedFrameNumber() const override;
    uint32_t BeginTimer(const std::string& name) override;
    void EndTimer(uint32_t timer) override;
    ImageHandle GetBackBuffer() const override;

    void TransitionImages(std::span<const ImageTransition> transitions) override;
    void TransitionBuffers(std::span<const BufferTransition> transitions) override;
    void CopyBufferToImage(BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch, ImageHandle image, const ImageRegion& region) override;
    void CopyImageToBuffer(ImageHandle image, const ImageRegion& region, BufferHandle buffer, uint64_t bufferOffset, uint32_t rowPitch) override;
    void CopyBuffer(BufferHandle source, uint64_t sourceOffset, BufferHandle destination, uint64_t destinationOffset, uint64_t size) override;
    void BeginRendering(const RenderingDesc& desc) override;
    void EndRendering() override;
    void BindPipeline(Pipeline* pipeline) override;
    void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) override;
    void BindDescriptorSet(DescriptorSetHandle set, std::span<const uint32_t> dynamicOffsets) override;
    void PushConstants(const void* data, uint32_t size) override;
    void DrawIndexed(BufferHandle vertexBuffer, uint32_t vertexStride, BufferHandle indexBuffer,
                     uint32_t indexCount, uint32_t instanceCount, uint32_t baseVertex) override;
    void Draw(uint32_t vertexCount, uint32_t instanceCount) override;
    void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
    void BeginOverlayPass(uint32_t width, uint32_t height) override;
    void EndOverlayPass() override;
    void ResetWindow() override;

private:
    VkQueryPool TimestampPool = VK_NULL_HANDLE;
    double MillisecondsPerTick = 0.0;

    VkCommandBuffer GetCommandBuffer();
    void EnlistBackBuffers();
};
