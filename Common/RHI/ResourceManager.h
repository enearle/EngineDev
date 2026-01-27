#pragma once
#include "BufferManager.h"
#include "ImageManager.h"
#include "ResourceBinder.h"
#include <memory>

class ResourceManager
{
public:
    static ResourceManager& GetInstance();
        
    // Initialize with API type
    void Initialize();
    void Shutdown();
        
    // Access managers
    BufferManager* GetBufferManager() { return BufferMgr.get(); }
    ImageManager* GetImageManager() { return ImageMgr.get(); }
    ResourceBinder* GetResourceBinder() { return Binder.get(); }
        
    // Convenience methods
    ResourceHandle CreateVertexBuffer(const void* data, uint64_t size);
    ResourceHandle CreateIndexBuffer(const void* data, uint64_t size);
    ResourceHandle CreateUniformBuffer(uint64_t size);
    ResourceHandle CreateDepthImage(uint32_t width, uint32_t height);
    ResourceHandle CreateColorImage(uint32_t width, uint32_t height, RHIStructures::Format format);

private:
    ResourceManager() = default;
    ~ResourceManager() = default;
        
    std::unique_ptr<BufferManager> BufferMgr;
    std::unique_ptr<ImageManager> ImageMgr;
    std::unique_ptr<ResourceBinder> Binder;
};

