#pragma once
#include <deque>
#include <map>
#include "RHIStructures.h"
#include "../RHI_API_Macro.h"

using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

// A slice of the per-frame ring buffer. Valid until the same frame slot begins again.
struct FrameAllocation
{
    BufferHandle Buffer = {};
    uint64_t Offset = 0;
    void* Address = nullptr;
};

class RHI_API BufferAllocator
{
public:
    // Satisfies both minUniformBufferOffsetAlignment on Vulkan and the 256-byte CBV rule on D3D12.
    static constexpr uint64_t UniformAlignment = 256;

protected:

    HandlePool<ImageAllocation, ImageHandle> Images;
    HandlePool<BufferAllocation, BufferHandle> Buffers;
    HandlePool<DescriptorSetAllocation, DescriptorSetHandle> DescriptorSets;

    enum class PendingKind : uint8_t { Buffer, Image, DescriptorSet, DescriptorSetLayouts };
    struct PendingRelease
    {
        uint64_t FrameNumber = 0;
        PendingKind Kind = PendingKind::Buffer;
        BufferAllocation Buffer = {};
        ImageAllocation Image = {};
        DescriptorSetAllocation DescriptorSet = {};
        uint32_t LayoutID = 0;
    };
    std::deque<PendingRelease> PendingReleases;
    uint64_t FrameNumber = 0;
    uint32_t FramesInFlight = 3;

    BufferHandle FrameRing = {};
    uint8_t* FrameRingAddress = nullptr;
    uint64_t FrameRingBytesPerFrame = 0;
    uint64_t FrameRingCursor = 0;
    uint64_t FrameRingEnd = 0;

    virtual void ReleaseBuffer(BufferAllocation& allocation) = 0;
    virtual void ReleaseImage(ImageAllocation& allocation) = 0;
    virtual void ReleaseDescriptorSet(DescriptorSetAllocation& allocation) = 0;
    virtual void ReleaseDescriptorSetLayouts(uint32_t layoutID) = 0;
    void ReleasePending(PendingRelease& pending);

    static BufferAllocator* Instance;
    BufferAllocator() = default;

public:

    static BufferAllocator* GetInstance();
    virtual ~BufferAllocator() = default;

    static uint64_t MakeKey(uint32_t layoutID, uint32_t setIndex) { return (static_cast<uint64_t>(layoutID) << 32) | setIndex; }

    virtual BufferHandle CreateBuffer(const BufferDesc& bufferDesc) = 0;

    // With InitialData the image is uploaded and left ready for sampling. Without it the image is
    // created empty in the Undefined layout, for use as an attachment.
    virtual ImageHandle CreateImage(const ImageDesc& imageDesc) = 0;

    // Records an image whose GPU object is owned elsewhere, such as a swapchain image.
    ImageHandle EnlistExternalImage(ImageAllocation imageAllocation) { return Images.Claim(imageAllocation); }

    // Drops the record of an image enlisted with EnlistExternalImage without touching its GPU object.
    virtual void EvictImage(ImageHandle image) = 0;

    // The handle goes stale at once. The GPU object is released only when every frame that
    // could still reference it has finished, FramesInFlight frames later.
    void DestroyBuffer(BufferHandle buffer);
    void DestroyImage(ImageHandle image);
    void FreeDescriptorSet(DescriptorSetHandle set);

    void BeginFrame(uint64_t frameNumber, uint32_t framesInFlight);
    void ReleaseAllPending();

    // One host-visible buffer split into one region per frame in flight. Each frame bump-allocates
    // from its own region, which is reset when that frame slot begins again.
    void CreateFrameRing(uint64_t bytesPerFrame);
    FrameAllocation AllocateFrameData(uint64_t size, uint64_t alignment);
    BufferHandle GetFrameRing() const { return FrameRing; }
    uint64_t GetFrameRingSize() const { return FrameRingBytesPerFrame * FramesInFlight; }

