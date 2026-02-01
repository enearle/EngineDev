#pragma once
#include "RHIStructures.h"

using namespace RHIStructures;
class BufferAllocator
{
public:
    static BufferAllocator* Create();
    virtual BufferAllocation CreateBuffer(MemoryAccess memoryAccess, BufferUsage bufferUsage, uint64_t sizeInBytes, void* data);
};

class VulkanBufferAllocator : public BufferAllocator
{
public:
    BufferAllocation CreateBuffer(MemoryAccess memoryAccess, BufferUsage bufferUsage, uint64_t sizeInBytes, void* data) override;
private:
    static void TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);
};

class DirectX12BufferAllocator : public BufferAllocator
{
public:
    //BufferAllocation CreateUploadBuffer() override;
};
