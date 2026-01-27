#pragma once
#include <unordered_map>

#include "RHIStructures.h"
using namespace RHIStructures;

class BufferManager
{
public:
    virtual ~BufferManager() = default;
        
    // Create a buffer with optional initial data
    virtual ResourceHandle CreateBuffer(const BufferDesc& desc) = 0;
        
    // Destroy a buffer
    virtual void DestroyBuffer(ResourceHandle handle) = 0;
        
    // Map buffer for CPU access
    virtual void* MapBuffer(ResourceHandle handle) = 0;
        
    // Unmap buffer
    virtual void UnmapBuffer(ResourceHandle handle) = 0;
        
    // Get buffer info
    virtual const BufferAllocation* GetBufferAllocation(ResourceHandle handle) const = 0;
        
    // Upload data to buffer (handles staging internally)
    virtual void UpdateBuffer(ResourceHandle handle, const void* data, uint64_t size, uint64_t offset = 0) = 0;
        
    // Get total allocated memory
    virtual uint64_t GetAllocatedMemory() const = 0;
};

class D3DBufferManager : public BufferManager
{
public:
    ResourceHandle CreateBuffer(const BufferDesc& desc) override;
    void DestroyBuffer(ResourceHandle handle) override;
    void* MapBuffer(ResourceHandle handle) override;
    void UnmapBuffer(ResourceHandle handle) override;
    const BufferAllocation* GetBufferAllocation(ResourceHandle handle) const override;
    void UpdateBuffer(ResourceHandle handle, const void* data, uint64_t size, uint64_t offset = 0) override;
    uint64_t GetAllocatedMemory() const override;

private:
    uint64_t NextHandleId = 1;
    std::unordered_map<uint64_t, BufferAllocation> Allocations;
    uint64_t TotalMemory = 0;
    
};

class VulkanBufferManager : public BufferManager
{
public:
    ResourceHandle CreateBuffer(const BufferDesc& desc) override;
    void DestroyBuffer(ResourceHandle handle) override;
    void* MapBuffer(ResourceHandle handle) override;
    void UnmapBuffer(ResourceHandle handle) override;
    const BufferAllocation* GetBufferAllocation(ResourceHandle handle) const override;
    void UpdateBuffer(ResourceHandle handle, const void* data, uint64_t size, uint64_t offset = 0) override;
    uint64_t GetAllocatedMemory() const override;

private:
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        
    uint64_t NextHandleId = 1;
    std::unordered_map<uint64_t, BufferAllocation> Allocations;
    uint64_t TotalMemory = 0;
};

