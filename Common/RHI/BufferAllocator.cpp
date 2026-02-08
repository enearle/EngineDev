#include "BufferAllocator.h"

#include <map>

#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../DirectX12/D3D12Structs.h"
#include "../Data/BitPool.h"


using namespace Win32ErrorHandler;
using namespace D3D12Structs;
BufferAllocator* BufferAllocator::Create()
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        return new VulkanBufferAllocator();
    else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        return new DirectX12BufferAllocator();
    else
        return nullptr;
}


//================================================//
// Vulkan                                         //
//================================================//

VulkanBufferAllocator::VulkanBufferAllocator()
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::GetInstance().GetPhysicalDevice();
    
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 64 * 1024 * 1024; // 128MB for descriptors
    bufferInfo.usage = VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
                      VK_BUFFER_USAGE_SAMPLER_DESCRIPTOR_BUFFER_BIT_EXT |
                        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        
    vkCreateBuffer(device, &bufferInfo, nullptr, &DescriptorBuffer);
    
    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, DescriptorBuffer, &memReqs);
    
    VkMemoryAllocateFlagsInfo allocFlags = {};
    allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlags.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(
        physicalDevice,
        memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | 
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT
    );
    allocInfo.pNext = &allocFlags;
    
    vkAllocateMemory(device, &allocInfo, nullptr, &DescriptorBufferMemory);
    
    vkBindBufferMemory(device, DescriptorBuffer, DescriptorBufferMemory, 0);
    
    vkMapMemory(device, DescriptorBufferMemory, 0, VK_WHOLE_SIZE, 0, &DescriptorBufferMapped);
    
    VkBufferDeviceAddressInfo addressInfo = {};
    addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addressInfo.buffer = DescriptorBuffer;
    DescriptorBufferAddress = vkGetBufferDeviceAddress(device, &addressInfo);
    
    SampledImageStride = VulkanCore::GetInstance().GetDescriptorBufferProperties().sampledImageDescriptorSize;
    StorageImageStride = VulkanCore::GetInstance().GetDescriptorBufferProperties().storageImageDescriptorSize;
    UniformBufferStride = VulkanCore::GetInstance().GetDescriptorBufferProperties().uniformBufferDescriptorSize;
    StorageBufferStride = VulkanCore::GetInstance().GetDescriptorBufferProperties().storageBufferDescriptorSize;

    uint32_t currentOffset = 0;
    SampledImagePool = new BitPool();
    SampledImagePool->Initialize(currentOffset, SampledImageStride, SampledImagePoolSize);
    currentOffset += SampledImagePoolSize * SampledImageStride;
    StorageImagePool = new BitPool();
    StorageImagePool->Initialize(currentOffset, StorageImageStride, StorageImagePoolSize);
    currentOffset += StorageImagePoolSize * StorageImageStride;
    UniformBufferPool = new BitPool();
    UniformBufferPool->Initialize(currentOffset, UniformBufferStride, UniformBufferPoolSize);
    currentOffset += UniformBufferPoolSize * UniformBufferStride;
    StorageBufferPool = new BitPool();
    StorageBufferPool->Initialize(currentOffset, StorageBufferStride, StorageBufferPoolSize);
    currentOffset += StorageBufferPoolSize * StorageBufferStride;

}

VkDeviceAddress VulkanBufferAllocator::AllocateDescriptor(VkDescriptorGetInfoEXT* descriptorInfo, DescriptorType type)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    PFN_vkGetDescriptorEXT vkGetDescriptorEXT_Fn = VulkanCore::GetInstance().GetVkGetDescriptorEXT();
    size_t stride = 0;
    BitPool* pool = nullptr;
    
    switch (type)
    {
    case SampledImage:
        pool = SampledImagePool;
        stride = SampledImageStride;
        break;
    case StorageImage:
        pool = StorageImagePool;
        stride = StorageImageStride;
        break;
    case UniformBuffer:
        pool = UniformBufferPool;
        stride = UniformBufferStride;
        break;
    case StorageBuffer:
        pool = StorageBufferPool;
        stride = StorageBufferStride;
        break;
    default:
        throw std::runtime_error("Invalid descriptor type");
    }
    
    if (!pool)
        throw std::runtime_error("Descriptor pool not initialized");
    
    size_t offset = pool->Allocate();
    
    uint8_t* descriptorLocation = (uint8_t*)DescriptorBufferMapped + offset;
    vkGetDescriptorEXT_Fn(device, descriptorInfo, stride, descriptorLocation);
    
    VkDeviceAddress descriptorAddress = DescriptorBufferAddress + offset;
    return descriptorAddress;
}

