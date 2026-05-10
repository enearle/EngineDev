#include "BufferAllocator.h"

#include <iostream>
#include <map>

#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../DirectX12/D3D12Structs.h"
#include "../Data/BitPool.h"


using namespace Win32ErrorHandler;
using namespace D3D12Structs;

BufferAllocator* BufferAllocator::Instance = nullptr;

BufferAllocator* BufferAllocator::GetInstance()
{
    if (!Instance)
    {
        if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
            Instance = new VulkanBufferAllocator();
        else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
            Instance = new DirectX12BufferAllocator();
        else
            throw std::runtime_error("Invalid graphics API selected");
    }
    return Instance;
}

//================================================//
// Vulkan                                         //
//================================================//

VulkanBufferAllocator::VulkanBufferAllocator()
{

}

uint64_t VulkanBufferAllocator::CreateBuffer(BufferDesc bufferDesc)
{
    VulkanBufferData* vulkanBufferData = new VulkanBufferData();
    VkBufferUsageFlags bufferFlags = VulkanBufferUsage(bufferDesc.Usage);
    VkMemoryPropertyFlags memoryFlags = VulkanMemoryType(bufferDesc.Usage.Access);
    VkDevice device = VulkanCore::Instance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::Instance().GetPhysicalDevice();
    
    bool needsDeviceAddress = (bufferDesc.Usage.Type == BufferType::Constant || bufferDesc.Usage.Type == BufferType::ShaderStorage);
    if (needsDeviceAddress)
    {
        bufferFlags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }

    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferDesc.Size;
    bufferInfo.usage = bufferFlags;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &vulkanBufferData->Buffer);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer.");

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device, vulkanBufferData->Buffer, &memoryRequirements);
    
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
    
    VkMemoryAllocateFlagsInfo allocFlags = {};
    allocFlags.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlags.flags = needsDeviceAddress ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0;
    
    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memoryFlags);
    allocInfo.pNext = needsDeviceAddress ? &allocFlags : nullptr;
    
    result = vkAllocateMemory(device, &allocInfo, nullptr, &vulkanBufferData->Memory);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory.");
    
    result = vkBindBufferMemory(device, vulkanBufferData->Buffer, vulkanBufferData->Memory, 0);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to bind vertex buffer memory.");

    bool isHostVisible = (memoryFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    
    void* mappedAddress = nullptr;

    if (isHostVisible)
    {
        // Map memory persistently for CPU access
        result = vkMapMemory(device, vulkanBufferData->Memory, 0, bufferDesc.Size, 0, &mappedAddress);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to map buffer memory.");
        
        // Upload initial data if provided
        if (bufferDesc.InitialData != nullptr)
            memcpy(mappedAddress, bufferDesc.InitialData, bufferDesc.Size);
    }
    else if (bufferDesc.InitialData != nullptr)
        // Device-local: use staging buffer
        CopyToDeviceLocalBuffer(vulkanBufferData->Buffer, bufferDesc.InitialData, bufferDesc.Size);
    
    BufferAllocation allocation;
    allocation.Buffer = vulkanBufferData;
    allocation.Address = mappedAddress;
    allocation.Size = bufferDesc.Size;
    allocation.Usage = bufferDesc.Usage;
    allocation.IsMapped = isHostVisible;
    allocation.Descriptor = 0;
    allocation.DescriptorType = 0;
    
    return CacheBuffer(allocation);
}

void VulkanBufferAllocator::CopyToDeviceLocalBuffer(VkBuffer dstBuffer, const void* srcData, VkDeviceSize size)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::Instance().GetPhysicalDevice();
    VkCommandBuffer commandBuffer = VulkanCore::Instance().GetTransferCommandBuffer();
    VkQueue transferQueue = VulkanCore::Instance().GetGraphicsQueue();
    VkFence transferFence = VulkanCore::Instance().GetTransferFence();
    
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
    stagingAllocInfo.memoryTypeIndex = FindMemoryType(stagingMemoryReqs.memoryTypeBits, stagingFlags);

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
    
    // Ensure command buffer is ready to be recorded
    // First wait for any previous operation to complete
    VkResult fenceStatus = vkGetFenceStatus(device, transferFence);
    if (fenceStatus == VK_NOT_READY)  // Previous operation still running
    {
        vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
    }
    
    // Now reset the command buffer and fence for new recording
    vkResetCommandBuffer(commandBuffer, 0);
    vkResetFences(device, 1, &transferFence);
    
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
    
    result = vkQueueSubmit(transferQueue, 1, &submitInfo, transferFence);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to submit transfer command buffer");
    
    // Wait for THIS operation to complete before cleaning up staging buffer
    result = vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to wait for transfer fence");
    
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
}

