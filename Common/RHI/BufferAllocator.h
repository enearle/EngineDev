#pragma once
#include <unordered_map>
#include "RHIStructures.h"

class BitPool;
using namespace RHIStructures;
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
    virtual BufferAllocation CreateBuffer(BufferDesc bufferDesc) = 0;
    virtual ImageAllocation CreateImage(ImageDesc imageDesc) = 0;
    virtual ~BufferAllocator() = default;
};

class VulkanBufferAllocator : public BufferAllocator
{
public:
    BufferAllocation CreateBuffer(BufferDesc bufferDesc) override;
    ImageAllocation CreateImage(ImageDesc imageDesc) override;
    VulkanBufferAllocator();
    ~VulkanBufferAllocator() override;

    enum DescriptorType : uint8_t { Sampler, SampledImage, StorageImage, UniformBuffer, StorageBuffer };
    
private:
    VkBuffer DescriptorBuffer;
    VkDeviceMemory DescriptorBufferMemory;
    void* DescriptorBufferMapped;
    VkDeviceAddress DescriptorBufferAddress = 0;
    uint16_t SamplerPoolSize = 4096;
    uint16_t SampledImagePoolSize = 4096;
    uint16_t StorageImagePoolSize = 1024;
    uint16_t UniformBufferPoolSize = 2048;
    uint16_t StorageBufferPoolSize = 1024;
    size_t SamplerStride = 16;
    size_t SampledImageStride = 32;
    size_t StorageImageStride = 32;
    size_t UniformBufferStride = 32;
    size_t StorageBufferStride = 32;
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
    BufferAllocation CreateBuffer(BufferDesc bufferDesc) override;
    ImageAllocation CreateImage(ImageDesc imageDesc) override;
    ~DirectX12BufferAllocator() override;
};