void VulkanBufferAllocator::FreeDescriptor(VkDeviceAddress address, DescriptorType type)
{
    if (!address)
        throw std::invalid_argument("Cannot free null descriptor address");
    
    size_t offset = address - DescriptorBufferAddress;
    
    if (offset >= (64 * 1024 * 1024))
        throw std::invalid_argument("Address not within descriptor buffer");
    
    BitPool* pool = nullptr;
    
    switch (type)
    {
    case SampledImage:
        pool = SampledImagePool;
        break;
    case StorageImage:
        pool = StorageImagePool;
        break;
    case UniformBuffer:
        pool = UniformBufferPool;
        break;
    case StorageBuffer:
        pool = StorageBufferPool;
        break;
    default:
        throw std::runtime_error("Invalid descriptor type");
    }
    
    if (!pool)
        throw std::runtime_error("Descriptor pool not initialized");
    
    pool->Free(offset);
}

uint64_t VulkanBufferAllocator::CreateBuffer(BufferDesc bufferDesc)
{
    VulkanBufferData* vulkanBufferData = new VulkanBufferData();
    VkBufferUsageFlags bufferFlags = VulkanBufferUsage(bufferDesc.Usage);
    VkMemoryPropertyFlags memoryFlags = VulkanMemoryType(bufferDesc.Access);
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::GetInstance().GetPhysicalDevice();

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferDesc.Size;
    bufferInfo.usage = bufferFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // If more than one queue family can access this resource
    
    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vulkanBufferData->Buffer);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer.");

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, vulkanBufferData->Buffer, &memoryRequirements);
    
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, memoryFlags);
    
    result = vkAllocateMemory(device, &allocInfo, nullptr, &vulkanBufferData->Memory);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory.");
    
    result = vkBindBufferMemory(device, vulkanBufferData->Buffer, vulkanBufferData->Memory, 0);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to bind vertex buffer memory.");

    bool isHostVisible = (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    
    void* mappedAddress = nullptr;

    if (bufferDesc.InitialData != nullptr)
    {
        if (isHostVisible)
        {
            result = vkMapMemory(device, vulkanBufferData->Memory, 0, bufferDesc.Size, 0, &mappedAddress);
            if (result != VK_SUCCESS)
                throw std::runtime_error("Failed to map buffer memory.");
            
            memcpy(mappedAddress, bufferDesc.InitialData, bufferDesc.Size);
        }
        else
        {
            // Staging
            CopyToDeviceLocalBuffer(vulkanBufferData->Buffer, bufferDesc.InitialData, bufferDesc.Size);
        }
    }
    
    BufferAllocation allocation;
    allocation.Buffer = vulkanBufferData;
    allocation.Address = mappedAddress;
    allocation.Size = bufferDesc.Size;
    allocation.Usage = bufferDesc.Usage;
    allocation.Access = bufferDesc.Access;
    allocation.IsMapped = isHostVisible;
    
    // Create descriptor for shader-accessible buffers
    if (bufferDesc.Type == BufferType::Constant || bufferDesc.Type == BufferType::ShaderStorage)
    {
        DescriptorType descriptorType;
        VkDescriptorType vkDescriptorType;
        
        if (bufferDesc.Type == BufferType::Constant)
        {
            descriptorType = DescriptorType::UniformBuffer;
            vkDescriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        else
        {
            descriptorType = DescriptorType::StorageBuffer;
            vkDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        
        // Get device address for the buffer
        VkBufferDeviceAddressInfo addressInfo{};
        addressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addressInfo.buffer = vulkanBufferData->Buffer;
        
        VkDeviceAddress bufferAddress = vkGetBufferDeviceAddress(device, &addressInfo);
        
        VkDescriptorAddressInfoEXT bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_ADDRESS_INFO_EXT;
        bufferInfo.address = bufferAddress;
        bufferInfo.range = bufferDesc.Size;
        bufferInfo.format = VK_FORMAT_UNDEFINED;
        
        VkDescriptorGetInfoEXT descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = vkDescriptorType;
        descriptorInfo.data.pUniformBuffer = &bufferInfo;
        
        VkDeviceAddress descriptorAddress = AllocateDescriptor(&descriptorInfo, descriptorType);
        
        allocation.Descriptor = descriptorAddress;
        allocation.DescriptorType = static_cast<uint8_t>(descriptorType);
    }
    else
    {
        // Vertex/Index buffers don't use descriptors
        allocation.Descriptor = 0;
        allocation.DescriptorType = 0;
    }
    
    return CacheBuffer(allocation);
}

void VulkanBufferAllocator::CopyToDeviceLocalBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::GetInstance().GetPhysicalDevice();
    VkCommandBuffer commandBuffer = VulkanCore::GetInstance().GetTransferCommandBuffer();
    VkQueue transferQueue = VulkanCore::GetInstance().GetGraphicsQueue();
    VkFence transferFence = VulkanCore::GetInstance().GetTransferFence();
    
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = size;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer;
    VkResult result = vkCreateBuffer(device, &stagingBufferInfo, nullptr, &stagingBuffer);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create staging buffer.");
    
    VkMemoryRequirements stagingMemoryReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingMemoryReqs);

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    
    VkMemoryPropertyFlags stagingFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkMemoryAllocateInfo stagingAllocInfo = {};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemoryReqs.size;
    stagingAllocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, stagingMemoryReqs.memoryTypeBits, stagingFlags);

    VkDeviceMemory stagingMemory;
    result = vkAllocateMemory(device, &stagingAllocInfo, nullptr, &stagingMemory);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate staging buffer memory.");

    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
    
    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, size, 0, &mappedData);
    memcpy(mappedData, srcData, size);
    vkUnmapMemory(device, stagingMemory);
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;

    vkCmdCopyBuffer(commandBuffer, stagingBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(transferQueue, 1, &submitInfo, transferFence);
    
    vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &transferFence);
    vkResetCommandBuffer(commandBuffer, 0);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

