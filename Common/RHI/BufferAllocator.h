#pragma once
#include <unordered_map>
#include <map>
#include "RHIStructures.h"

class BitPool;
using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

class BufferAllocator
{
protected:
    std::unordered_map<uint64_t, ImageAllocation> AllocatedImages;
    std::unordered_map<uint64_t, BufferAllocation> AllocatedBuffers;
    std::unordered_map<uint64_t, DescriptorSetAllocation> AllocatedDescriptorSets;
    
    uint64_t NextBufferID = 0;
    uint64_t NextImageID = 0;
    uint64_t NextDescriptorSetID = 0;
    
    uint64_t CacheImage(ImageAllocation imageAllocation) {AllocatedImages[NextImageID] = imageAllocation; return NextImageID++;}
    uint64_t CacheBuffer(BufferAllocation bufferAllocation) {AllocatedBuffers[NextBufferID] = bufferAllocation; return NextBufferID++;}
    uint64_t CacheDescriptorSet(DescriptorSetAllocation setAllocation) {AllocatedDescriptorSets[NextDescriptorSetID] = setAllocation; return NextDescriptorSetID++;}
    
    static BufferAllocator* Instance;
    BufferAllocator() = default;

public:
    static BufferAllocator* GetInstance();
    
    virtual uint64_t CreateBuffer(BufferDesc bufferDesc, bool createDescriptor = false) = 0;
    virtual uint64_t CreateImage(ImageDesc imageDesc, bool createDescriptor = false) = 0;
    
    virtual ~BufferAllocator() = default;
    virtual void FreeBuffer(uint64_t id) = 0;
    virtual void FreeImage(uint64_t id) = 0;
    
    virtual void RegisterDescriptorSetLayout(const ResourceLayout& layout) = 0;
    virtual uint64_t AllocateDescriptorSet(uint32_t pipelineIndex, const std::vector<DescriptorSetBinding>& bindings) = 0;
    virtual void FreeDescriptorSet(uint64_t setID) = 0;
    
    ImageAllocation GetImageAllocation(uint64_t id) const { return AllocatedImages.at(id); }
    BufferAllocation GetBufferAllocation(uint64_t id) const { 
        return AllocatedBuffers.at(id); }
    DescriptorSetAllocation GetDescriptorSet(uint64_t id) const { return AllocatedDescriptorSets.at(id); }
};

class VulkanBufferAllocator : public BufferAllocator
{
public:
    uint64_t CreateBuffer(BufferDesc bufferDesc, bool createDescriptor = false) override;
    uint64_t CreateImage(ImageDesc imageDesc, bool createDescriptor = false) override;
    VulkanBufferAllocator();
    ~VulkanBufferAllocator() override;
    void FreeBuffer(uint64_t id) override;
    void FreeImage(uint64_t id) override;
    
    void RegisterDescriptorSetLayout(const ResourceLayout& layout) override;
    uint64_t AllocateDescriptorSet(uint32_t pipelineIndex, const std::vector<DescriptorSetBinding>& bindings) override;
    void FreeDescriptorSet(uint64_t setID) override;
    
    uint64_t GetDescriptorBufferAddress() { return DescriptorBufferAddress; }

    enum DescriptorType : uint8_t { SampledImage, StorageImage, UniformBuffer, StorageBuffer};
    
    static uint32_t FindMemoryType(uint32_t allowdTypes, VkMemoryPropertyFlags flags);
    
private:
    // Descriptor buffer for individual descriptors
    VkBuffer DescriptorBuffer;
    VkDeviceMemory DescriptorBufferMemory;
    void* DescriptorBufferMapped;
    VkDeviceAddress DescriptorBufferAddress = 0;
    
    static constexpr uint16_t SampledImagePoolSize = 4096;
    static constexpr uint16_t StorageImagePoolSize = 1024;
    static constexpr uint16_t UniformBufferPoolSize = 2048;
    static constexpr uint16_t StorageBufferPoolSize = 1024;
    
    size_t SampledImageStride;
    size_t StorageImageStride;
    size_t UniformBufferStride;
    size_t StorageBufferStride;
    
    BitPool* SampledImagePool;
    BitPool* StorageImagePool;
    BitPool* UniformBufferPool;
    BitPool* StorageBufferPool;
    
    // Descriptor pools for descriptor sets (traditional Vulkan approach)
    struct DescriptorSetLayoutInfo
    {
        VkDescriptorSetLayout Layout;
        VkDescriptorPool Pool;
        std::vector<DescriptorBinding> Bindings;
    };
    std::map<uint32_t, DescriptorSetLayoutInfo> DescriptorSetLayouts;
    
    VkDeviceAddress AllocateDescriptor(VkDescriptorGetInfoEXT* descriptorInfo, DescriptorType type);
    void FreeDescriptor(VkDeviceAddress address, DescriptorType type);
    
    static VkImage CreateVulkanImage(ImageDesc imageDesc, VkDeviceMemory* imageMemory);
    static VkImageView CreateVulkanImageView(VkImage image, ImageDesc imageDesc);
   
    static void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
    static void CopyToDeviceLocalBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size);
    static void CopyBufferToImage(VkBuffer stagingBuffer, VkImage dstImage, uint32_t width, uint32_t height);
};

class DirectX12BufferAllocator : public BufferAllocator
{
public:
    uint64_t CreateBuffer(BufferDesc bufferDesc, bool createDescriptor = false) override;
    uint64_t CreateImage(ImageDesc imageDesc, bool createDescriptor = false) override;
    DirectX12BufferAllocator();
    ~DirectX12BufferAllocator() override;
    void FreeBuffer(uint64_t id) override;
    void FreeImage(uint64_t id) override;

    void RegisterDescriptorSetLayout(const ResourceLayout& layout) override;
    uint64_t AllocateDescriptorSet(uint32_t pipelineIndex, const std::vector<DescriptorSetBinding>& bindings) override;
    void FreeDescriptorSet(uint64_t setID) override;

    enum DescriptorType : uint8_t { SRV, CBV, UAV, RTV, DSV };
    
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(DescriptorType type);
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(size_t index, DescriptorType type);
    void FreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, DescriptorType type);
    
    ComPtr<ID3D12DescriptorHeap> GetShaderResourceHeap() const { return ShaderResourceHeap; }

private:
    struct DescriptorSetLayoutInfo
    {
        std::vector<DescriptorBinding> Bindings;
    };
    std::map<uint32_t, DescriptorSetLayoutInfo> DescriptorSetLayouts;
    
    struct DescriptorTableData
    {
        D3D12_GPU_DESCRIPTOR_HANDLE BaseHandle;
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> CpuHandles;
        std::vector<DescriptorType> DescriptorTypes;
    };
    
    ComPtr<ID3D12DescriptorHeap> ShaderResourceHeap;
    ComPtr<ID3D12DescriptorHeap> RenderTargetHeap;
    ComPtr<ID3D12DescriptorHeap> DepthStencilHeap;

    UINT ShaderResourceOffset = 0;
    UINT RenderTargetOffset = 0;
    UINT DepthStencilOffset = 0;
    
    BitPool* SRVAllocator;
    BitPool* CBVAllocator;
    BitPool* UAVAllocator;
    BitPool* RTVAllocator;
    BitPool* DSVAllocator;
    
    static constexpr UINT MaxSRVs = 8192;
    static constexpr UINT MaxCBVs = 2048;
    static constexpr UINT MaxUAVs = 1024;
    static constexpr UINT MaxRTVs = 512;
    static constexpr UINT MaxDSVs = 256;
};

