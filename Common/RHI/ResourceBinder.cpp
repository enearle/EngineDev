#include "ResourceBinder.h"
#include "BufferManager.h"
#include "ImageManager.h"
#include "ResourceManager.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"Buffer
#include "../Windows/Win32ErrorHandler.h"

using namespace Win32ErrorHandler;
void D3DResourceBinder::BindBuffer(uint32_t slot, ResourceHandle buffer, uint32_t offset, uint32_t size)
    {
        if (!buffer.IsValid()) return;
        
        const BufferAllocation* allocation = ResourceManager::GetInstance().GetBufferManager()->GetBufferAllocation(buffer);
        if (!allocation) return;
        
        ResourceBinding binding{};
        binding.Slot = slot;
        binding.Resource = buffer;
        binding.Offset = offset;
        binding.Size = size > 0 ? size : static_cast<uint32_t>(allocation->Size);
        
        PendingBindings.push_back(binding);
    }

    void D3DResourceBinder::BindImage(uint32_t slot, ResourceHandle image)
    {
        if (!image.IsValid()) return;
        
        const ImageAllocation* allocation = ResourceManager::GetInstance().GetImageManager()->GetImageAllocation(image);
        if (!allocation) return;
        
        ResourceBinding binding{};
        binding.Slot = slot;
        binding.Resource = image;
        
        PendingBindings.push_back(binding);
    }

    void D3DResourceBinder::CommitBindings()
    {
        auto& d3dCore = D3DCore::GetInstance();
        auto cmdList = d3dCore.GetCommandList();
        auto bufferMgr = ResourceManager::GetInstance().GetBufferManager();
        auto imageMgr = ResourceManager::GetInstance().GetImageManager();
        
        for (const auto& binding : PendingBindings)
        {
            const BufferAllocation* bufferAlloc = bufferMgr->GetBufferAllocation(binding.Resource);
            const ImageAllocation* imageAlloc = imageMgr->GetImageAllocation(binding.Resource);
            
            if (bufferAlloc)
            {
                ID3D12Resource* resource = static_cast<ID3D12Resource*>(bufferAlloc->D3D12Resource);
                
                if ((bufferAlloc->Usage & BufferUsage::VertexBuffer) != 0)
                {
                    D3D12_VERTEX_BUFFER_VIEW vbv{};
                    vbv.BufferLocation = resource->GetGPUVirtualAddress() + binding.Offset;
                    vbv.SizeInBytes = binding.Size;
                    vbv.StrideInBytes = 0;  // Set by application
                    
                    cmdList->IASetVertexBuffers(binding.Slot, 1, &vbv);
                }
                else if ((bufferAlloc->Usage & BufferUsage::IndexBuffer) != 0)
                {
                    D3D12_INDEX_BUFFER_VIEW ibv{};
                    ibv.BufferLocation = resource->GetGPUVirtualAddress() + binding.Offset;
                    ibv.SizeInBytes = binding.Size;
                    ibv.Format = DXGI_FORMAT_R32_UINT;  // Could be parameterized
                    
                    cmdList->IASetIndexBuffer(&ibv);
                }
                else if ((bufferAlloc->Usage & BufferUsage::UniformBuffer) != 0)
                {
                    // Constant buffers bound via root signature
                    cmdList->SetGraphicsRootConstantBufferView(binding.Slot, resource->GetGPUVirtualAddress() + binding.Offset);
                }
            }
            else if (imageAlloc)
            {
                // Texture binding would go through root signature descriptors
                // This is simplified; full implementation needs descriptor tables
            }
        }
        
        PendingBindings.clear();
    }

    void D3DResourceBinder::Reset()
    {
        PendingBindings.clear();
    }


    void VulkanResourceBinder::BindBuffer(uint32_t slot, ResourceHandle buffer, uint32_t offset, uint32_t size)
    {
        if (!buffer.IsValid()) return;
        
        const BufferAllocation* allocation = ResourceManager::GetInstance().GetBufferManager()->GetBufferAllocation(buffer);
        if (!allocation) return;
        
        ResourceBinding binding{};
        binding.Slot = slot;
        binding.Resource = buffer;
        binding.Offset = offset;
        binding.Size = size > 0 ? size : static_cast<uint32_t>(allocation->Size);
        
        PendingBindings.push_back(binding);
    }

    void VulkanResourceBinder::BindImage(uint32_t slot, ResourceHandle image)
    {
        if (!image.IsValid()) return;
        
        const ImageAllocation* allocation = ResourceManager::GetInstance().GetImageManager()->GetImageAllocation(image);
        if (!allocation) return;
        
        ResourceBinding binding{};
        binding.Slot = slot;
        binding.Resource = image;
        
        PendingBindings.push_back(binding);
    }

    void VulkanResourceBinder::CommitBindings()
    {
        auto& vulkanCore = VulkanCore::GetInstance();
        VkCommandBuffer cmdBuffer = vulkanCore.GetCommandBuffer();
        auto bufferMgr = ResourceManager::GetInstance().GetBufferManager();
        auto imageMgr = ResourceManager::GetInstance().GetImageManager();
        
        DescriptorWrites.clear();
        
        for (const auto& binding : PendingBindings)
        {
            const BufferAllocation* bufferAlloc = bufferMgr->GetBufferAllocation(binding.Resource);
            const ImageAllocation* imageAlloc = imageMgr->GetImageAllocation(binding.Resource);
            
            if (bufferAlloc)
            {
                VkBuffer buffer = static_cast<VkBuffer>(bufferAlloc->VulkanBuffer);
                
                if ((bufferAlloc->Usage & BufferUsage::VertexBuffer) != 0)
                {
                    VkDeviceSize offset_size = binding.Offset;
                    vkCmdBindVertexBuffers(cmdBuffer, binding.Slot, 1, &buffer, &offset_size);
                }
                else if ((bufferAlloc->Usage & BufferUsage::IndexBuffer) != 0)
                {
                    vkCmdBindIndexBuffer(cmdBuffer, buffer, binding.Offset, VK_INDEX_TYPE_UINT32);
                }
                else if ((bufferAlloc->Usage & BufferUsage::UniformBuffer) != 0)
                {
                    // Uniform buffers are typically bound through descriptor sets
                    // This is a simplified binding - full implementation needs descriptor sets
                }
            }
            else if (imageAlloc)
            {
                // Image binding through descriptor sets
                VkImageView imageView = static_cast<VkImageView>(imageAlloc->VulkanImageView);
                
                // Simplified - would need full descriptor set management
                // Full implementation requires VkDescriptorSet updates
            }
        }
        
        PendingBindings.clear();
    }

    void VulkanResourceBinder::Reset()
    {
        PendingBindings.clear();
        DescriptorWrites.clear();
    }