uint64_t VulkanBufferAllocator::CreateImage(ImageDesc imageDesc)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::GetInstance().GetPhysicalDevice();
    
    VkBufferCreateInfo stagingBufferInfo = {};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = imageDesc.Size;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer;
    VkResult result = vkCreateBuffer(device, &stagingBufferInfo, nullptr, &stagingBuffer);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create image staging buffer.");
    
    VkMemoryRequirements stagingMemoryReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingMemoryReqs);

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    VkMemoryPropertyFlags stagingFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    
    VkMemoryAllocateInfo stagingAllocInfo = {};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingMemoryReqs.size;
    stagingAllocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, stagingMemoryReqs.memoryTypeBits, stagingFlags);

    VkDeviceMemory stagingMemory;
    result = vkAllocateMemory(device, &stagingAllocInfo, nullptr, &stagingMemory);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image staging buffer memory.");
    
    vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
    
    void* mappedData;
    vkMapMemory(device, stagingMemory, 0, imageDesc.Size, 0, &mappedData);
    memcpy(mappedData, imageDesc.InitialData, imageDesc.Size);
    vkUnmapMemory(device, stagingMemory);

    VulkanImageData* vulkanImageData = new VulkanImageData();
    vulkanImageData->ImageHandle = CreateVulkanImage(imageDesc, &vulkanImageData->Memory);
    
    TransitionImageLayout(vulkanImageData->ImageHandle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    
    CopyBufferToImage(stagingBuffer, vulkanImageData->ImageHandle, imageDesc.Width, imageDesc.Height);
    
    TransitionImageLayout(vulkanImageData->ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vulkanImageData->ImageView = CreateVulkanImageView(vulkanImageData->ImageHandle, imageDesc);
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    ImageAllocation allocation;
    allocation.Image = vulkanImageData;
    allocation.Desc = imageDesc;
    
    // Create descriptor for shader-accessible images
    if (imageDesc.Type == ImageType::Sampled || imageDesc.Type == ImageType::Storage)
    {
        DescriptorType descriptorType;
        VkDescriptorType vkDescriptorType;
        
        if (imageDesc.Type == ImageType::Sampled)
        {
            descriptorType = DescriptorType::SampledImage;
            vkDescriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        else
        {
            descriptorType = DescriptorType::StorageImage;
            vkDescriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = vulkanImageData->ImageView;
        imageInfo.imageLayout = (descriptorType == DescriptorType::SampledImage) 
            ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL 
            : VK_IMAGE_LAYOUT_GENERAL;
        
        if (vkDescriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            imageInfo.sampler = *VulkanCore::GetInstance().GetGenericSampler();
        }
        else
        {
            imageInfo.sampler = VK_NULL_HANDLE;
        }
        
        VkDescriptorGetInfoEXT descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT;
        descriptorInfo.type = vkDescriptorType;
        descriptorInfo.data.pSampledImage = &imageInfo;
        
        VkDeviceAddress descriptorAddress = AllocateDescriptor(&descriptorInfo, descriptorType);
        
        allocation.Descriptor = descriptorAddress;
        allocation.DescriptorType = static_cast<uint8_t>(descriptorType);
    }
    else
    {
        // RenderTarget and DepthStencil don't use descriptors
        allocation.Descriptor = 0;
        allocation.DescriptorType = 0;
    }
    
    return CacheImage(allocation);
}

VulkanBufferAllocator::~VulkanBufferAllocator()
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    
    delete SampledImagePool;
    delete StorageImagePool;
    delete UniformBufferPool;
    delete StorageBufferPool;
    
    if (DescriptorBuffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, DescriptorBuffer, nullptr);
    if (DescriptorBufferMemory != VK_NULL_HANDLE)
        vkFreeMemory(device, DescriptorBufferMemory, nullptr);
    
    for (auto& [handle, allocation] : AllocatedBuffers)
    {
        VulkanBufferData* bufferData = static_cast<VulkanBufferData*>(allocation.Buffer);
        if (bufferData)
        {
            vkDestroyBuffer(device, bufferData->Buffer, nullptr);
            vkFreeMemory(device, bufferData->Memory, nullptr);
            delete bufferData;
        }
    }
    AllocatedBuffers.clear();
    
    for (auto& [handle, allocation] : AllocatedImages)
    {
        VulkanImageData* imageData = static_cast<VulkanImageData*>(allocation.Image);
        if (imageData)
        {
            vkDestroyImageView(device, imageData->ImageView, nullptr);
            vkDestroyImage(device, imageData->ImageHandle, nullptr);
            vkFreeMemory(device, imageData->Memory, nullptr);
            delete imageData;
        }
    }
    AllocatedImages.clear();
}

void VulkanBufferAllocator::FreeBuffer(uint64_t id)
{
    FreeDescriptor(AllocatedBuffers[id].Descriptor, static_cast<DescriptorType>(AllocatedBuffers[id].DescriptorType));
}

void VulkanBufferAllocator::FreeImage(uint64_t id)
{
    FreeDescriptor(AllocatedImages[id].Descriptor, static_cast<DescriptorType>(AllocatedImages[id].DescriptorType));
}

void VulkanBufferAllocator::CopyBufferToImage(VkBuffer stagingBuffer, VkImage dstImage, uint32_t width, uint32_t height)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkCommandBuffer commandBuffer = VulkanCore::GetInstance().GetTransferCommandBuffer();
    VkQueue transferQueue = VulkanCore::GetInstance().GetGraphicsQueue();
    VkFence transferFence = VulkanCore::GetInstance().GetTransferFence();
    
    VkResult fenceStatus = vkGetFenceStatus(device, transferFence);
    if (fenceStatus == VK_NOT_READY)
    {
        VkResult waitRes = vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
        if (waitRes != VK_SUCCESS)
            throw std::runtime_error("vkWaitForFences failed in CopyBufferToImage().");
    }
    else if (fenceStatus != VK_SUCCESS)
    {
        throw std::runtime_error("vkGetFenceStatus returned an error in CopyBufferToImage().");
    }
    
    vkResetFences(device, 1, &transferFence);
    vkResetCommandBuffer(commandBuffer, 0);
    
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);
    
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, dstImage, 
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    vkEndCommandBuffer(commandBuffer);
    
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkResult submitRes = vkQueueSubmit(transferQueue, 1, &submitInfo, transferFence);
    if (submitRes != VK_SUCCESS)
        throw std::runtime_error("vkQueueSubmit failed in CopyBufferToImage().");

    VkResult waitRes = vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
    if (waitRes != VK_SUCCESS)
        throw std::runtime_error("vkWaitForFences failed after submit in CopyBufferToImage().");
}

