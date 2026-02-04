#pragma once
#include <unordered_map>
#include "RHIStructures.h"

class BitPool;
using namespace RHIStructures;
using Microsoft::WRL::ComPtr;
class BufferAllocator
{
protected:
    std::unordered_map<uint64_t, ImageAllocation> AllocatedImages;
    std::unordered_map<uint64_t, BufferAllocation> AllocatedBuffers;
    uint64_t NextBufferID = 0;
    uint64_t NextImageID = 0;
    uint64_t CacheImage(ImageAllocation imageAllocation) {AllocatedImages[NextImageID] = imageAllocation; return NextImageID++;}
    uint64_t CacheBuffer(BufferAllocation bufferAllocation) {AllocatedBuffers[NextBufferID] = bufferAllocation; return NextBufferID++;}
public:
    static BufferAllocator* Create();
    virtual uint64_t CreateBuffer(BufferDesc bufferDesc) = 0;
    virtual uint64_t CreateImage(ImageDesc imageDesc) = 0;
    BufferAllocator() = default;
    virtual ~BufferAllocator() = default;
    virtual void FreeBuffer(uint64_t id) = 0;
    virtual void FreeImage(uint64_t id) = 0;
    ImageAllocation GetImageAllocation(uint64_t id) const { return AllocatedImages.at(id); }
    BufferAllocation GetBufferAllocation(uint64_t id) const { return AllocatedBuffers.at(id); }
};

class VulkanBufferAllocator : public BufferAllocator
{
public:
    uint64_t CreateBuffer(BufferDesc bufferDesc) override;
    uint64_t CreateImage(ImageDesc imageDesc) override;
    VulkanBufferAllocator();
    ~VulkanBufferAllocator() override;
    void FreeBuffer(uint64_t id) override;
    void FreeImage(uint64_t id) override;
    VkDeviceAddress GetDescriptorBufferAddress() { return DescriptorBufferAddress; }

    enum DescriptorType : uint8_t { Sampler, SampledImage, StorageImage, UniformBuffer, StorageBuffer};
    
private:
    VkBuffer DescriptorBuffer;
    VkDeviceMemory DescriptorBufferMemory;
    void* DescriptorBufferMapped;
    VkDeviceAddress DescriptorBufferAddress = 0;
    
    static constexpr uint16_t SamplerPoolSize = 4096;
    static constexpr uint16_t SampledImagePoolSize = 4096;
    static constexpr uint16_t StorageImagePoolSize = 1024;
    static constexpr uint16_t UniformBufferPoolSize = 2048;
    static constexpr uint16_t StorageBufferPoolSize = 1024;
    
    size_t SamplerStride;
    size_t SampledImageStride;
    size_t StorageImageStride;
    size_t UniformBufferStride;
    size_t StorageBufferStride;
    
    BitPool* SamplerPool;
    BitPool* SampledImagePool;
    BitPool* StorageImagePool;
    BitPool* UniformBufferPool;
    BitPool* StorageBufferPool;
    
    VkDeviceAddress AllocateDescriptor(VkDescriptorGetInfoEXT* descriptorInfo, DescriptorType type);
    void FreeDescriptor(VkDeviceAddress address, DescriptorType type);
    
    static VkImage CreateVulkanImage(ImageDesc imageDesc, VkDeviceMemory* imageMemory);
    static VkImageView CreateVulkanImageView(VkImage image, ImageDesc imageDesc);
    static uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t allowdTypes, VkMemoryPropertyFlags flags);
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
    void FreeBuffer(uint64_t id) override;
    void FreeImage(uint64_t id) override;

    enum DescriptorType : uint8_t { SRV, CBV, UAV, RTV, DSV };
    
    D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(DescriptorType type);
    D3D12_CPU_DESCRIPTOR_HANDLE GetHandle(size_t index, DescriptorType type);
    void FreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, DescriptorType type);


    ID3D12DescriptorHeap* GetShaderResourceHeap() const { return ShaderResourceHeap.Get(); }

private:
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