uint64_t VulkanBufferAllocator::CreateImage(ImageDesc imageDesc)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::Instance().GetPhysicalDevice();
    
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
    stagingAllocInfo.memoryTypeIndex = FindMemoryType(stagingMemoryReqs.memoryTypeBits, stagingFlags);

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
    allocation.Descriptor = 0;
    allocation.DescriptorType = 0;
    
    
    return CacheImage(allocation);
}

void VulkanBufferAllocator::RegisterDescriptorSetLayout(uint32_t pipelineID, const ResourceLayout& layout, bool fillEmptySets)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    
    // Group bindings by set
    std::map<uint32_t, std::vector<DescriptorBinding>> bindingsBySet;
    for (const DescriptorBinding& binding : layout.Bindings)
        bindingsBySet[binding.Set].push_back(binding);
    
    // Create descriptor set layout and pool for each set
    for (const auto& [setIndex, bindings] : bindingsBySet)
    {
        uint64_t key = MakeKey(pipelineID, setIndex);
        
        // Skip if already registered
        if (DescriptorSetLayouts.find(key) != DescriptorSetLayouts.end())
            continue;
        
        // Create VkDescriptorSetLayoutBinding array
        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        std::vector<VkDescriptorPoolSize> poolSizes;
        
        for (const DescriptorBinding& binding : bindings)
        {
            VkDescriptorSetLayoutBinding vkBinding{};
            vkBinding.binding = binding.Slot;
            vkBinding.descriptorType = VulkanDescriptorType(binding.Type);
            vkBinding.descriptorCount = binding.Count > 0 ? binding.Count : 1;
            vkBinding.stageFlags = VulkanShaderStageFlags(layout.VisibleStages);
            vkBinding.pImmutableSamplers = VulkanCore::Instance().GetLinearSampler();
            vkBindings.push_back(vkBinding);
            
            // Add to pool sizes
            VkDescriptorPoolSize poolSize{};
            poolSize.type = vkBinding.descriptorType;
            poolSize.descriptorCount = vkBinding.descriptorCount * 256; // 256 sets max
            poolSizes.push_back(poolSize);
        }
        
        // Create descriptor set layout
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
        layoutInfo.pBindings = vkBindings.data();
        
        VkDescriptorSetLayout descriptorSetLayout;
        VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor set layout");
        
        // Create descriptor pool
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 256;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        
        VkDescriptorPool descriptorPool;
        result = vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool");
        
        // Store layout info
        DescriptorSetLayoutInfo layoutInfoStore;
        layoutInfoStore.Layout = descriptorSetLayout;
        layoutInfoStore.Pool = descriptorPool;
        layoutInfoStore.Bindings = bindings;
        DescriptorSetLayouts[key] = layoutInfoStore;
    }
    
    // After creating all layouts with bindings, fill gaps with empty layouts
    if (!bindingsBySet.empty() && fillEmptySets)
    {
        uint32_t maxSetNumber = bindingsBySet.rbegin()->first;
    
        for (uint32_t setIndex = 0; setIndex <= maxSetNumber; setIndex++)
        {
            uint64_t key = MakeKey(pipelineID, setIndex);
            
            if (DescriptorSetLayouts.find(key) != DescriptorSetLayouts.end())
                continue;
            
            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 0;
            layoutInfo.pBindings = nullptr;
        
            VkDescriptorSetLayout descriptorSetLayout;
            VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
            if (result != VK_SUCCESS)
                throw std::runtime_error("Failed to create empty descriptor set layout");
            
            DescriptorSetLayoutInfo layoutInfoStore;
            layoutInfoStore.Layout = descriptorSetLayout;
            layoutInfoStore.Pool = VK_NULL_HANDLE; // No pool for empty layouts
            DescriptorSetLayouts[key] = layoutInfoStore;
        }
    }
}