void VulkanBufferAllocator::TransitionImageLayout(VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkCommandBuffer commandBuffer = VulkanCore::GetInstance().GetTransferCommandBuffer();
    VkQueue commandQueue = VulkanCore::GetInstance().GetGraphicsQueue();
    VkFence transferFence = VulkanCore::GetInstance().GetTransferFence();

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VkResult fenceStatus = vkGetFenceStatus(device, transferFence);
    if (fenceStatus == VK_NOT_READY)
    {
        VkResult waitRes = vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
        if (waitRes != VK_SUCCESS)
            throw std::runtime_error("vkWaitForFences failed in CopyBufferToImage().");
    }
    else if (fenceStatus != VK_SUCCESS)
    {
        throw std::runtime_error("vkGetFenceStatus returned an error in CopyBufferToImage().");
    }
    
    vkResetFences(device, 1, &transferFence);
    vkResetCommandBuffer(commandBuffer, 0);
    
    VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording transfer command buffer.");

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

    VkResult submitRes = vkQueueSubmit(commandQueue, 1, &submitInfo, transferFence);
    if (submitRes != VK_SUCCESS)
        throw std::runtime_error("vkQueueSubmit failed in CopyBufferToImage().");

    VkResult waitRes = vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
    if (waitRes != VK_SUCCESS)
        throw std::runtime_error("vkWaitForFences failed after submit in CopyBufferToImage().");
}

