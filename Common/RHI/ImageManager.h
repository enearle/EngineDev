#pragma once
#include <unordered_map>
#include "RHIStructures.h"

using namespace RHIStructures;
class ImageManager
{
public:
    virtual ~ImageManager() = default;
        
    virtual ResourceHandle CreateImage(const ImageDesc& desc) = 0;
    virtual void DestroyImage(ResourceHandle handle) = 0;
    virtual const ImageAllocation* GetImageAllocation(ResourceHandle handle) const = 0;
    virtual uint64_t GetAllocatedMemory() const = 0;
};

class D3DImageManager : public ImageManager
{
public:
    ResourceHandle CreateImage(const ImageDesc& desc) override;
    void DestroyImage(ResourceHandle handle) override;
    const ImageAllocation* GetImageAllocation(ResourceHandle handle) const override;
    uint64_t GetAllocatedMemory() const override;

private:
    uint64_t NextHandleId = 1;
    std::unordered_map<uint64_t, ImageAllocation> Allocations;
    uint64_t TotalMemory = 0;
};

class VulkanImageManager : public ImageManager
{
public:
    ResourceHandle CreateImage(const ImageDesc& desc) override;
    void DestroyImage(ResourceHandle handle) override;
    const ImageAllocation* GetImageAllocation(ResourceHandle handle) const override;
    uint64_t GetAllocatedMemory() const override;

private:
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        
    uint64_t NextHandleId = 1;
    std::unordered_map<uint64_t, ImageAllocation> Allocations;
    uint64_t TotalMemory = 0;
};
