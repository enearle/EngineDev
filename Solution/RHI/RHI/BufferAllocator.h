#pragma once
#include <deque>
#include <map>
#include "RHIStructures.h"
#include "../RHI_API_Macro.h"

using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

class RHI_API BufferAllocator
{
protected:

    HandlePool<ImageAllocation, ImageHandle> Images;
    HandlePool<BufferAllocation, BufferHandle> Buffers;
    HandlePool<DescriptorSetAllocation, DescriptorSetHandle> DescriptorSets;

    enum class PendingKind : uint8_t { Buffer, Image, DescriptorSet };
    struct PendingRelease
    {
        uint64_t FrameNumber = 0;
        PendingKind Kind = PendingKind::Buffer;
        BufferAllocation Buffer = {};
        ImageAllocation Image = {};
        DescriptorSetAllocation DescriptorSet = {};
    };
    std::deque<PendingRelease> PendingReleases;
    uint64_t FrameNumber = 0;
    uint32_t FramesInFlight = 3;

    virtual void ReleaseBuffer(BufferAllocation& allocation) = 0;
    virtual void ReleaseImage(ImageAllocation& allocation) = 0;
    virtual void ReleaseDescriptorSet(DescriptorSetAllocation& allocation) = 0;
    void ReleasePending(PendingRelease& pending);

    static BufferAllocator* Instance;
    BufferAllocator() = default;

public:

    static BufferAllocator* GetInstance();
    virtual ~BufferAllocator() = default;

    uint64_t MakeKey(uint32_t pipelineID, uint32_t setIndex) { return (static_cast<uint64_t>(pipelineID) << 32) | setIndex; }

    virtual BufferHandle CreateBuffer(const BufferDesc& bufferDesc) = 0;
    virtual ImageHandle CreateImage(const ImageDesc& imageDesc) = 0;
    ImageHandle CacheImage(ImageAllocation imageAllocation) { return Images.Claim(imageAllocation); }

    // The handle goes stale at once. The GPU object is released only when every frame that
    // could still reference it has finished, FramesInFlight frames later.
    void DestroyBuffer(BufferHandle buffer);
    void DestroyImage(ImageHandle image);
    void FreeDescriptorSet(DescriptorSetHandle set);

    void BeginFrame(uint64_t frameNumber, uint32_t framesInFlight);
    void ReleaseAllPending();

    virtual void RegisterDescriptorSetLayout(uint32_t pipelineID, const ResourceLayout& layout, bool fillEmptySets = false) = 0;
    virtual DescriptorSetHandle AllocateDescriptorSet(uint32_t pipelineID, uint32_t setIndex,
                                                      const std::vector<DescriptorSetBinding>& bindings) = 0;
    virtual void UpdateDescriptorSetDynamicOffsets(DescriptorSetHandle set, const std::vector<uint32_t>& offsets) = 0;
    virtual void UpdateDescriptorSet(DescriptorSetHandle set, const std::vector<DescriptorSetBinding>& newBindings) = 0;

    // Drops the allocator's record of an image whose GPU object is owned elsewhere.
    virtual void EvictImage(ImageHandle image) = 0;

    const ImageAllocation& GetImageAllocation(ImageHandle image) const { return Images.Get(image); }
    const BufferAllocation& GetBufferAllocation(BufferHandle buffer) const { return Buffers.Get(buffer); }
    const DescriptorSetAllocation& GetDescriptorSet(DescriptorSetHandle set) const { return DescriptorSets.Get(set); }
};

class VulkanBufferAllocator : public BufferAllocator
{
public:

    VulkanBufferAllocator();
    ~VulkanBufferAllocator() override;

    BufferHandle CreateBuffer(const BufferDesc& bufferDesc) override;
    ImageHandle CreateImage(const ImageDesc& imageDesc) override;

    void RegisterDescriptorSetLayout(uint32_t pipelineID, const ResourceLayout& layout, bool fillEmptySets) override;
    DescriptorSetHandle AllocateDescriptorSet(uint32_t pipelineID, uint32_t setIndex,
                                              const std::vector<DescriptorSetBinding>& bindings) override;
    void UpdateDescriptorSetDynamicOffsets(DescriptorSetHandle set, const std::vector<uint32_t>& offsets) override;
    void UpdateDescriptorSet(DescriptorSetHandle set, const std::vector<DescriptorSetBinding>& newBindings) override;
    void EvictImage(ImageHandle image) override;