VkImage VulkanBufferAllocator::CreateVulkanImage(ImageDesc imageDesc, VkDeviceMemory* imageMemory)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::GetInstance().GetPhysicalDevice();
    VkMemoryPropertyFlags memoryFlags = VulkanMemoryType(imageDesc.Access);
    // Create image
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;                 // Image dimension type (1d, 2d, 3d)
    imageInfo.extent.width = imageDesc.Width;               // Width of image
    imageInfo.extent.height = imageDesc.Height;             // Height of image
    imageInfo.extent.depth = 1;                             // Depth (if 3d)
    imageInfo.mipLevels = 1;                                // Number of mipmap levels
    imageInfo.arrayLayers = 1;                              // Number of indices in the image array
    imageInfo.format = VulkanFormat(imageDesc.Format);      // Image format structure of data and colour space
                                                            // Tiling of the image (linear, optimal) how image data is arranged in memory for optimal reading
    imageInfo.tiling = imageDesc.TilingLinear ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;    // Layout of image data on creation
    imageInfo.usage = VulkanImageUsage(imageDesc.Usage); // Image usage flags
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;              // Number of samples for multisampling
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;      // Whether image can be shared between queues

    VkImage image;
    VkResult result = vkCreateImage(device, &imageInfo, nullptr, &image);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create image.");
    
    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image, &memoryRequirements);

    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memoryRequirements.memoryTypeBits, memoryFlags);

    result = vkAllocateMemory(device, &allocInfo, nullptr, imageMemory);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory.");
    
    result = vkBindImageMemory(device, image, *imageMemory, 0);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to bind image memory.");
    
    return image;
}

VkImageView VulkanBufferAllocator::CreateVulkanImageView(VkImage image, ImageDesc imageDesc)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;                                                 // Image to create view for
    viewInfo.viewType = VulkanImageViewType(imageDesc);                  // Essentially, the number of dimensions in the image data 1D, 2D, 3D, 2D_ARRAY, etc. 
    viewInfo.format = VulkanFormat(imageDesc.Format);                       // Colour data packing and colour space of the image
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;                  // Map RGBA data
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    
    viewInfo.subresourceRange.aspectMask = VulkanAspects(imageDesc.Format); // Which aspect of image to view colour, stencil, etc. 
    viewInfo.subresourceRange.baseMipLevel = 0;                             // Start mipmap level to view from
    viewInfo.subresourceRange.levelCount = imageDesc.MipLevels;             // Number of mipmap levels to view
    viewInfo.subresourceRange.baseArrayLayer = 0;                           // Start array level to view from
    viewInfo.subresourceRange.layerCount = imageDesc.ArrayLayers;           // Number of array levels to view

    VkImageView imageView = VK_NULL_HANDLE;
    VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view.");
    
    return imageView;
}