uint64_t VulkanBufferAllocator::AllocateDescriptorSet(uint32_t pipelineID, uint32_t setIndex, const std::vector<DescriptorSetBinding>& bindings)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    
    
    uint64_t key = MakeKey(pipelineID, setIndex);
    auto iterator = DescriptorSetLayouts.find(key);
    
    if (iterator == DescriptorSetLayouts.end())
        throw std::runtime_error("Descriptor set layout not registered for set " + std::to_string(pipelineID));
    
    DescriptorSetLayoutInfo& layoutInfo = iterator->second;
    
    // Allocate descriptor set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = layoutInfo.Pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layoutInfo.Layout;
    
    if (layoutInfo.Pool == VK_NULL_HANDLE)
        throw std::runtime_error("Cannot allocate from empty descriptor set layout for pipeline " + std::to_string(pipelineID) + " set " + std::to_string(setIndex));

    VkDescriptorSet descriptorSet;
    VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set");
    
    // Update descriptor set with bindings
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    std::vector<uint32_t> dynamicOffsets;
    
    writes.reserve(layoutInfo.Bindings.size());
    imageInfos.reserve(layoutInfo.Bindings.size());
    bufferInfos.reserve(layoutInfo.Bindings.size());
    
    for (const DescriptorBinding& layoutBinding : layoutInfo.Bindings)
    {
        auto bindingIt = std::find_if(bindings.begin(), bindings.end(),
            [&](const DescriptorSetBinding& b) { return b.Binding == layoutBinding.Slot; });
        
        if (bindingIt == bindings.end())
            throw std::runtime_error("Missing resource for binding " + std::to_string(layoutBinding.Slot));
        
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = layoutBinding.Slot;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VulkanDescriptorType(layoutBinding.Type);
        write.pImageInfo = nullptr;        // Initialize to null
        write.pBufferInfo = nullptr;       // Initialize to null
        write.pTexelBufferView = nullptr;  // Initialize to null
        
        if (layoutBinding.Type == RHIStructures::DescriptorType::SampledImage)
        {
            ImageAllocation imageAlloc = GetImageAllocation(bindingIt->ResourceID);
            VulkanImageData* imageData = static_cast<VulkanImageData*>(imageAlloc.Image);
            
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageView = imageData->ImageView;
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.sampler = *VulkanCore::Instance().GetLinearSampler();
            imageInfos.push_back(imageInfo);
            
            write.pImageInfo = &imageInfos.back();
        }
        else if (layoutBinding.Type == RHIStructures::DescriptorType::UniformBuffer ||
                 layoutBinding.Type == RHIStructures::DescriptorType::DynamicUniformBuffer)
        {
            BufferAllocation bufferAlloc = GetBufferAllocation(bindingIt->ResourceID);
            VulkanBufferData* bufferData = static_cast<VulkanBufferData*>(bufferAlloc.Buffer);
            
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = bufferData->Buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = bufferAlloc.Size;
            bufferInfos.push_back(bufferInfo);
            
            write.pBufferInfo = &bufferInfos.back();
            
            if (layoutBinding.Type == RHIStructures::DescriptorType::DynamicUniformBuffer)
            {
                dynamicOffsets.push_back(bindingIt->DynamicOffset);
            }
        }
        
        writes.push_back(write);
    }
    
    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    
    // Create allocation
    DescriptorSetAllocation allocation;
    allocation.DescriptorAddress = reinterpret_cast<uint64_t>(descriptorSet);
    allocation.SetKey = MakeKey(pipelineID, setIndex);
    allocation.PlatformData = nullptr;
    allocation.DynamicOffsets = dynamicOffsets;
    allocation.DynamicDescriptorCount = static_cast<uint32_t>(dynamicOffsets.size());
    
    uint64_t id = CacheDescriptorSet(allocation);
    return id;
}