    // Sets above maxSetIndex that have no bindings get empty layouts so set numbers stay contiguous.
    virtual void RegisterDescriptorSetLayout(uint32_t layoutID, const ResourceLayout& layout, uint32_t maxSetIndex) = 0;
    void RetireDescriptorSetLayouts(uint32_t layoutID);

    // Dynamic uniform buffers take their offset when the set is bound, so the set is never rewritten
    // per frame. Storage buffers always cover the whole buffer and are indexed by element.
    virtual DescriptorSetHandle AllocateDescriptorSet(uint32_t layoutID, uint32_t setIndex,
                                                      const std::vector<DescriptorSetBinding>& bindings) = 0;
    virtual void UpdateDescriptorSet(DescriptorSetHandle set, const std::vector<DescriptorSetBinding>& newBindings) = 0;

    const ImageAllocation& GetImageAllocation(ImageHandle image) const { return Images.Get(image); }
    const BufferAllocation& GetBufferAllocation(BufferHandle buffer) const { return Buffers.Get(buffer); }
    const DescriptorSetAllocation& GetDescriptorSet(DescriptorSetHandle set) const { return DescriptorSets.Get(set); }
    bool IsImageLive(ImageHandle image) const { return Images.IsLive(image); }
};

class VulkanBufferAllocator : public BufferAllocator
{
public:

    VulkanBufferAllocator();
    ~VulkanBufferAllocator() override;

    BufferHandle CreateBuffer(const BufferDesc& bufferDesc) override;
    ImageHandle CreateImage(const ImageDesc& imageDesc) override;

    void RegisterDescriptorSetLayout(uint32_t layoutID, const ResourceLayout& layout, uint32_t maxSetIndex) override;
    DescriptorSetHandle AllocateDescriptorSet(uint32_t layoutID, uint32_t setIndex,
                                              const std::vector<DescriptorSetBinding>& bindings) override;
    void UpdateDescriptorSet(DescriptorSetHandle set, const std::vector<DescriptorSetBinding>& newBindings) override;
    void EvictImage(ImageHandle image) override;

    VkDescriptorSetLayout GetRegisteredDescriptorSetLayout(uint32_t layoutID, uint32_t setIndex);
    static uint32_t FindMemoryType(uint32_t allowdTypes, VkMemoryPropertyFlags flags);

protected:

    void ReleaseBuffer(BufferAllocation& allocation) override;
    void ReleaseImage(ImageAllocation& allocation) override;
    void ReleaseDescriptorSet(DescriptorSetAllocation& allocation) override;
    void ReleaseDescriptorSetLayouts(uint32_t layoutID) override;

private:

    // Pools are chained: a new one is created when the last is full. A set records its pool in
    // DescriptorSetAllocation::PlatformData.
    struct DescriptorSetLayoutInfo
    {
        VkDescriptorSetLayout Layout = VK_NULL_HANDLE;
        std::vector<VkDescriptorPool> Pools;
        std::vector<VkDescriptorPoolSize> PoolSizes;
        std::vector<DescriptorBinding> Bindings;
    };

    VkCommandPool UploadCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer UploadCommands = VK_NULL_HANDLE;
    VkFence UploadFence = VK_NULL_HANDLE;

    VkCommandBuffer BeginUpload();
    void SubmitUploadAndWait();
    static void CreateStagingBuffer(const void* data, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory);
    std::map<uint64_t, DescriptorSetLayoutInfo> DescriptorSetLayouts;
    static constexpr uint32_t DescriptorSetsPerPool = 256;

    void WriteDescriptors(VkDescriptorSet descriptorSet, const std::vector<DescriptorBinding>& layoutBindings,
                          const std::vector<DescriptorSetBinding>& bindings, bool requireEveryBinding);

    static VkImage CreateVulkanImage(const ImageDesc& imageDesc, VkDeviceMemory* imageMemory);
    static VkImageView CreateVulkanImageView(VkImage image, const ImageDesc& imageDesc);

    void CopyToDeviceLocalBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size);
};

