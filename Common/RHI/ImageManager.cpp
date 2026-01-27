#include "ImageManager.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"Image
#include "../Windows/Win32ErrorHandler.h"

using namespace Win32ErrorHandler;

ResourceHandle D3DImageManager::CreateImage(const ImageDesc& desc)
    {
        auto& d3dCore = D3DCore::GetInstance();
        auto device = d3dCore.GetDevice();
        
        D3D12_RESOURCE_DESC resourceDesc{};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = desc.Width;
        resourceDesc.Height = desc.Height;
        resourceDesc.DepthOrArraySize = static_cast<UINT16>(desc.ArrayLayers);
        resourceDesc.MipLevels = static_cast<UINT16>(desc.MipLevels);
        resourceDesc.Format = RHIStructures::DXFormat(desc.Format);
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        
        // Determine resource flags
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if ((desc.Usage & ImageUsage::ColorAttachmentImage) != 0)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        if ((desc.Usage & ImageUsage::DepthAttachmentImage) != 0)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        if ((desc.Usage & ImageUsage::StorageImage) != 0)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        
        resourceDesc.Flags = flags;
        
        D3D12_HEAP_PROPERTIES heapProps{};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        
        // Determine initial state
        D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
        if ((desc.Usage & ImageUsage::ColorAttachmentImage) != 0)
            initialState = D3D12_RESOURCE_STATE_RENDER_TARGET;
        else if ((desc.Usage & ImageUsage::DepthAttachmentImage) != 0)
            initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        
        ComPtr<ID3D12Resource> resource;
        device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            initialState,
            nullptr,
            IID_PPV_ARGS(&resource)
        ) >> ERROR_HANDLER;
        
        // Create descriptor handle if needed
        void* descriptorHandle = nullptr;
        if ((desc.Usage & ImageUsage::ColorAttachmentImage) != 0)
        {
            auto rtvHeap = d3dCore.GetRenderTargetDescriptorHeap();
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            descriptorHandle = new D3D12_CPU_DESCRIPTOR_HANDLE(rtvHandle);
            
            device->CreateRenderTargetView(resource.Get(), nullptr, rtvHandle);
        }
        else if ((desc.Usage & ImageUsage::DepthAttachmentImage) != 0)
        {
            auto dsvHeap = d3dCore.GetDepthStencilDescriptorHeap();
            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
            descriptorHandle = new D3D12_CPU_DESCRIPTOR_HANDLE(dsvHandle);
            
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
            dsvDesc.Format = resourceDesc.Format;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = 0;
            
            device->CreateDepthStencilView(resource.Get(), &dsvDesc, dsvHandle);
        }
        
        ImageAllocation allocation{};
        allocation.Width = desc.Width;
        allocation.Height = desc.Height;
        allocation.Depth = desc.Depth;
        allocation.Format = desc.Format;
        allocation.Usage = desc.Usage;
        allocation.MipLevels = desc.MipLevels;
        allocation.ArrayLayers = desc.ArrayLayers;
        allocation.D3D12Resource = resource.Get();
        allocation.DescriptorHandle = descriptorHandle;
        
        resource.Detach();
        
        uint64_t handle = NextHandleId++;
        Allocations[handle] = allocation;
        TotalMemory += desc.Width * desc.Height * 4;  // Rough estimate
        
        ResourceHandle result;
        result.Handle = handle;
        return result;
    }

    void D3DImageManager::DestroyImage(ResourceHandle handle)
    {
        if (!handle.IsValid()) return;
        
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return;
        
        auto resource = static_cast<ID3D12Resource*>(it->second.D3D12Resource);
        resource->Release();
        
        if (it->second.DescriptorHandle)
            delete static_cast<D3D12_CPU_DESCRIPTOR_HANDLE*>(it->second.DescriptorHandle);
        
        TotalMemory -= it->second.Width * it->second.Height * 4;
        Allocations.erase(it);
    }

    const ImageAllocation* D3DImageManager::GetImageAllocation(ResourceHandle handle) const
    {
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return nullptr;
        return &it->second;
    }

    uint64_t D3DImageManager::GetAllocatedMemory() const
    {
        return TotalMemory;
    }

