#pragma once
#include "RHIStructures.h"

using namespace RHIStructures;
class ResourceBinder
{
public:
    virtual ~ResourceBinder() = default;
        
    // Bind a buffer to a slot
    virtual void BindBuffer(uint32_t slot, ResourceHandle buffer, uint32_t offset = 0, uint32_t size = 0) = 0;
        
    // Bind an image/sampled texture
    virtual void BindImage(uint32_t slot, ResourceHandle image) = 0;
        
    // Commit bindings (issue actual GPU commands)
    virtual void CommitBindings() = 0;
        
    // Clear all bindings
    virtual void Reset() = 0;
};

class D3DResourceBinder : public ResourceBinder
{
public:
    void BindBuffer(uint32_t slot, ResourceHandle buffer, uint32_t offset = 0, uint32_t size = 0) override;
    void BindImage(uint32_t slot, ResourceHandle image) override;
    void CommitBindings() override;
    void Reset() override;

private:
    std::vector<ResourceBinding> PendingBindings;
};

class VulkanResourceBinder : public ResourceBinder
{
public:
    void BindBuffer(uint32_t slot, ResourceHandle buffer, uint32_t offset = 0, uint32_t size = 0) override;
    void BindImage(uint32_t slot, ResourceHandle image) override;
    void CommitBindings() override;
    void Reset() override;

private:
    std::vector<ResourceBinding> PendingBindings;
    std::vector<VkWriteDescriptorSet> DescriptorWrites;
};