void VulkanBufferAllocator::FreeDescriptorSet(uint64_t setID)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    auto& allocation = AllocatedDescriptorSets[setID];
    
    auto it = DescriptorSetLayouts.find(allocation.SetKey);
    if (it != DescriptorSetLayouts.end())
    {
        VkDescriptorSet descriptorSet = reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress);
        vkFreeDescriptorSets(device, it->second.Pool, 1, &descriptorSet);
    }
    
    AllocatedDescriptorSets.erase(setID);
}

void VulkanBufferAllocator::UpdateDescriptorSet(uint64_t setID, const std::vector<DescriptorSetBinding>& newBindings)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    auto it = AllocatedDescriptorSets.find(setID);
    if (it == AllocatedDescriptorSets.end())
        return;

    DescriptorSetAllocation& allocation = it->second;
    auto layoutIt = DescriptorSetLayouts.find(allocation.SetKey);
    if (layoutIt == DescriptorSetLayouts.end())
        return;

    DescriptorSetLayoutInfo& layoutInfo = layoutIt->second;
    VkDescriptorSet descriptorSet = reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress);

    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorImageInfo> imageInfos;
    imageInfos.reserve(layoutInfo.Bindings.size());

    for (const DescriptorBinding& layoutBinding : layoutInfo.Bindings)
    {
        if (layoutBinding.Type != RHIStructures::DescriptorType::SampledImage)
            continue;

        auto bindingIt = std::find_if(newBindings.begin(), newBindings.end(),
            [&](const DescriptorSetBinding& b) { return b.Binding == layoutBinding.Slot; });

        if (bindingIt == newBindings.end())
            continue;

        ImageAllocation imageAlloc = GetImageAllocation(bindingIt->ResourceID);
        VulkanImageData* imageData = static_cast<VulkanImageData*>(imageAlloc.Image);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = imageData->ImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.sampler = *VulkanCore::Instance().GetLinearSampler();
        imageInfos.push_back(imageInfo);

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = layoutBinding.Slot;
        write.dstArrayElement = 0;
        write.descriptorCount = 1;
        write.descriptorType = VulkanDescriptorType(layoutBinding.Type);
        write.pImageInfo = &imageInfos.back();
        writes.push_back(write);
    }

    if (!writes.empty())
        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanBufferAllocator::UpdateDescriptorSetDynamicOffsets(uint64_t setID, const std::vector<uint32_t>& offsets)
{
    auto it = AllocatedDescriptorSets.find(setID);
    if (it == AllocatedDescriptorSets.end())
        throw std::runtime_error("Invalid descriptor set ID");
    
    DescriptorSetAllocation& allocation = it->second;
    
    if (offsets.size() != allocation.DynamicDescriptorCount)
        throw std::runtime_error("Dynamic offset count mismatch");
    
    VkPhysicalDevice physicalDevice = VulkanCore::Instance().GetPhysicalDevice();
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
    
    uint32_t minAlignment = static_cast<uint32_t>(deviceProperties.limits.minUniformBufferOffsetAlignment);
    
    for (uint32_t offset : offsets)
        if (offset % minAlignment != 0)
            throw std::runtime_error("Dynamic offset " + std::to_string(offset) + 
                                    " not aligned to " + std::to_string(minAlignment) + " bytes");
    
    // Update cached offsets
    allocation.DynamicOffsets = offsets;
}

VkDescriptorSetLayout VulkanBufferAllocator::GetRegisteredDescriptorSetLayout(uint32_t pipelineID, uint32_t setIndex)
{
    uint64_t key = MakeKey(pipelineID, setIndex);
    auto iterator = DescriptorSetLayouts.find(key);
    
    if (iterator == DescriptorSetLayouts.end())
        return VK_NULL_HANDLE;
    
    return iterator->second.Layout;
}

VulkanBufferAllocator::~VulkanBufferAllocator()
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    
    for (auto& [handle, allocation] : DescriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(device, allocation.Layout, nullptr);
    
        if (allocation.Pool != VK_NULL_HANDLE)  // ← ADD THIS CHECK
            vkDestroyDescriptorPool(device, allocation.Pool, nullptr);
    }
    
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

