#include "BufferManager.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../Windows/Win32ErrorHandler.h"

using namespace Win32ErrorHandler;

ResourceHandle D3DBufferManager::CreateBuffer(const BufferDesc& desc)
    {
        auto& d3dCore = D3DCore::GetInstance();
        auto device = d3dCore.GetDevice();
        
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = desc.Size;
        resourceDesc.Height = 1;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        resourceDesc.SampleDesc.Count = 1;
        resourceDesc.SampleDesc.Quality = 0;
        resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        
        // Determine resource flags
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if ((desc.Usage & BufferUsage::StorageBuffer) != 0)
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        resourceDesc.Flags = flags;
        
        // Determine heap type
        D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
        if ((desc.Access & MemoryAccess::CPUWrite) != 0)
            heapType = D3D12_HEAP_TYPE_UPLOAD;
        else if ((desc.Access & MemoryAccess::CPURead) != 0)
            heapType = D3D12_HEAP_TYPE_READBACK;
        
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = heapType;
        heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        
        ComPtr<ID3D12Resource> resource;
        device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&resource)
        ) >> ERROR_HANDLER;
        
        BufferAllocation allocation = {};
        allocation.Size = desc.Size;
        allocation.Usage = desc.Usage;
        allocation.Access = desc.Access;
        allocation.D3D12Resource = resource.Get();
        allocation.GPUAddress = resource->GetGPUVirtualAddress();
        
        // Map if CPU-accessible
        if ((desc.Access & (MemoryAccess::CPUWrite | MemoryAccess::CPURead)) != 0)
        {
            resource->Map(0, nullptr, &allocation.CPUAddress) >> ERROR_HANDLER;
        }
        
        // Upload initial data if provided
        if (desc.InitialData && allocation.CPUAddress)
        {
            memcpy(allocation.CPUAddress, desc.InitialData, desc.Size);
        }
        
        resource.Detach();  // Release COM reference, we hold raw pointer
        
        uint64_t handle = NextHandleId++;
        Allocations[handle] = allocation;
        TotalMemory += desc.Size;
        
        ResourceHandle result;
        result.Handle = handle;
        return result;
    }

    void D3DBufferManager::DestroyBuffer(ResourceHandle handle)
    {
        if (!handle.IsValid()) return;
        
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return;
        
        auto resource = static_cast<ID3D12Resource*>(it->second.D3D12Resource);
        if (it->second.CPUAddress)
            resource->Unmap(0, nullptr);
        
        resource->Release();
        TotalMemory -= it->second.Size;
        Allocations.erase(it);
    }

    void* D3DBufferManager::MapBuffer(ResourceHandle handle)
    {
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return nullptr;
        return it->second.CPUAddress;
    }

    void D3DBufferManager::UnmapBuffer(ResourceHandle handle)
    {
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return;
        
        auto resource = static_cast<ID3D12Resource*>(it->second.D3D12Resource);
        if (it->second.CPUAddress)
            resource->Unmap(0, nullptr);
    }

    const BufferAllocation* D3DBufferManager::GetBufferAllocation(ResourceHandle handle) const
    {
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return nullptr;
        return &it->second;
    }

    void D3DBufferManager::UpdateBuffer(ResourceHandle handle, const void* data, uint64_t size, uint64_t offset)
    {
        void* mapped = MapBuffer(handle);
        if (!mapped) return;
        
        memcpy(static_cast<char*>(mapped) + offset, data, size);
    }

    uint64_t D3DBufferManager::GetAllocatedMemory() const
    {
        return TotalMemory;
    }