uint32_t VulkanBufferAllocator::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t allowdTypes, VkMemoryPropertyFlags flags)
{
    // Get properties of physical device memory
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    // Iterate through memory types to find one that matches the required properties
    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
        if ((allowdTypes & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & flags) == flags)
            return i;

    throw std::runtime_error("Failed to find suitable memory type for mesh vertex buffer.");
}

//================================================//
// DirectX 12                                     //
//================================================//

uint64_t DirectX12BufferAllocator::CreateBuffer(BufferDesc bufferDesc)
{
    ID3D12Device* device = D3DCore::GetInstance().GetDevice().Get();
    ID3D12GraphicsCommandList* cmdList = D3DCore::GetInstance().GetTransferCommandList().Get();

    ComPtr<ID3D12Resource> defaultBuffer;
    ComPtr<ID3D12Resource> uploadBuffer;
    
    BufferType finalType = bufferDesc.Type;
    
    CD3DX12_HEAP_PROPERTIES defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC bufferDesc_dx12 = CD3DX12_RESOURCE_DESC::Buffer(bufferDesc.Size);
    
    device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc_dx12,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(defaultBuffer.GetAddressOf())) >> ERROR_HANDLER;

    CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferDesc.Size);
    
    device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())) >> ERROR_HANDLER;
    
    D3DCore::GetInstance().DeferUploadBufferRelease(uploadBuffer);

    // Copy initial data if provided
    if (bufferDesc.InitialData != nullptr)
    {
        void* mappedData = nullptr;
        uploadBuffer->Map(0, nullptr, &mappedData);
        memcpy(mappedData, bufferDesc.InitialData, bufferDesc.Size);
        uploadBuffer->Unmap(0, nullptr);

        D3D12_SUBRESOURCE_DATA subResourceData = {};
        subResourceData.pData = bufferDesc.InitialData;
        subResourceData.RowPitch = bufferDesc.Size;
        subResourceData.SlicePitch = bufferDesc.Size;

        CD3DX12_RESOURCE_BARRIER transition1 = CD3DX12_RESOURCE_BARRIER::Transition(
            defaultBuffer.Get(), 
            D3D12_RESOURCE_STATE_COMMON, 
            D3D12_RESOURCE_STATE_COPY_DEST);
        cmdList->ResourceBarrier(1, &transition1);

        UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

        CD3DX12_RESOURCE_BARRIER transition2 = CD3DX12_RESOURCE_BARRIER::Transition(
            defaultBuffer.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, 
            D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList->ResourceBarrier(1, &transition2);
    }

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = defaultBuffer->GetGPUVirtualAddress();

    DX12BufferData* bufferData = new DX12BufferData();
    bufferData->Buffer = defaultBuffer.Get();
    bufferData->GPUAddress = gpuAddress;

    BufferAllocation allocation;
    allocation.Buffer = bufferData;
    allocation.Size = bufferDesc.Size;
    allocation.Address = reinterpret_cast<void*>(gpuAddress);
    allocation.Usage = bufferDesc.Usage;
    allocation.Access = bufferDesc.Access;
    allocation.Type = bufferDesc.Type;
    allocation.IsMapped = false;

    // Only create descriptor if needed
    if (bufferDesc.Type == BufferType::Constant)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE cbvHandle = AllocateDescriptor(CBV);
        
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation = gpuAddress;
        cbvDesc.SizeInBytes = static_cast<UINT>((bufferDesc.Size + 255) & ~255);  // Must be 256-byte aligned
        
        device->CreateConstantBufferView(&cbvDesc, cbvHandle);
        allocation.Descriptor = *reinterpret_cast<uint64_t*>(&cbvHandle);
    }
    else if (bufferDesc.Type == BufferType::ShaderStorage)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = AllocateDescriptor(SRV);
        
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(bufferDesc.Size / 4);  // Assuming 4-byte elements
        srvDesc.Buffer.StructureByteStride = 4;
        
        device->CreateShaderResourceView(defaultBuffer.Get(), &srvDesc, srvHandle);
        allocation.Descriptor = *reinterpret_cast<uint64_t*>(&srvHandle);
    }
    // Vertex and Index buffers don't need descriptors
    
    return CacheBuffer(allocation);
}