void VulkanBufferAllocator::CopyBufferToImage(VkBuffer stagingBuffer, VkImage dstImage, uint32_t width, uint32_t height)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    VkCommandBuffer commandBuffer = VulkanCore::Instance().GetTransferCommandBuffer();
    VkQueue transferQueue = VulkanCore::Instance().GetGraphicsQueue();
    VkFence transferFence = VulkanCore::Instance().GetTransferFence();
    
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
    VkDevice device = VulkanCore::Instance().GetDevice();
    VkCommandBuffer commandBuffer = VulkanCore::Instance().GetTransferCommandBuffer();
    VkQueue commandQueue = VulkanCore::Instance().GetGraphicsQueue();
    VkFence transferFence = VulkanCore::Instance().GetTransferFence();

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
        throw std::runtime_error("vkGetFenceStatus returned an error in CopyBufferToImage().");
    
    
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
    imageMemoryBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;// number of array layers

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
    VkDevice device = VulkanCore::Instance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::Instance().GetPhysicalDevice();
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
    allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memoryFlags);

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
    VkDevice device = VulkanCore::Instance().GetDevice();
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

uint32_t VulkanBufferAllocator::FindMemoryType(uint32_t allowdTypes, VkMemoryPropertyFlags flags)
{
    // Get properties of physical device memory
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vkGetPhysicalDeviceMemoryProperties(VulkanCore::Instance().GetPhysicalDevice(), &memoryProperties);

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
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    ID3D12GraphicsCommandList* cmdList = D3DCore::Instance().GetTransferCommandList().Get();

    ComPtr<ID3D12Resource> buffer;
    void* mappedAddress = nullptr;
    bool isHostVisible = bufferDesc.Usage.Access.GetCPUWrite();

    if (isHostVisible)
    {
     // Create UPLOAD heap for CPU-writable buffers (dynamic uniforms)
     CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
     CD3DX12_RESOURCE_DESC bufferDescDX = CD3DX12_RESOURCE_DESC::Buffer(bufferDesc.Size);
     
     device->CreateCommittedResource(
         &uploadHeapProperties,
         D3D12_HEAP_FLAG_NONE,
         &bufferDescDX,
         D3D12_RESOURCE_STATE_GENERIC_READ,
         nullptr,
         IID_PPV_ARGS(buffer.GetAddressOf())) >> ERROR_HANDLER;
     
     // Map persistently for dynamic updates
     buffer->Map(0, nullptr, &mappedAddress);
     
     // Upload initial data if provided
     if (bufferDesc.InitialData != nullptr)
     {
         memcpy(mappedAddress, bufferDesc.InitialData, bufferDesc.Size);
     }
    }
    else
    {
     // Create DEFAULT heap for GPU-only buffers (your existing code)
     ComPtr<ID3D12Resource> defaultBuffer;
     ComPtr<ID3D12Resource> uploadBuffer;
     
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
     
     D3DCore::Instance().DeferUploadBufferRelease(uploadBuffer);
     
     if (bufferDesc.InitialData != nullptr)
     {
         void* stagingMappedData = nullptr;
         uploadBuffer->Map(0, nullptr, &stagingMappedData);
         memcpy(stagingMappedData, bufferDesc.InitialData, bufferDesc.Size);
         uploadBuffer->Unmap(0, nullptr);

         D3D12_SUBRESOURCE_DATA subResourceData = {};
         subResourceData.pData = bufferDesc.InitialData;
         subResourceData.RowPitch = bufferDesc.Size;
         subResourceData.SlicePitch = bufferDesc.Size;

         D3DCore::Instance().GetTransferCommandAllocator()->Reset();
         cmdList->Reset(D3DCore::Instance().GetTransferCommandAllocator().Get(), nullptr);
         
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
         cmdList->Close();
         
         ID3D12CommandQueue* commandQueue = D3DCore::Instance().GetCommandQueue().Get();
         ComPtr<ID3D12Fence> transferFence = D3DCore::Instance().GetTransferFence();
         
         ID3D12CommandList* ppCommandLists[] = { cmdList };
         commandQueue->ExecuteCommandLists(1, ppCommandLists);
         
         static UINT64 transferFenceValue = 0;
         transferFenceValue++;
         commandQueue->Signal(transferFence.Get(), transferFenceValue);
         if (transferFence->GetCompletedValue() < transferFenceValue)
         {
             HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
             transferFence->SetEventOnCompletion(transferFenceValue, eventHandle);
             WaitForSingleObject(eventHandle, INFINITE);
             CloseHandle(eventHandle);
         }
     }
     
     buffer = defaultBuffer;
    }

    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = buffer->GetGPUVirtualAddress();

    DX12BufferData* bufferData = new DX12BufferData();
    bufferData->Buffer = buffer;
    bufferData->GPUAddress = gpuAddress;

    BufferAllocation allocation;
    allocation.Buffer = bufferData;
    allocation.Size = bufferDesc.Size;
    allocation.Address = mappedAddress;  // CPU mapped address (or nullptr if not mapped)
    allocation.Usage = bufferDesc.Usage;
    allocation.IsMapped = (mappedAddress != nullptr);

    return CacheBuffer(allocation);
}