uint32_t VulkanImageManager::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        auto& vulkanCore = VulkanCore::GetInstance();
        VkPhysicalDevice physicalDevice = vulkanCore.GetPhysicalDevice();
        
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
        
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && 
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i;
            }
        }
        
        throw std::runtime_error("Failed to find suitable memory type for image");
    }

    ResourceHandle VulkanImageManager::CreateImage(const ImageDesc& desc)
    {
        auto& vulkanCore = VulkanCore::GetInstance();
        VkDevice device = vulkanCore.GetDevice();
        
        // Determine image usage flags
        VkImageUsageFlags usageFlags = 0;
        if ((desc.Usage & ImageUsage::ColorAttachmentImage) != 0)
            usageFlags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if ((desc.Usage & ImageUsage::DepthAttachmentImage) != 0)
            usageFlags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if ((desc.Usage & ImageUsage::SampledImage) != 0)
            usageFlags |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if ((desc.Usage & ImageUsage::StorageImage) != 0)
            usageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        if ((desc.Usage & ImageUsage::TransferSrcImage) != 0)
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if ((desc.Usage & ImageUsage::TransferDstImage) != 0)
            usageFlags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = desc.Depth > 1 ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = desc.Width;
        imageInfo.extent.height = desc.Height;
        imageInfo.extent.depth = desc.Depth;
        imageInfo.mipLevels = desc.MipLevels;
        imageInfo.arrayLayers = desc.ArrayLayers;
        imageInfo.format = RHIStructures::VulkanFormat(desc.Format);
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usageFlags;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        
        VkImage image = VK_NULL_HANDLE;
        VkResult result = vkCreateImage(device, &imageInfo, nullptr, &image);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image");
        
        // Get memory requirements
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, image, &memRequirements);
        
        // Allocate memory
        uint32_t memoryType = FindMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryType;
        
        VkDeviceMemory imageMemory = VK_NULL_HANDLE;
        result = vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory);
        if (result != VK_SUCCESS)
        {
            vkDestroyImage(device, image, nullptr);
            throw std::runtime_error("Failed to allocate Vulkan image memory");
        }
        
        // Bind memory to image
        result = vkBindImageMemory(device, image, imageMemory, 0);
        if (result != VK_SUCCESS)
        {
            vkFreeMemory(device, imageMemory, nullptr);
            vkDestroyImage(device, image, nullptr);
            throw std::runtime_error("Failed to bind Vulkan image memory");
        }
        
        // Create image view
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        if ((desc.Usage & ImageUsage::DepthAttachmentImage) != 0)
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = desc.ArrayLayers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange.aspectMask = aspectMask;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = desc.MipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = desc.ArrayLayers;
        
        VkImageView imageView = VK_NULL_HANDLE;
        result = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
        if (result != VK_SUCCESS)
        {
            vkFreeMemory(device, imageMemory, nullptr);
            vkDestroyImage(device, image, nullptr);
            throw std::runtime_error("Failed to create Vulkan image view");
        }
        
        ImageAllocation allocation{};
        allocation.Width = desc.Width;
        allocation.Height = desc.Height;
        allocation.Depth = desc.Depth;
        allocation.Format = desc.Format;
        allocation.Usage = desc.Usage;
        allocation.MipLevels = desc.MipLevels;
        allocation.ArrayLayers = desc.ArrayLayers;
        allocation.VulkanImage = image;
        allocation.VulkanImageView = imageView;
        allocation.VulkanMemory = imageMemory;
        
        uint64_t handle = NextHandleId++;
        Allocations[handle] = allocation;
        TotalMemory += memRequirements.size;
        
        ResourceHandle result_handle;
        result_handle.Handle = handle;
        return result_handle;
    }

    void VulkanImageManager::DestroyImage(ResourceHandle handle)
    {
        if (!handle.IsValid()) return;
        
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return;
        
        VkDevice device = VulkanCore::GetInstance().GetDevice();
        
        VkImage image = static_cast<VkImage>(it->second.VulkanImage);
        VkImageView imageView = static_cast<VkImageView>(it->second.VulkanImageView);
        VkDeviceMemory memory = static_cast<VkDeviceMemory>(it->second.VulkanMemory);
        
        vkDestroyImageView(device, imageView, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, memory, nullptr);
    
        TotalMemory -= it->second.Width * it->second.Height * 4;  // Rough estimate
        Allocations.erase(it);
    }

    const ImageAllocation* VulkanImageManager::GetImageAllocation(ResourceHandle handle) const
    {
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return nullptr;
        return &it->second;
    }

    uint64_t VulkanImageManager::GetAllocatedMemory() const
    {
        return TotalMemory;
    }