uint64_t DirectX12BufferAllocator::CreateImage(ImageDesc imageDesc)
{
    ID3D12Device* device = D3DCore::GetInstance().GetDevice().Get();
    ID3D12GraphicsCommandList* cmdList = D3DCore::GetInstance().GetTransferCommandList().Get();

    if (imageDesc.ArrayLayers != 1 || imageDesc.MipLevels != 1)
        throw std::runtime_error("DirectX12BufferAllocator::CreateImage currently supports only 1 layer and 1 mip (match Vulkan path later).");

    if (imageDesc.InitialData == nullptr || imageDesc.Size == 0)
        throw std::runtime_error("DirectX12BufferAllocator::CreateImage requires InitialData + Size (like Vulkan staging upload).");

    ComPtr<ID3D12Resource> imageResource;

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Alignment = 0;
    textureDesc.Width = imageDesc.Width;
    textureDesc.Height = imageDesc.Height;
    textureDesc.DepthOrArraySize = static_cast<UINT16>(imageDesc.ArrayLayers);
    textureDesc.MipLevels = static_cast<UINT16>(imageDesc.MipLevels);
    textureDesc.Format = DXFormat(imageDesc.Format);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = DXImageUsage(imageDesc.Usage);

    auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    
    device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(imageResource.GetAddressOf())) >> ERROR_HANDLER;
    
    ComPtr<ID3D12Resource> uploadBuffer;

    const UINT firstSubresource = 0;
    const UINT numSubresources = 1;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(imageResource.Get(), firstSubresource, numSubresources);

    auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    device->CreateCommittedResource(
        &uploadHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(uploadBuffer.GetAddressOf())) >> ERROR_HANDLER;
    
    D3DCore::GetInstance().DeferUploadBufferRelease(uploadBuffer);
    
    const uint32_t bytesPerPixel =
        (imageDesc.Format == Format::R8G8B8A8_UNORM || imageDesc.Format == Format::R8G8B8A8_UNORM_SRGB) ? 4u : 0u;

    if (bytesPerPixel == 0)
        throw std::runtime_error("CreateImage upload currently only implemented for R8G8B8A8(_SRGB). Add proper bpp/rowPitch handling for other formats.");

    const UINT64 expectedSize = static_cast<UINT64>(imageDesc.Width) * static_cast<UINT64>(imageDesc.Height) * bytesPerPixel;
    if (imageDesc.Size < expectedSize)
        throw std::runtime_error("ImageDesc::Size is smaller than expected for the provided Width/Height/Format.");

    D3D12_SUBRESOURCE_DATA subresource = {};
    subresource.pData = imageDesc.InitialData;
    subresource.RowPitch = static_cast<LONG_PTR>(imageDesc.Width) * bytesPerPixel;
    subresource.SlicePitch = subresource.RowPitch * imageDesc.Height;

    UpdateSubresources(cmdList, imageResource.Get(), uploadBuffer.Get(), 0, firstSubresource, numSubresources, &subresource);

    CD3DX12_RESOURCE_BARRIER toShaderRead = CD3DX12_RESOURCE_BARRIER::Transition(
        imageResource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    cmdList->ResourceBarrier(1, &toShaderRead);
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXFormat(imageDesc.Format);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = AllocateDescriptor(SRV);
    device->CreateShaderResourceView(imageResource.Get(), &srvDesc, srvHandle);

    const D3D12_CPU_DESCRIPTOR_HANDLE heapCpuStart = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
    const D3D12_GPU_DESCRIPTOR_HANDLE heapGpuStart = ShaderResourceHeap->GetGPUDescriptorHandleForHeapStart();

    const UINT64 byteOffset = srvHandle.ptr - heapCpuStart.ptr;
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle{};
    srvGpuHandle.ptr = heapGpuStart.ptr + byteOffset;

    DX12ImageData* imageData = new DX12ImageData();
    imageData->Image = imageResource;
    imageData->Descriptor = srvHandle;

    ImageAllocation allocation;
    allocation.Image = imageData;
    allocation.Desc = imageDesc;
    allocation.Descriptor = srvGpuHandle.ptr;

    return CacheImage(allocation);
}