uint32_t VulkanBufferManager::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
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
        
        throw std::runtime_error("Failed to find suitable memory type for buffer");
    }

    ResourceHandle VulkanBufferManager::CreateBuffer(const BufferDesc& desc)
    {
        auto& vulkanCore = VulkanCore::GetInstance();
        VkDevice device = vulkanCore.GetDevice();
        
        // Determine buffer usage flags
        VkBufferUsageFlags usageFlags = 0;
        if ((desc.Usage & BufferUsage::VertexBuffer) != 0)
            usageFlags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        if ((desc.Usage & BufferUsage::IndexBuffer) != 0)
            usageFlags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        if ((desc.Usage & BufferUsage::UniformBuffer) != 0)
            usageFlags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        if ((desc.Usage & BufferUsage::StorageBuffer) != 0)
            usageFlags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        if ((desc.Usage & BufferUsage::TransferSrcBuffer) != 0)
            usageFlags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        if ((desc.Usage & BufferUsage::TransferDstBuffer) != 0)
            usageFlags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = desc.Size;
        bufferInfo.usage = usageFlags;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
        VkBuffer buffer = VK_NULL_HANDLE;
        VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan buffer");
        
        // Get memory requirements
        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, buffer, &memRequirements);
        
        // Determine memory property flags
        VkMemoryPropertyFlags memoryProperties = 0;
        bool isCPUAccessible = false;
        
        if ((desc.Access & MemoryAccess::CPUWrite) != 0)
        {
            memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            isCPUAccessible = true;
        }
        else if ((desc.Access & MemoryAccess::CPURead) != 0)
        {
            memoryProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
            isCPUAccessible = true;
        }
        else
        {
            memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
        
        // Allocate memory
        uint32_t memoryType = FindMemoryType(memRequirements.memoryTypeBits, memoryProperties);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = memoryType;
        
        VkDeviceMemory bufferMemory = VK_NULL_HANDLE;
        result = vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory);
        if (result != VK_SUCCESS)
        {
            vkDestroyBuffer(device, buffer, nullptr);
            throw std::runtime_error("Failed to allocate Vulkan buffer memory");
        }
        
        // Bind memory to buffer
        result = vkBindBufferMemory(device, buffer, bufferMemory, 0);
        if (result != VK_SUCCESS)
        {
            vkFreeMemory(device, bufferMemory, nullptr);
            vkDestroyBuffer(device, buffer, nullptr);
            throw std::runtime_error("Failed to bind Vulkan buffer memory");
        }
        
        BufferAllocation allocation{};
        allocation.Size = desc.Size;
        allocation.Usage = desc.Usage;
        allocation.Access = desc.Access;
        allocation.VulkanBuffer = buffer;
        allocation.VulkanMemory = bufferMemory;
        allocation.GPUAddress = 0;  // Vulkan doesn't expose GPU addresses directly in the same way
        
        // Map memory if CPU-accessible
        if (isCPUAccessible)
        {
            result = vkMapMemory(device, bufferMemory, 0, desc.Size, 0, &allocation.CPUAddress);
            if (result != VK_SUCCESS)
            {
                vkFreeMemory(device, bufferMemory, nullptr);
                vkDestroyBuffer(device, buffer, nullptr);
                throw std::runtime_error("Failed to map Vulkan buffer memory");
            }
        }
        
        // Upload initial data if provided
        if (desc.InitialData && allocation.CPUAddress)
        {
            memcpy(allocation.CPUAddress, desc.InitialData, desc.Size);
            
            // Flush if not coherent
            if ((memoryProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
            {
                VkMappedMemoryRange flushRange{};
                flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
                flushRange.memory = bufferMemory;
                flushRange.offset = 0;
                flushRange.size = desc.Size;
                vkFlushMappedMemoryRanges(device, 1, &flushRange);
            }
        }
        else if (desc.InitialData && !allocation.CPUAddress)
        {
            // Need staging buffer for GPU-local memory with initial data
            BufferDesc stagingDesc{};
            stagingDesc.Size = desc.Size;
            stagingDesc.Usage = BufferUsage::TransferSrcBuffer;
            stagingDesc.Access = MemoryAccess::CPUWrite;
            stagingDesc.InitialData = desc.InitialData;
            
            ResourceHandle stagingHandle = CreateBuffer(stagingDesc);
            
            // Copy from staging to device local buffer
            // This would require command buffer recording - defer for now
            // In practice, you'd want a dedicated upload queue/thread
        }
        
        uint64_t handle = NextHandleId++;
        Allocations[handle] = allocation;
        TotalMemory += desc.Size;
        
        ResourceHandle result_handle;
        result_handle.Handle = handle;
        return result_handle;
    }

    void VulkanBufferManager::DestroyBuffer(ResourceHandle handle)
    {
        if (!handle.IsValid()) return;
        
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return;
        
        VkDevice device = VulkanCore::GetInstance().GetDevice();
        
        VkBuffer buffer = static_cast<VkBuffer>(it->second.VulkanBuffer);
        VkDeviceMemory memory = static_cast<VkDeviceMemory>(it->second.VulkanMemory);
        
        if (it->second.CPUAddress)
            vkUnmapMemory(device, memory);
        
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
        
        TotalMemory -= it->second.Size;
        Allocations.erase(it);
    }

    void* VulkanBufferManager::MapBuffer(ResourceHandle handle)
    {
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return nullptr;
        
        if (it->second.CPUAddress) return it->second.CPUAddress;
    
        VkDeviceMemory memory = static_cast<VkDeviceMemory>(it->second.VulkanMemory);
        
        void* mapped = nullptr;
        VkResult result = vkMapMemory(VulkanCore::GetInstance().GetDevice(), memory, 0, it->second.Size, 0, &mapped);
        
        if (result == VK_SUCCESS)
        {
            it->second.CPUAddress = mapped;
            return mapped;
        }
        
        return nullptr;
    }

    void VulkanBufferManager::UnmapBuffer(ResourceHandle handle)
    {
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end() || !it->second.CPUAddress) return;
    
        VkDeviceMemory memory = static_cast<VkDeviceMemory>(it->second.VulkanMemory);
        
        vkUnmapMemory(VulkanCore::GetInstance().GetDevice(), memory);
        it->second.CPUAddress = nullptr;
    }

    const BufferAllocation* VulkanBufferManager::GetBufferAllocation(ResourceHandle handle) const
    {
        auto it = Allocations.find(handle.Handle);
        if (it == Allocations.end()) return nullptr;
        return &it->second;
    }

    void VulkanBufferManager::UpdateBuffer(ResourceHandle handle, const void* data, uint64_t size, uint64_t offset)
    {
        void* mapped = MapBuffer(handle);
        if (!mapped) return;
        
        memcpy(static_cast<char*>(mapped) + offset, data, size);
        
        auto it = Allocations.find(handle.Handle);
        if (it != Allocations.end())
        {
            // Flush if not coherent
            VkDeviceMemory memory = static_cast<VkDeviceMemory>(it->second.VulkanMemory);
            // Check if coherent - simplified, you may want to track this per allocation
            VkMappedMemoryRange flushRange{};
            flushRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
            flushRange.memory = memory;
            flushRange.offset = offset;
            flushRange.size = size;
            vkFlushMappedMemoryRanges(VulkanCore::GetInstance().GetDevice(), 1, &flushRange);
        }
    }

    uint64_t VulkanBufferManager::GetAllocatedMemory() const
    {
        return TotalMemory;
    }