uint64_t DirectX12BufferAllocator::CreateImage(ImageDesc imageDesc)
{
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    ID3D12GraphicsCommandList* cmdList = D3DCore::Instance().GetTransferCommandList().Get();
    D3DCore::Instance().GetTransferCommandAllocator()->Reset();
    cmdList->Reset(D3DCore::Instance().GetTransferCommandAllocator().Get(), nullptr);
    
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
    
    D3DCore::Instance().DeferUploadBufferRelease(uploadBuffer);
    
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
    cmdList->Close();

    ID3D12CommandQueue* commandQueue = D3DCore::Instance().GetCommandQueue().Get();
    ComPtr<ID3D12Fence> transferFence = D3DCore::Instance().GetTransferFence();

    ID3D12CommandList* ppCommandLists[] = { cmdList };
    commandQueue->ExecuteCommandLists(1, ppCommandLists);

    UINT64 transferFenceValue = transferFence->GetCompletedValue() + 1;
    //transferFenceValue++;
    commandQueue->Signal(transferFence.Get(), transferFenceValue);
    if (transferFence->GetCompletedValue() < transferFenceValue)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        transferFence->SetEventOnCompletion(transferFenceValue, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = DXFormat(imageDesc.Format);
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.PlaneSlice = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    
    DX12ImageData* imageData = new DX12ImageData();
    imageData->Image = imageResource;
    
    ImageAllocation allocation;
    allocation.Desc = imageDesc;
    allocation.Image = imageData;

    return CacheImage(allocation);
}

DirectX12BufferAllocator::DirectX12BufferAllocator()
{
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    
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
    
}

DirectX12BufferAllocator::~DirectX12BufferAllocator()
{

}

void DirectX12BufferAllocator::RegisterDescriptorSetLayout(uint32_t pipelineID, const ResourceLayout& layout, bool fillEmptySets)
{
    std::map<uint32_t, std::vector<DescriptorBinding>> bindingsBySet;
    for (const DescriptorBinding& binding : layout.Bindings)
        bindingsBySet[binding.Set].push_back(binding);
    
    for (const auto& [setIndex, bindings] : bindingsBySet)
    {
        uint64_t key = MakeKey(pipelineID, setIndex);
        
        if (DescriptorSetLayouts.find(key) != DescriptorSetLayouts.end())
            continue;
        
        DescriptorSetLayoutInfo layoutInfo;
        layoutInfo.Bindings = bindings;
        DescriptorSetLayouts[key] = layoutInfo;
    }
}

uint64_t DirectX12BufferAllocator::AllocateDescriptorSet(uint32_t pipelineID, uint32_t setIndex, const std::vector<DescriptorSetBinding>& bindings)
{
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    
    uint64_t key = MakeKey(pipelineID, setIndex);
    auto iterator = DescriptorSetLayouts.find(key);
    
    if (iterator == DescriptorSetLayouts.end())
        throw std::runtime_error("Descriptor set layout not registered");
    
    DescriptorSetLayoutInfo& layoutInfo = iterator->second;
    DescriptorTableData* tableData = new DescriptorTableData();
    
    // First pass: Pre-allocate all descriptors contiguously to reserve heap space
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> allocatedHandles;
    std::vector<DescriptorType> descriptorTypes;
    std::vector<uint64_t> resourceIDs;
    std::vector<bool> isDynamic;
    
    for (const DescriptorBinding& layoutBinding : layoutInfo.Bindings)
    {
        DescriptorType dxType;
        D3D12_CPU_DESCRIPTOR_HANDLE dstHandle;

        if (layoutBinding.Type == RHIStructures::DescriptorType::SampledImage)
        {
            dxType = SRV;
            dstHandle = AllocateDescriptor(SRV);
        }
        else if (layoutBinding.Type == RHIStructures::DescriptorType::UniformBuffer ||
                 layoutBinding.Type == RHIStructures::DescriptorType::DynamicUniformBuffer)
        {
            dxType = CBV;
            dstHandle = AllocateDescriptor(CBV);
        }
        else if (layoutBinding.Type == RHIStructures::DescriptorType::StorageBuffer)
        {
            dxType = SRV;
            dstHandle = AllocateDescriptor(SRV);
        }
        else if (layoutBinding.Type == RHIStructures::DescriptorType::StorageImage)
        {
            dxType = UAV;
            dstHandle = AllocateDescriptor(UAV);
        }
        else
        {
            throw std::runtime_error("Unsupported descriptor type");
        }
        
        allocatedHandles.push_back(dstHandle);
        descriptorTypes.push_back(dxType);
        resourceIDs.push_back(0);  // Will be set in second pass
        isDynamic.push_back(layoutBinding.Type == RHIStructures::DescriptorType::DynamicUniformBuffer);
    }
    
    // Second pass: Create the actual descriptor views
    size_t descriptorIndex = 0;
    std::vector<uint32_t> dynamicOffsets;
    
    for (const DescriptorBinding& layoutBinding : layoutInfo.Bindings)
    {
        auto bindingIterator = std::find_if(bindings.begin(), bindings.end(),
            [&](const DescriptorSetBinding& b) { return b.Binding == layoutBinding.Slot; });
        
        if (bindingIterator == bindings.end())
            throw std::runtime_error("Missing resource for binding");
        
        D3D12_CPU_DESCRIPTOR_HANDLE dstHandle = allocatedHandles[descriptorIndex];
        
        // Store the resource ID
        resourceIDs[descriptorIndex] = bindingIterator->ResourceID;
        
        if (layoutBinding.Type == RHIStructures::DescriptorType::SampledImage)
        {
            ImageAllocation imageAlloc = GetImageAllocation(bindingIterator->ResourceID);
            DX12ImageData* imageData = static_cast<DX12ImageData*>(imageAlloc.Image);
            
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXFormat(imageAlloc.Desc.Format);
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.MostDetailedMip = 0;
            srvDesc.Texture2D.PlaneSlice = 0;
            
            device->CreateShaderResourceView(imageData->Image.Get(), &srvDesc, dstHandle);
        }
        else if (layoutBinding.Type == RHIStructures::DescriptorType::UniformBuffer ||
                 layoutBinding.Type == RHIStructures::DescriptorType::DynamicUniformBuffer)
        {
            BufferAllocation bufferAlloc = GetBufferAllocation(bindingIterator->ResourceID);
            DX12BufferData* bufferData = static_cast<DX12BufferData*>(bufferAlloc.Buffer);
            
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
            uint32_t offset = bindingIterator->DynamicOffset;
            cbvDesc.BufferLocation = bufferData->GPUAddress + offset;
            cbvDesc.SizeInBytes = static_cast<UINT>((bufferAlloc.Size + 255) & ~255);
            
            device->CreateConstantBufferView(&cbvDesc, dstHandle);
            
            // Track dynamic offsets for dynamic uniform buffers
            if (layoutBinding.Type == RHIStructures::DescriptorType::DynamicUniformBuffer)
            {
                dynamicOffsets.push_back(bindingIterator->DynamicOffset);
            }
        }
        
        descriptorIndex++;
    }
    
    // Store all handles and types
    tableData->CpuHandles = allocatedHandles;
    tableData->DescriptorTypes = descriptorTypes;
    tableData->ResourceIDs = resourceIDs;
    tableData->IsDynamic = isDynamic;
    
    // Calculate the GPU handle for the base of the table (first descriptor)
    if (!allocatedHandles.empty())
    {
        D3D12_CPU_DESCRIPTOR_HANDLE heapStart = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE gpuStart = ShaderResourceHeap->GetGPUDescriptorHandleForHeapStart();
        size_t offset = allocatedHandles[0].ptr - heapStart.ptr;
        tableData->BaseHandle.ptr = gpuStart.ptr + offset;
    }
    
    DescriptorSetAllocation allocation;
    allocation.DescriptorAddress = tableData->BaseHandle.ptr;
    allocation.SetKey = MakeKey(pipelineID, setIndex);
    allocation.PlatformData = tableData;
    allocation.DynamicOffsets = dynamicOffsets;
    allocation.DynamicDescriptorCount = static_cast<uint32_t>(dynamicOffsets.size());
    
    return CacheDescriptorSet(allocation);
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12BufferAllocator::AllocateDescriptor(DescriptorType type)
{
    size_t index = 0;
    switch (type)
    {
    case SRV:
        index = NextSRVIndex++;
        break;
    case CBV:
        index = NextCBVIndex++;
        break;
    case UAV:
        index = NextUAVIndex++;
        break;
    case RTV:
        index = NextRTVIndex++;
        break;
    case DSV:
        index = NextDSVIndex++;
        break;
    }
    
    return GetHandle(index, type);
}

void DirectX12BufferAllocator::FreeDescriptorSet(uint64_t setID)
{
    auto& allocation = AllocatedDescriptorSets[setID];
    DescriptorTableData* tableData = static_cast<DescriptorTableData*>(allocation.PlatformData);
    
    delete tableData;
    AllocatedDescriptorSets.erase(setID);
}

void DirectX12BufferAllocator::UpdateDescriptorSetDynamicOffsets(uint64_t setID, const std::vector<uint32_t>& offsets)
{
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    
    auto it = AllocatedDescriptorSets.find(setID);
    if (it == AllocatedDescriptorSets.end())
        throw std::runtime_error("Invalid descriptor set ID");
    
    DescriptorSetAllocation& allocation = it->second;
    DescriptorTableData* tableData = static_cast<DescriptorTableData*>(allocation.PlatformData);
    
    if (!tableData)
        throw std::runtime_error("Invalid descriptor table data");
    
    // Validate alignment (DirectX12 requires 256-byte alignment for CBVs)
    constexpr uint32_t D3D12_CONSTANT_BUFFER_ALIGNMENT = 256;
    for (uint32_t offset : offsets)
    {
        if (offset % D3D12_CONSTANT_BUFFER_ALIGNMENT != 0)
            throw std::runtime_error("Dynamic offset " + std::to_string(offset) + 
                                    " not aligned to 256 bytes");
    }
    
    // Update CBVs with new offsets
    uint32_t offsetIndex = 0;
    for (size_t i = 0; i < tableData->IsDynamic.size(); ++i)
    {
        if (tableData->IsDynamic[i])
        {
            if (offsetIndex >= offsets.size())
                throw std::runtime_error("Not enough offsets provided");
            
            // Get the buffer allocation
            BufferAllocation bufferAlloc = GetBufferAllocation(tableData->ResourceIDs[i]);
            DX12BufferData* bufferData = static_cast<DX12BufferData*>(bufferAlloc.Buffer);
            
            // Recreate the CBV with new offset
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
            cbvDesc.BufferLocation = bufferData->GPUAddress + offsets[offsetIndex];
            cbvDesc.SizeInBytes = static_cast<UINT>((bufferAlloc.Size + 255) & ~255);
            
            device->CreateConstantBufferView(&cbvDesc, tableData->CpuHandles[i]);
            
            offsetIndex++;
        }
    }
    
    if (offsetIndex != offsets.size())
        throw std::runtime_error("Offset count mismatch");
    
    // Update cached offsets
    allocation.DynamicOffsets = offsets;
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