    VkDescriptorSetLayout GetRegisteredDescriptorSetLayout(uint32_t pipelineID, uint32_t setIndex);
    static uint32_t FindMemoryType(uint32_t allowdTypes, VkMemoryPropertyFlags flags);

protected:

    void ReleaseBuffer(BufferAllocation& allocation) override;
    void ReleaseImage(ImageAllocation& allocation) override;
    void ReleaseDescriptorSet(DescriptorSetAllocation& allocation) override;

private:

    struct DescriptorSetLayoutInfo
    {
        VkDescriptorSetLayout Layout;
        VkDescriptorPool Pool;
        std::vector<DescriptorBinding> Bindings;
    };
    std::map<uint64_t, DescriptorSetLayoutInfo> DescriptorSetLayouts;

    static VkImage CreateVulkanImage(ImageDesc imageDesc, VkDeviceMemory* imageMemory);
    static VkImageView CreateVulkanImageView(VkImage image, ImageDesc imageDesc);

    static void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    static void CopyToDeviceLocalBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size);
    static void CopyBufferToImage(VkBuffer stagingBuffer, VkImage dstImage, uint32_t width, uint32_t height);
};

class DirectX12BufferAllocator : public BufferAllocator
{
public:

    DirectX12BufferAllocator();
    ~DirectX12BufferAllocator() override;

    BufferHandle CreateBuffer(const BufferDesc& bufferDesc) override;
    ImageHandle CreateImage(const ImageDesc& imageDesc) override;

    void RegisterDescriptorSetLayout(uint32_t pipelineID, const ResourceLayout& layout, bool fillEmptySets) override;
    DescriptorSetHandle AllocateDescriptorSet(uint32_t pipelineID, uint32_t setIndex,
                                              const std::vector<DescriptorSetBinding>& bindings) override;
    void UpdateDescriptorSetDynamicOffsets(DescriptorSetHandle set, const std::vector<uint32_t>& offsets) override;
    void UpdateDescriptorSet(DescriptorSetHandle set, const std::vector<DescriptorSetBinding>& newBindings) override;
    void EvictImage(ImageHandle image) override;

    enum DescriptorType : uint8_t { CBV, SRV, UAV, RTV, DSV };
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(DescriptorType type);
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(size_t index, DescriptorType type);

    ComPtr<ID3D12DescriptorHeap> GetShaderResourceHeap() const { return ShaderResourceHeap; }

protected:

    void ReleaseBuffer(BufferAllocation& allocation) override;
    void ReleaseImage(ImageAllocation& allocation) override;
    void ReleaseDescriptorSet(DescriptorSetAllocation& allocation) override;

private:

    struct DescriptorSetLayoutInfo
    {
        std::vector<DescriptorBinding> Bindings;
    };
    std::map<uint64_t, DescriptorSetLayoutInfo> DescriptorSetLayouts;

    struct DescriptorTableData
    {
        D3D12_GPU_DESCRIPTOR_HANDLE BaseHandle;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> CpuHandles;
        std::vector<DescriptorType> DescriptorTypes;
        std::vector<BufferHandle> Buffers;
        std::vector<bool> IsDynamic;
    };

    ComPtr<ID3D12DescriptorHeap> ShaderResourceHeap;
    ComPtr<ID3D12DescriptorHeap> RenderTargetHeap;
    ComPtr<ID3D12DescriptorHeap> DepthStencilHeap;

    UINT ShaderResourceOffset = 0;
    UINT RenderTargetOffset = 0;
    UINT DepthStencilOffset = 0;

    uint32_t NextSRVIndex = 0;
    uint32_t NextCBVIndex = 0;
    uint32_t NextUAVIndex = 0;
    uint32_t NextRTVIndex = 0;
    uint32_t NextDSVIndex = 0;

    static constexpr UINT MaxSRVs = 8192;
    static constexpr UINT MaxCBVs = 2048;
    static constexpr UINT MaxUAVs = 1024;
    static constexpr UINT MaxRTVs = 512;
    static constexpr UINT MaxDSVs = 256;
};