DirectX12BufferAllocator::DirectX12BufferAllocator()
{
    ID3D12Device* device = D3DCore::GetInstance().GetDevice().Get();
    
    ShaderResourceOffset = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    RenderTargetOffset = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    DepthStencilOffset = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = MaxSRVs + MaxCBVs + MaxUAVs;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    srvHeapDesc.NodeMask = 0;
    device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(ShaderResourceHeap.GetAddressOf())) >> ERROR_HANDLER;
    
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = MaxRTVs;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;
    device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(RenderTargetHeap.GetAddressOf())) >> ERROR_HANDLER;
    
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = MaxDSVs;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(DepthStencilHeap.GetAddressOf())) >> ERROR_HANDLER;
    
    SRVAllocator = new BitPool();
    SRVAllocator->Initialize(0, 1, MaxSRVs);
    
    CBVAllocator = new BitPool();
    CBVAllocator->Initialize(MaxSRVs, 1, MaxCBVs);
    
    UAVAllocator = new BitPool();
    UAVAllocator->Initialize(MaxSRVs + MaxCBVs, 1, MaxUAVs);
    
    RTVAllocator = new BitPool();
    RTVAllocator->Initialize(0, 1, MaxRTVs);
    
    DSVAllocator = new BitPool();
    DSVAllocator->Initialize(0, 1, MaxDSVs);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12BufferAllocator::AllocateDescriptor(DescriptorType type)
{
    size_t index = 0;
    switch (type)
    {
    case SRV:
        index = SRVAllocator->Allocate();
        break;
    case CBV:
        index = CBVAllocator->Allocate();
        break;
    case UAV:
        index = UAVAllocator->Allocate();
        break;
    case RTV:
        index = RTVAllocator->Allocate();
        break;
    case DSV:
        index = DSVAllocator->Allocate();
        break;
    }
    
    return GetHandle(index, type);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12BufferAllocator::GetHandle(size_t index, DescriptorType type)
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = {};
    switch (type)
    {
    case SRV:
        handle = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * ShaderResourceOffset;
        return handle;
    case CBV:
        handle = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += (MaxSRVs + index) * ShaderResourceOffset;
        return handle;
    case UAV:
        handle = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += (MaxSRVs + MaxCBVs + index) * ShaderResourceOffset;
        return handle;
    case RTV:
        handle = RenderTargetHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * RenderTargetOffset;
        return handle;
    case DSV:
        handle = DepthStencilHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += index * DepthStencilOffset;
        return handle;
    }
    return handle;
}

void DirectX12BufferAllocator::FreeDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE handle, DescriptorType type)
{
    BitPool* pool = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE start = {};
    switch (type)
    {
    case SRV:
        start = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
        pool = SRVAllocator;
        break;
    case CBV:
        start = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
        start.ptr += MaxSRVs * ShaderResourceOffset;
        pool = CBVAllocator;
        break;
    case UAV:
        start = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
        start.ptr += (MaxSRVs + MaxCBVs) * ShaderResourceOffset;
        pool = UAVAllocator;
        break;
    case RTV:
        start = RenderTargetHeap->GetCPUDescriptorHandleForHeapStart();
        pool = RTVAllocator;
        break;
    case DSV:
        start = DepthStencilHeap->GetCPUDescriptorHandleForHeapStart();
        pool = DSVAllocator;
        break;
    }

    size_t offset = static_cast<size_t>(handle.ptr) - static_cast<size_t>(start.ptr);
    
    pool->Free(offset);
}

DirectX12BufferAllocator::~DirectX12BufferAllocator()
{
    delete SRVAllocator;
    delete CBVAllocator;
    delete UAVAllocator;
    delete RTVAllocator;
    delete DSVAllocator;
}

void DirectX12BufferAllocator::FreeBuffer(uint64_t id)
{
    FreeDescriptor(DXDescriptor(AllocatedBuffers[id]), static_cast<DescriptorType>(AllocatedBuffers[id].DescriptorType));
}

void DirectX12BufferAllocator::FreeImage(uint64_t id)
{
    FreeDescriptor(DXDescriptor(AllocatedImages[id]), static_cast<DescriptorType>(AllocatedImages[id].DescriptorType));
}

