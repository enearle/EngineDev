#include "ResourceManager.h"
#include "../GraphicsSettings.h"


ResourceManager& ResourceManager::GetInstance()
{
    static ResourceManager instance;
    return instance;
}

void ResourceManager::Initialize()
{
    if (GRAPHICS_SETTINGS.APIToUse == Direct3D12)
    {
        BufferMgr = std::make_unique<D3DBufferManager>();
        ImageMgr = std::make_unique<D3DImageManager>();
        Binder = std::make_unique<D3DResourceBinder>();
    }
    else if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
    {
        BufferMgr = std::make_unique<VulkanBufferManager>();
        ImageMgr = std::make_unique<VulkanImageManager>();
        Binder = std::make_unique<VulkanResourceBinder>();
    }
}

void ResourceManager::Shutdown()
{
    BufferMgr.reset();
    ImageMgr.reset();
    Binder.reset();
}

ResourceHandle ResourceManager::CreateVertexBuffer(const void* data, uint64_t size)
{
    BufferDesc desc = {};
    desc.Size = size;
    desc.Usage = BufferUsage::VertexBuffer;
    desc.Access = MemoryAccess::GPURead;
    desc.InitialData = data;
    return BufferMgr->CreateBuffer(desc);
}

ResourceHandle ResourceManager::CreateIndexBuffer(const void* data, uint64_t size)
{
    BufferDesc desc = {};
    desc.Size = size;
    desc.Usage = BufferUsage::IndexBuffer;
    desc.Access = MemoryAccess::GPURead;
    desc.InitialData = data;
    return BufferMgr->CreateBuffer(desc);
}

ResourceHandle ResourceManager::CreateUniformBuffer(uint64_t size)
{
    BufferDesc desc = {};
    desc.Size = size;
    desc.Usage = BufferUsage::UniformBuffer;
    desc.Access = MemoryAccess::CPUWrite | MemoryAccess::GPURead;
    return BufferMgr->CreateBuffer(desc);
}

ResourceHandle ResourceManager::CreateDepthImage(uint32_t width, uint32_t height)
{
    ImageDesc desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = RHIStructures::Format::D32_FLOAT;
    desc.Usage = ImageUsage::DepthAttachmentImage;
    return ImageMgr->CreateImage(desc);
}

ResourceHandle ResourceManager::CreateColorImage(uint32_t width, uint32_t height, RHIStructures::Format format)
{
    ImageDesc desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.Usage = ImageUsage::ColorAttachmentImage | ImageUsage::SampledImage;
    return ImageMgr->CreateImage(desc);
}
