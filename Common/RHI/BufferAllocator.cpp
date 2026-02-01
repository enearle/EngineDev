#include "BufferAllocator.h"
#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"

BufferAllocator* BufferAllocator::Create()
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        return new VulkanBufferAllocator();
    else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        return new DirectX12BufferAllocator();
    else
        return nullptr;
}

BufferAllocation VulkanBufferAllocator::CreateBuffer(MemoryAccess memoryAccess, BufferUsage bufferUsage, uint64_t sizeInBytes, void* data)
{
    VulkanBufferData* vulkanBufferData = new VulkanBufferData();
    VkMemoryPropertyFlags flags = VulkanBufferUsage(bufferUsage);
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::GetInstance().GetPhysicalDevice();

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeInBytes;
    bufferInfo.usage = flags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;     // If more than one queue family access this resource
    
    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vulkanBufferData->Buffer);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer.");

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, vulkanBufferData->Buffer, &memoryRequirements);
    
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    uint32_t memoryTypeIndex = -1;
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        if ((memoryRequirements.memoryTypeBits & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
            memoryTypeIndex = i;
    if (memoryTypeIndex == -1)
        throw std::runtime_error("Failed to find suitable memory type.");
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = memoryTypeIndex;
    
    result = vkAllocateMemory(device, &allocInfo, nullptr, &vulkanBufferData->Memory);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory.");
    
    result = vkBindBufferMemory(device, vulkanBufferData->Buffer, vulkanBufferData->Memory, 0);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to bind vertex buffer memory.");
    
    if (data != nullptr)
    {
        void* mappedMemory;
        result = vkMapMemory(device, vulkanBufferData->Memory, 0, sizeInBytes, 0, &mappedMemory);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to map buffer memory.");
        
        memcpy(mappedMemory, data, sizeInBytes);
        vkUnmapMemory(device, vulkanBufferData->Memory);
    }
    
    BufferAllocation allocation;
    allocation.Buffer = vulkanBufferData;
    return allocation;

}

void VulkanBufferAllocator::TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkCommandBuffer commandBuffer = VulkanCore::GetInstance().GetTransferCommandBuffer();
    VkQueue commandQueue = VulkanCore::GetInstance().GetGraphicsQueue();
    VkCommandPool commandPool = VulkanCore::GetInstance().GetCommandPool();
    VkFence fence = VulkanCore::GetInstance().GetTransferFence();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording transfer command buffer.");


    if (fence != VK_NULL_HANDLE)
    {
        vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &fence);
    }

    VkImageMemoryBarrier imageMemoryBarrier = {};
    imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    imageMemoryBarrier.oldLayout = oldLayout;                                  // starting layout
    imageMemoryBarrier.newLayout = newLayout;                                  // layout to transition to
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;          // starting queue family
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;          // queue family to transition to
    imageMemoryBarrier.image = image;                                          // image to transition
    imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;// aspect to transition
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0;                      // starting mip level
    imageMemoryBarrier.subresourceRange.levelCount = 1;                        // number of mip levels
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;                    // starting array layer
    imageMemoryBarrier.subresourceRange.layerCount = 1;                        // number of array layers

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_NONE;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT; 
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
        throw std::invalid_argument("Unsupported layout transition.");
    
    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &imageMemoryBarrier
        );
    
    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to end recording transfer command buffer.");
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    result = vkQueueSubmit(commandQueue, 1, &submitInfo, fence);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to submit transfer command buffer to queue.");
    
    if (fence == VK_NULL_HANDLE)
        vkQueueWaitIdle(commandQueue);

}