// First-fit allocator of contiguous index ranges, used for D3D12 descriptor heap slots.
class DescriptorRangeAllocator
{
    struct Range
    {
        uint32_t Start = 0;
        uint32_t Count = 0;
    };
    std::vector<Range> FreeRanges;

public:

    void Reset(uint32_t capacity);
    uint32_t Claim(uint32_t count);
    void Release(uint32_t start, uint32_t count);
};

class DirectX12BufferAllocator : public BufferAllocator
{
public:

    DirectX12BufferAllocator();
    ~DirectX12BufferAllocator() override;

    BufferHandle CreateBuffer(const BufferDesc& bufferDesc) override;
    ImageHandle CreateImage(const ImageDesc& imageDesc) override;

    void RegisterDescriptorSetLayout(uint32_t layoutID, const ResourceLayout& layout, uint32_t maxSetIndex) override;
    DescriptorSetHandle AllocateDescriptorSet(uint32_t layoutID, uint32_t setIndex,
                                              const std::vector<DescriptorSetBinding>& bindings) override;
    void UpdateDescriptorSet(DescriptorSetHandle set, const std::vector<DescriptorSetBinding>& newBindings) override;
    void EvictImage(ImageHandle image) override;

    ID3D12DescriptorHeap* GetShaderResourceHeap() const { return ShaderResourceHeap.Get(); }

    // Bindings of one set sorted by slot. Dynamic uniform buffers become root CBVs; everything else
    // sits in one descriptor table at TableOffsets[i] from its start.
    struct SetLayoutInfo
    {
        std::vector<DescriptorBinding> Bindings;
        std::vector<uint32_t> TableOffsets;
        uint32_t TableSize = 0;
        uint32_t DynamicCount = 0;
    };
    static SetLayoutInfo BuildSetLayoutInfo(std::vector<DescriptorBinding> bindings);

    struct DescriptorTableData
    {
        uint32_t RangeStart = 0;
        uint32_t RangeCount = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE BaseHandle = {};
        std::vector<D3D12_GPU_VIRTUAL_ADDRESS> DynamicAddresses;
    };

protected:

    void ReleaseBuffer(BufferAllocation& allocation) override;
    void ReleaseImage(ImageAllocation& allocation) override;
    void ReleaseDescriptorSet(DescriptorSetAllocation& allocation) override;
    void ReleaseDescriptorSetLayouts(uint32_t layoutID) override;

private:

    std::map<uint64_t, SetLayoutInfo> DescriptorSetLayouts;

    ComPtr<ID3D12DescriptorHeap> ShaderResourceHeap;
    ComPtr<ID3D12DescriptorHeap> RenderTargetHeap;
    ComPtr<ID3D12DescriptorHeap> DepthStencilHeap;

    UINT ShaderResourceIncrement = 0;
    UINT RenderTargetIncrement = 0;
    UINT DepthStencilIncrement = 0;

    DescriptorRangeAllocator ShaderResourceSlots;
    DescriptorRangeAllocator RenderTargetSlots;
    DescriptorRangeAllocator DepthStencilSlots;

    static constexpr UINT MaxShaderResourceDescriptors = 16384;
    static constexpr UINT MaxRenderTargetDescriptors = 512;
    static constexpr UINT MaxDepthStencilDescriptors = 256;

    D3D12_CPU_DESCRIPTOR_HANDLE ShaderResourceCPU(uint32_t index) const;
    void WriteDescriptor(const DescriptorBinding& layoutBinding, const DescriptorSetBinding& binding, D3D12_CPU_DESCRIPTOR_HANDLE destination);

    // Uploads record into their own list and wait on a fence that only counts up, so they are safe
    // at any time, including while a frame is being recorded.
    ComPtr<ID3D12CommandAllocator> UploadAllocator;
    ComPtr<ID3D12GraphicsCommandList> UploadList;
    ComPtr<ID3D12Fence> UploadFence;
    UINT64 UploadFenceValue = 0;
    HANDLE UploadEvent = nullptr;

    ID3D12GraphicsCommandList* BeginUpload();
    void SubmitUploadAndWait();
};
