#pragma once
#include <unordered_map>
#include <map>
#include "RHIStructures.h"
#include "../RHI_API_Macro.h"
class BitPool;
using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

class BufferAllocator
{
protected:
    
    std::unordered_map<uint64_t, ImageAllocation> AllocatedImages;
    std::unordered_map<uint64_t, BufferAllocation> AllocatedBuffers;
    std::map<uint64_t, DescriptorSetAllocation> AllocatedDescriptorSets;
    
    uint64_t NextBufferID = 0;
    uint64_t NextImageID = 0;
    uint64_t NextDescriptorSetID = 0;
    
    uint64_t CacheBuffer(BufferAllocation bufferAllocation) {AllocatedBuffers[NextBufferID] = bufferAllocation; return NextBufferID++;}
    uint64_t CacheDescriptorSet(DescriptorSetAllocation setAllocation) { AllocatedDescriptorSets[NextDescriptorSetID] = setAllocation; return NextDescriptorSetID++; }
    
    static BufferAllocator* Instance;
    BufferAllocator() = default;
    
public:    
    
    uint64_t MakeKey(uint32_t pipelineID, uint32_t setIndex) { return (static_cast<uint64_t>(pipelineID) << 32) | setIndex; }
    static BufferAllocator* GetInstance();
    uint64_t CacheImage(ImageAllocation imageAllocation) {AllocatedImages[NextImageID] = imageAllocation; return NextImageID++;}
    virtual uint64_t CreateBuffer(BufferDesc bufferDesc) = 0;
    virtual uint64_t CreateImage(ImageDesc imageDesc) = 0;
    
    virtual ~BufferAllocator() = default;
    
    virtual void RegisterDescriptorSetLayout(uint32_t pipelineID, const ResourceLayout& layout, bool fillEmptySets = false) = 0;
    virtual uint64_t AllocateDescriptorSet(uint32_t pipelineID, uint32_t setIndex, 
                                           const std::vector<DescriptorSetBinding>& bindings) = 0;
    virtual void FreeDescriptorSet(uint64_t setID) = 0;
    virtual void UpdateDescriptorSetDynamicOffsets(uint64_t setID, const std::vector<uint32_t>& offsets) = 0;
    virtual void UpdateDescriptorSet(uint64_t setID, const std::vector<DescriptorSetBinding>& newBindings) {}
    virtual void EvictImage(uint64_t id) { AllocatedImages.erase(id); }

    ImageAllocation GetImageAllocation(uint64_t id) const { return AllocatedImages.at(id); }
    BufferAllocation GetBufferAllocation(uint64_t id) const { return AllocatedBuffers.at(id); }
    DescriptorSetAllocation GetDescriptorSet(uint64_t id) const { return AllocatedDescriptorSets.at(id); }
};

class VulkanBufferAllocator : public BufferAllocator
{
public:
    
    uint64_t CreateBuffer(BufferDesc bufferDesc) override;
    uint64_t CreateImage(ImageDesc imageDesc) override;
    VulkanBufferAllocator();
    ~VulkanBufferAllocator() override;
    
    void RegisterDescriptorSetLayout(uint32_t pipelineID, const ResourceLayout& layout, bool fillEmptySets) override;
    uint64_t AllocateDescriptorSet(uint32_t pipelineID, uint32_t setIndex, 
                                           const std::vector<DescriptorSetBinding>& bindings) override;
    void FreeDescriptorSet(uint64_t setID) override;
    void UpdateDescriptorSetDynamicOffsets(uint64_t setID, const std::vector<uint32_t>& offsets) override;
    void UpdateDescriptorSet(uint64_t setID, const std::vector<DescriptorSetBinding>& newBindings) override;
    void EvictImage(uint64_t id) override;
    enum DescriptorType : uint8_t { SampledImage, StorageImage, UniformBuffer, StorageBuffer};
    VkDescriptorSetLayout GetRegisteredDescriptorSetLayout(uint32_t pipelineID, uint32_t setIndex);
    static uint32_t FindMemoryType(uint32_t allowdTypes, VkMemoryPropertyFlags flags);
    
private:
    
    static constexpr uint16_t SampledImagePoolSize = 4096;
    static constexpr uint16_t StorageImagePoolSize = 1024;
    static constexpr uint16_t UniformBufferPoolSize = 2048;
    static constexpr uint16_t StorageBufferPoolSize = 1024;

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
    uint64_t CreateBuffer(BufferDesc bufferDesc) override;
    uint64_t CreateImage(ImageDesc imageDesc) override;
    DirectX12BufferAllocator();
    ~DirectX12BufferAllocator() override;

    void RegisterDescriptorSetLayout(uint32_t pipelineID, const ResourceLayout& layout, bool fillEmptySets) override;
    uint64_t AllocateDescriptorSet(uint32_t pipelineID, uint32_t setIndex, 
                                           const std::vector<DescriptorSetBinding>& bindings) override;
    void FreeDescriptorSet(uint64_t setID) override;
    void UpdateDescriptorSetDynamicOffsets(uint64_t setID, const std::vector<uint32_t>& offsets) override;
    enum DescriptorType : uint8_t {CBV, SRV, UAV, RTV, DSV};
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(DescriptorType type);
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(size_t index, DescriptorType type);
    
    ComPtr<ID3D12DescriptorHeap> GetShaderResourceHeap() const { return ShaderResourceHeap; }

    
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
        std::vector<uint64_t> ResourceIDs;
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

