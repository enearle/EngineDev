#include "BufferAllocator.h"

#include <algorithm>
#include <iostream>
#include <map>
#include <string>

#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../DirectX12/D3D12Structs.h"


using namespace Win32ErrorHandler;
using namespace D3D12Structs;

BufferAllocator* BufferAllocator::Instance = nullptr;

void BufferAllocator::DestroyBuffer(BufferHandle buffer)
{
    PendingRelease pending;
    pending.FrameNumber = FrameNumber;
    pending.Kind = PendingKind::Buffer;
    pending.Buffer = Buffers.Get(buffer);
    PendingReleases.push_back(pending);
    Buffers.Release(buffer);
}

void BufferAllocator::DestroyImage(ImageHandle image)
{
    PendingRelease pending;
    pending.FrameNumber = FrameNumber;
    pending.Kind = PendingKind::Image;
    pending.Image = Images.Get(image);
    PendingReleases.push_back(pending);
    Images.Release(image);
}

void BufferAllocator::FreeDescriptorSet(DescriptorSetHandle set)
{
    PendingRelease pending;
    pending.FrameNumber = FrameNumber;
    pending.Kind = PendingKind::DescriptorSet;
    pending.DescriptorSet = DescriptorSets.Get(set);
    PendingReleases.push_back(pending);
    DescriptorSets.Release(set);
}

void BufferAllocator::RetireDescriptorSetLayouts(uint32_t layoutID)
{
    PendingRelease pending;
    pending.FrameNumber = FrameNumber;
    pending.Kind = PendingKind::DescriptorSetLayouts;
    pending.LayoutID = layoutID;
    PendingReleases.push_back(pending);
}

void BufferAllocator::ReleasePending(PendingRelease& pending)
{
    switch (pending.Kind)
    {
    case PendingKind::Buffer:               ReleaseBuffer(pending.Buffer); break;
    case PendingKind::Image:                ReleaseImage(pending.Image); break;
    case PendingKind::DescriptorSet:        ReleaseDescriptorSet(pending.DescriptorSet); break;
    case PendingKind::DescriptorSetLayouts: ReleaseDescriptorSetLayouts(pending.LayoutID); break;
    }
}

void BufferAllocator::BeginFrame(uint64_t frameNumber, uint32_t framesInFlight)
{
    if (FrameRing.IsValid() && framesInFlight != FramesInFlight)
        throw std::runtime_error("Frames in flight changed after the frame ring was created.");

    FrameNumber = frameNumber;
    FramesInFlight = framesInFlight;

    while (!PendingReleases.empty() && PendingReleases.front().FrameNumber + FramesInFlight <= FrameNumber)
    {
        ReleasePending(PendingReleases.front());
        PendingReleases.pop_front();
    }

    uint64_t slot = FrameNumber % FramesInFlight;
    FrameRingCursor = slot * FrameRingBytesPerFrame;
    FrameRingEnd = FrameRingCursor + FrameRingBytesPerFrame;
}

void BufferAllocator::ReleaseAllPending()
{
    for (PendingRelease& pending : PendingReleases)
        ReleasePending(pending);
    PendingReleases.clear();
}

void BufferAllocator::CreateFrameRing(uint64_t bytesPerFrame)
{
    if (FrameRing.IsValid())
        return;

    bytesPerFrame = (bytesPerFrame + UniformAlignment - 1) / UniformAlignment * UniformAlignment;

    BufferDesc desc = {};
    desc.Size = bytesPerFrame * FramesInFlight;
    desc.Usage.Access = MemoryAccess(1);
    desc.Usage.Type = BufferType::FrameData;
    FrameRing = CreateBuffer(desc);

    const BufferAllocation& allocation = Buffers.Get(FrameRing);
    if (!allocation.IsMapped || !allocation.Address)
        throw std::runtime_error("The frame ring buffer is not mapped.");

    FrameRingAddress = static_cast<uint8_t*>(allocation.Address);
    FrameRingBytesPerFrame = bytesPerFrame;
    FrameRingCursor = 0;
    FrameRingEnd = bytesPerFrame;
}

FrameAllocation BufferAllocator::AllocateFrameData(uint64_t size, uint64_t alignment)
{
    if (!FrameRing.IsValid())
        throw std::runtime_error("AllocateFrameData called before CreateFrameRing.");

    uint64_t offset = (FrameRingCursor + alignment - 1) / alignment * alignment;
    if (offset + size > FrameRingEnd)
        throw std::runtime_error("Frame ring overflow: " + std::to_string(size) + " bytes requested, "
            + std::to_string(FrameRingEnd - FrameRingCursor) + " left of " + std::to_string(FrameRingBytesPerFrame));

    FrameRingCursor = offset + size;
    return FrameAllocation{ FrameRing, offset, FrameRingAddress + offset };
}

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

VulkanBufferAllocator::~VulkanBufferAllocator()
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    ReleaseAllPending();
    DescriptorSets.Clear();
    Buffers.ForEachLive([this](BufferAllocation& allocation) { ReleaseBuffer(allocation); });
    Images.ForEachLive([this](ImageAllocation& allocation) { ReleaseImage(allocation); });
    Buffers.Clear();
    Images.Clear();

    for (auto& [key, info] : DescriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(device, info.Layout, nullptr);
        if (info.Pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device, info.Pool, nullptr);
    }
    DescriptorSetLayouts.clear();
    Instance = nullptr;
}

ImageHandle VulkanBufferAllocator::CreateImage(const ImageDesc& imageDesc)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    VulkanImageData* vulkanImageData = new VulkanImageData();
    vulkanImageData->ImageHandle = CreateVulkanImage(imageDesc, &vulkanImageData->Memory);

    if (imageDesc.InitialData)
    {
        VkBufferCreateInfo stagingBufferInfo = {};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = imageDesc.Size;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer stagingBuffer;
        if (vkCreateBuffer(device, &stagingBufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image staging buffer.");

        VkMemoryRequirements stagingMemoryReqs;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingMemoryReqs);

        VkMemoryAllocateInfo stagingAllocInfo = {};
        stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        stagingAllocInfo.allocationSize = stagingMemoryReqs.size;
        stagingAllocInfo.memoryTypeIndex = FindMemoryType(stagingMemoryReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDeviceMemory stagingMemory;
        if (vkAllocateMemory(device, &stagingAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate image staging buffer memory.");
        vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);

        void* mappedData;
        vkMapMemory(device, stagingMemory, 0, imageDesc.Size, 0, &mappedData);
        memcpy(mappedData, imageDesc.InitialData, imageDesc.Size);
        vkUnmapMemory(device, stagingMemory);

        TransitionImageLayout(vulkanImageData->ImageHandle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(stagingBuffer, vulkanImageData->ImageHandle, imageDesc.Width, imageDesc.Height);
        TransitionImageLayout(vulkanImageData->ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
    }

    vulkanImageData->ImageView = CreateVulkanImageView(vulkanImageData->ImageHandle, imageDesc);

    ImageAllocation allocation;
    allocation.Image = vulkanImageData;
    allocation.Desc = imageDesc;
    allocation.Desc.InitialData = nullptr;
    return Images.Claim(allocation);
}

VkImage VulkanBufferAllocator::CreateVulkanImage(const ImageDesc& imageDesc, VkDeviceMemory* imageMemory)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    ImageUsage usage = imageDesc.Usage;
    if (imageDesc.InitialData)
        usage.TransferDestination = true;

    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { imageDesc.Width, imageDesc.Height, 1 };
    imageInfo.mipLevels = (std::max)(imageDesc.MipLevels, 1u);
    imageInfo.arrayLayers = (std::max)(imageDesc.ArrayLayers, 1u);
    imageInfo.format = VulkanFormat(imageDesc.Format);
    imageInfo.tiling = imageDesc.TilingLinear ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VulkanImageUsage(usage);
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkImage image;
    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image.");

    VkMemoryRequirements memoryRequirements;
    vkGetImageMemoryRequirements(device, image, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, imageMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate image memory.");
    if (vkBindImageMemory(device, image, *imageMemory, 0) != VK_SUCCESS)
        throw std::runtime_error("Failed to bind image memory.");

    return image;
}

VkImageView VulkanBufferAllocator::CreateVulkanImageView(VkImage image, const ImageDesc& imageDesc)
{
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VulkanImageViewType(imageDesc);
    viewInfo.format = VulkanFormat(imageDesc.Format);
    viewInfo.subresourceRange.aspectMask = IsDepthFormat(imageDesc.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = (std::max)(imageDesc.MipLevels, 1u);
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = (std::max)(imageDesc.ArrayLayers, 1u);

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(VulkanCore::Instance().GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view.");
    return imageView;
}

void VulkanBufferAllocator::RegisterDescriptorSetLayout(uint32_t layoutID, const ResourceLayout& layout, uint32_t maxSetIndex)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    std::map<uint32_t, std::vector<DescriptorBinding>> bindingsBySet;
    for (const DescriptorBinding& binding : layout.Bindings)
    {
        if (binding.Set == 0)
            throw std::runtime_error("Descriptor set 0 is reserved for the global samplers.");
        bindingsBySet[binding.Set].push_back(binding);
    }

    for (uint32_t setIndex = 1; setIndex <= maxSetIndex; setIndex++)
    {
        uint64_t key = MakeKey(layoutID, setIndex);
        if (DescriptorSetLayouts.contains(key))
            continue;

        DescriptorSetLayoutInfo info;
        std::vector<VkDescriptorSetLayoutBinding> vkBindings;
        std::vector<VkDescriptorPoolSize> poolSizes;

        auto found = bindingsBySet.find(setIndex);
        if (found != bindingsBySet.end())
        {
            info.Bindings = found->second;
            std::sort(info.Bindings.begin(), info.Bindings.end(),
                [](const DescriptorBinding& a, const DescriptorBinding& b) { return a.Slot < b.Slot; });

            for (const DescriptorBinding& binding : info.Bindings)
            {
                VkDescriptorSetLayoutBinding vkBinding{};
                vkBinding.binding = binding.Slot;
                vkBinding.descriptorType = VulkanDescriptorType(binding.Type);
                vkBinding.descriptorCount = (std::max)(binding.Count, 1u);
                vkBinding.stageFlags = VulkanShaderStageFlags(layout.VisibleStages);
                if (binding.Type == DescriptorType::SampledImage)
                    vkBinding.pImmutableSamplers = binding.Sampler == SamplerType::Nearest
                        ? VulkanCore::Instance().GetNearestSampler()
                        : VulkanCore::Instance().GetLinearSampler();
                vkBindings.push_back(vkBinding);
                poolSizes.push_back({ vkBinding.descriptorType, vkBinding.descriptorCount * DescriptorSetsPerPool });
            }
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
        layoutInfo.pBindings = vkBindings.empty() ? nullptr : vkBindings.data();
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &info.Layout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor set layout");

        if (!poolSizes.empty())
        {
            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
            poolInfo.pPoolSizes = poolSizes.data();
            poolInfo.maxSets = DescriptorSetsPerPool;
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &info.Pool) != VK_SUCCESS)
                throw std::runtime_error("Failed to create descriptor pool");
        }

        DescriptorSetLayouts[key] = info;
    }
}

VkDescriptorSetLayout VulkanBufferAllocator::GetRegisteredDescriptorSetLayout(uint32_t layoutID, uint32_t setIndex)
{
    auto found = DescriptorSetLayouts.find(MakeKey(layoutID, setIndex));
    return found == DescriptorSetLayouts.end() ? VK_NULL_HANDLE : found->second.Layout;
}

void VulkanBufferAllocator::WriteDescriptors(VkDescriptorSet descriptorSet, const std::vector<DescriptorBinding>& layoutBindings,
                                             const std::vector<DescriptorSetBinding>& bindings, bool requireEveryBinding)
{
    std::vector<VkWriteDescriptorSet> writes;
    std::vector<VkDescriptorImageInfo> imageInfos;
    std::vector<VkDescriptorBufferInfo> bufferInfos;
    writes.reserve(layoutBindings.size());
    imageInfos.reserve(layoutBindings.size());
    bufferInfos.reserve(layoutBindings.size());

    for (const DescriptorBinding& layoutBinding : layoutBindings)
    {
        auto bindingIt = std::find_if(bindings.begin(), bindings.end(),
            [&](const DescriptorSetBinding& b) { return b.Binding == layoutBinding.Slot; });

        if (bindingIt == bindings.end())
        {
            if (requireEveryBinding)
                throw std::runtime_error("Missing resource for binding " + std::to_string(layoutBinding.Slot));
            continue;
        }

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptorSet;
        write.dstBinding = layoutBinding.Slot;
        write.descriptorCount = 1;
        write.descriptorType = VulkanDescriptorType(layoutBinding.Type);

        if (layoutBinding.Type == DescriptorType::SampledImage || layoutBinding.Type == DescriptorType::StorageImage)
        {
            const VulkanImageData* imageData = static_cast<VulkanImageData*>(GetImageAllocation(bindingIt->Image).Image);

            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageView = imageData->ImageView;
            imageInfo.imageLayout = layoutBinding.Type == DescriptorType::StorageImage
                ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfos.push_back(imageInfo);
            write.pImageInfo = &imageInfos.back();
        }
        else
        {
            const BufferAllocation& bufferAllocation = GetBufferAllocation(bindingIt->Buffer);

            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = static_cast<VulkanBufferData*>(bufferAllocation.Buffer)->Buffer;
            bufferInfo.offset = 0;
            bufferInfo.range = layoutBinding.Type == DescriptorType::DynamicUniformBuffer ? layoutBinding.Size : VK_WHOLE_SIZE;
            if (layoutBinding.Type == DescriptorType::DynamicUniformBuffer && layoutBinding.Size == 0)
                throw std::runtime_error("A dynamic uniform buffer binding needs a Size.");
            bufferInfos.push_back(bufferInfo);
            write.pBufferInfo = &bufferInfos.back();
        }

        writes.push_back(write);
    }

    if (!writes.empty())
        vkUpdateDescriptorSets(VulkanCore::Instance().GetDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

DescriptorSetHandle VulkanBufferAllocator::AllocateDescriptorSet(uint32_t layoutID, uint32_t setIndex, const std::vector<DescriptorSetBinding>& bindings)
{
    auto found = DescriptorSetLayouts.find(MakeKey(layoutID, setIndex));
    if (found == DescriptorSetLayouts.end() || found->second.Pool == VK_NULL_HANDLE)
        throw std::runtime_error("No descriptor set layout with bindings for layout " + std::to_string(layoutID) + " set " + std::to_string(setIndex));

    DescriptorSetLayoutInfo& layoutInfo = found->second;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = layoutInfo.Pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layoutInfo.Layout;

    VkDescriptorSet descriptorSet;
    if (vkAllocateDescriptorSets(VulkanCore::Instance().GetDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set");

    WriteDescriptors(descriptorSet, layoutInfo.Bindings, bindings, true);

    DescriptorSetAllocation allocation;
    allocation.DescriptorAddress = reinterpret_cast<uint64_t>(descriptorSet);
    allocation.SetKey = MakeKey(layoutID, setIndex);
    return DescriptorSets.Claim(allocation);
}

void VulkanBufferAllocator::UpdateDescriptorSet(DescriptorSetHandle set, const std::vector<DescriptorSetBinding>& newBindings)
{
    if (!DescriptorSets.IsLive(set))
        return;

    const DescriptorSetAllocation& allocation = DescriptorSets.Get(set);
    auto found = DescriptorSetLayouts.find(allocation.SetKey);
    if (found == DescriptorSetLayouts.end())
        return;

    WriteDescriptors(reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress), found->second.Bindings, newBindings, false);
}

void VulkanBufferAllocator::ReleaseDescriptorSet(DescriptorSetAllocation& allocation)
{
    auto found = DescriptorSetLayouts.find(allocation.SetKey);
    if (found == DescriptorSetLayouts.end() || found->second.Pool == VK_NULL_HANDLE)
        return;

    VkDescriptorSet descriptorSet = reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress);
    vkFreeDescriptorSets(VulkanCore::Instance().GetDevice(), found->second.Pool, 1, &descriptorSet);
}

void VulkanBufferAllocator::ReleaseDescriptorSetLayouts(uint32_t layoutID)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    for (auto it = DescriptorSetLayouts.begin(); it != DescriptorSetLayouts.end();)
    {
        if ((it->first >> 32) != layoutID)
        {
            ++it;
            continue;
        }

        vkDestroyDescriptorSetLayout(device, it->second.Layout, nullptr);
        if (it->second.Pool != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(device, it->second.Pool, nullptr);
        it = DescriptorSetLayouts.erase(it);
    }
}

void VulkanBufferAllocator::ReleaseBuffer(BufferAllocation& allocation)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    VulkanBufferData* bufferData = static_cast<VulkanBufferData*>(allocation.Buffer);
    if (!bufferData)
        return;

    vkDestroyBuffer(device, bufferData->Buffer, nullptr);
    vkFreeMemory(device, bufferData->Memory, nullptr);
    delete bufferData;
    allocation.Buffer = nullptr;
}

void VulkanBufferAllocator::ReleaseImage(ImageAllocation& allocation)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    VulkanImageData* imageData = static_cast<VulkanImageData*>(allocation.Image);
    if (!imageData)
        return;

    vkDestroyImageView(device, imageData->ImageView, nullptr);
    vkDestroyImage(device, imageData->ImageHandle, nullptr);
    vkFreeMemory(device, imageData->Memory, nullptr);
    delete imageData;
    allocation.Image = nullptr;
}

void VulkanBufferAllocator::EvictImage(ImageHandle image)
{
    if (!Images.IsLive(image))
        return;
    delete static_cast<VulkanImageData*>(Images.Get(image).Image);
    Images.Release(image);
}

BufferHandle VulkanBufferAllocator::CreateBuffer(const BufferDesc& bufferDesc)
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

    return Buffers.Claim(allocation);
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

BufferHandle DirectX12BufferAllocator::CreateBuffer(const BufferDesc& bufferDesc)
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
     CD3DX12_RESOURCE_DESC bufferDescDX = CD3DX12_RESOURCE_DESC::Buffer((bufferDesc.Size + 255) & ~255);

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

    return Buffers.Claim(allocation);
}

//================================================//
// DirectX 12                                     //
//================================================//

void DescriptorRangeAllocator::Reset(uint32_t capacity)
{
    FreeRanges.clear();
    FreeRanges.push_back({ 0, capacity });
}

uint32_t DescriptorRangeAllocator::Claim(uint32_t count)
{
    for (size_t i = 0; i < FreeRanges.size(); i++)
    {
        Range& range = FreeRanges[i];
        if (range.Count < count)
            continue;

        uint32_t start = range.Start;
        range.Start += count;
        range.Count -= count;
        if (range.Count == 0)
            FreeRanges.erase(FreeRanges.begin() + i);
        return start;
    }
    throw std::runtime_error("Descriptor heap is full: no free range of " + std::to_string(count));
}

void DescriptorRangeAllocator::Release(uint32_t start, uint32_t count)
{
    std::vector<Range>::iterator next = std::lower_bound(FreeRanges.begin(), FreeRanges.end(), start,
        [](const Range& range, uint32_t value) { return range.Start < value; });
    next = FreeRanges.insert(next, { start, count });

    std::vector<Range>::iterator following = next + 1;
    if (following != FreeRanges.end() && next->Start + next->Count == following->Start)
    {
        next->Count += following->Count;
        FreeRanges.erase(following);
    }

    if (next != FreeRanges.begin())
    {
        std::vector<Range>::iterator previous = next - 1;
        if (previous->Start + previous->Count == next->Start)
        {
            previous->Count += next->Count;
            FreeRanges.erase(next);
        }
    }
}

static DXGI_FORMAT TypelessDepthFormat(Format format)
{
    switch (format)
    {
    case Format::D16_UNORM:             return DXGI_FORMAT_R16_TYPELESS;
    case Format::D24_UNORM_S8_UINT:     return DXGI_FORMAT_R24G8_TYPELESS;
    case Format::D32_FLOAT:             return DXGI_FORMAT_R32_TYPELESS;
    case Format::D32_FLOAT_S8X24_UINT:  return DXGI_FORMAT_R32G8X24_TYPELESS;
    default:                            return DXFormat(format);
    }
}

static DXGI_FORMAT SampledDepthFormat(Format format)
{
    switch (format)
    {
    case Format::D16_UNORM:             return DXGI_FORMAT_R16_UNORM;
    case Format::D24_UNORM_S8_UINT:     return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case Format::D32_FLOAT:             return DXGI_FORMAT_R32_FLOAT;
    case Format::D32_FLOAT_S8X24_UINT:  return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:                            return DXFormat(format);
    }
}

DirectX12BufferAllocator::DirectX12BufferAllocator()
{
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    ShaderResourceIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    RenderTargetIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    DepthStencilIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = MaxShaderResourceDescriptors;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(ShaderResourceHeap.GetAddressOf())) >> ERROR_HANDLER;

    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = MaxRenderTargetDescriptors;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(RenderTargetHeap.GetAddressOf())) >> ERROR_HANDLER;

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = MaxDepthStencilDescriptors;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(DepthStencilHeap.GetAddressOf())) >> ERROR_HANDLER;

    ShaderResourceSlots.Reset(MaxShaderResourceDescriptors);
    RenderTargetSlots.Reset(MaxRenderTargetDescriptors);
    DepthStencilSlots.Reset(MaxDepthStencilDescriptors);
}

DirectX12BufferAllocator::~DirectX12BufferAllocator()
{
    ReleaseAllPending();
    Buffers.ForEachLive([this](BufferAllocation& allocation) { ReleaseBuffer(allocation); });
    Images.ForEachLive([this](ImageAllocation& allocation) { ReleaseImage(allocation); });
    DescriptorSets.ForEachLive([this](DescriptorSetAllocation& allocation) { ReleaseDescriptorSet(allocation); });
    Instance = nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12BufferAllocator::ShaderResourceCPU(uint32_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * ShaderResourceIncrement;
    return handle;
}

void DirectX12BufferAllocator::SubmitTransferAndWait(ID3D12GraphicsCommandList* commandList)
{
    commandList->Close();

    ID3D12CommandQueue* commandQueue = D3DCore::Instance().GetCommandQueue().Get();
    ComPtr<ID3D12Fence> transferFence = D3DCore::Instance().GetTransferFence();
    ID3D12CommandList* commandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(1, commandLists);

    UINT64 fenceValue = transferFence->GetCompletedValue() + 1;
    commandQueue->Signal(transferFence.Get(), fenceValue);
    if (transferFence->GetCompletedValue() < fenceValue)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        transferFence->SetEventOnCompletion(fenceValue, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

ImageHandle DirectX12BufferAllocator::CreateImage(const ImageDesc& imageDesc)
{
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    bool isDepth = IsDepthFormat(imageDesc.Format);
    uint32_t arrayLayers = (std::max)(imageDesc.ArrayLayers, 1u);

    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = imageDesc.Width;
    textureDesc.Height = imageDesc.Height;
    textureDesc.DepthOrArraySize = static_cast<UINT16>(arrayLayers);
    textureDesc.MipLevels = static_cast<UINT16>((std::max)(imageDesc.MipLevels, 1u));
    textureDesc.Format = isDepth && imageDesc.Usage.Sampled ? TypelessDepthFormat(imageDesc.Format) : DXFormat(imageDesc.Format);
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = DXImageUsage(imageDesc.Usage);

    D3D12_CLEAR_VALUE clearValue = {};
    D3D12_CLEAR_VALUE* optimizedClear = nullptr;
    if (imageDesc.Usage.RenderTarget)
    {
        clearValue.Format = DXFormat(imageDesc.Format);
        memcpy(clearValue.Color, &imageDesc.ClearColour, sizeof(clearValue.Color));
        optimizedClear = &clearValue;
    }
    else if (imageDesc.Usage.DepthStencil)
    {
        clearValue.Format = DXFormat(imageDesc.Format);
        clearValue.DepthStencil.Depth = imageDesc.ClearDepth;
        optimizedClear = &clearValue;
    }

    D3D12_RESOURCE_STATES initialState = imageDesc.InitialData ? D3D12_RESOURCE_STATE_COPY_DEST : D3D12_RESOURCE_STATE_COMMON;
    CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

    ComPtr<ID3D12Resource> imageResource;
    device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc,
        initialState, optimizedClear, IID_PPV_ARGS(imageResource.GetAddressOf())) >> ERROR_HANDLER;

    if (imageDesc.InitialData)
    {
        if (arrayLayers != 1 || textureDesc.MipLevels != 1)
            throw std::runtime_error("Image upload supports one layer and one mip.");

        ID3D12GraphicsCommandList* cmdList = D3DCore::Instance().GetTransferCommandList().Get();
        D3DCore::Instance().GetTransferCommandAllocator()->Reset();
        cmdList->Reset(D3DCore::Instance().GetTransferCommandAllocator().Get(), nullptr);

        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(imageResource.Get(), 0, 1);
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        ComPtr<ID3D12Resource> uploadBuffer;
        device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())) >> ERROR_HANDLER;
        D3DCore::Instance().DeferUploadBufferRelease(uploadBuffer);

        const uint64_t bytesPerPixel = FormatSize(imageDesc.Format);
        if (imageDesc.Size < static_cast<uint64_t>(imageDesc.Width) * imageDesc.Height * bytesPerPixel)
            throw std::runtime_error("ImageDesc::Size is smaller than Width * Height * the format size.");

        D3D12_SUBRESOURCE_DATA subresource = {};
        subresource.pData = imageDesc.InitialData;
        subresource.RowPitch = static_cast<LONG_PTR>(imageDesc.Width * bytesPerPixel);
        subresource.SlicePitch = subresource.RowPitch * imageDesc.Height;
        UpdateSubresources(cmdList, imageResource.Get(), uploadBuffer.Get(), 0, 0, 1, &subresource);

        CD3DX12_RESOURCE_BARRIER toShaderRead = CD3DX12_RESOURCE_BARRIER::Transition(imageResource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        cmdList->ResourceBarrier(1, &toShaderRead);
        SubmitTransferAndWait(cmdList);
    }

    DX12ImageData* imageData = new DX12ImageData();
    imageData->Image = imageResource;

    if (imageDesc.Usage.RenderTarget)
    {
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXFormat(imageDesc.Format);
        if (arrayLayers > 1)
        {
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.ArraySize = arrayLayers;
        }
        else
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

        imageData->RenderTargetSlot = RenderTargetSlots.Claim(1);
        imageData->RenderTargetView = RenderTargetHeap->GetCPUDescriptorHandleForHeapStart();
        imageData->RenderTargetView.ptr += static_cast<SIZE_T>(imageData->RenderTargetSlot) * RenderTargetIncrement;
        device->CreateRenderTargetView(imageResource.Get(), &rtvDesc, imageData->RenderTargetView);
    }

    if (imageDesc.Usage.DepthStencil)
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXFormat(imageDesc.Format);
        if (arrayLayers > 1)
        {
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.ArraySize = arrayLayers;
        }
        else
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

        imageData->DepthStencilSlot = DepthStencilSlots.Claim(1);
        imageData->DepthStencilView = DepthStencilHeap->GetCPUDescriptorHandleForHeapStart();
        imageData->DepthStencilView.ptr += static_cast<SIZE_T>(imageData->DepthStencilSlot) * DepthStencilIncrement;
        device->CreateDepthStencilView(imageResource.Get(), &dsvDesc, imageData->DepthStencilView);
    }

    ImageAllocation allocation;
    allocation.Desc = imageDesc;
    allocation.Desc.InitialData = nullptr;
    allocation.Image = imageData;
    return Images.Claim(allocation);
}

void DirectX12BufferAllocator::ReleaseBuffer(BufferAllocation& allocation)
{
    delete static_cast<DX12BufferData*>(allocation.Buffer);
    allocation.Buffer = nullptr;
}

void DirectX12BufferAllocator::ReleaseImage(ImageAllocation& allocation)
{
    DX12ImageData* imageData = static_cast<DX12ImageData*>(allocation.Image);
    if (!imageData)
        return;

    if (imageData->RenderTargetSlot != UINT32_MAX)
        RenderTargetSlots.Release(imageData->RenderTargetSlot, 1);
    if (imageData->DepthStencilSlot != UINT32_MAX)
        DepthStencilSlots.Release(imageData->DepthStencilSlot, 1);
    delete imageData;
    allocation.Image = nullptr;
}

void DirectX12BufferAllocator::EvictImage(ImageHandle image)
{
    if (!Images.IsLive(image))
        return;
    delete static_cast<DX12ImageData*>(Images.Get(image).Image);
    Images.Release(image);
}

DirectX12BufferAllocator::SetLayoutInfo DirectX12BufferAllocator::BuildSetLayoutInfo(std::vector<DescriptorBinding> bindings)
{
    std::sort(bindings.begin(), bindings.end(),
        [](const DescriptorBinding& a, const DescriptorBinding& b) { return a.Slot < b.Slot; });

    SetLayoutInfo info;
    info.Bindings = bindings;
    for (const DescriptorBinding& binding : info.Bindings)
        if (binding.Type == DescriptorType::DynamicUniformBuffer)
        {
            info.TableOffsets.push_back(UINT32_MAX);
            info.DynamicCount++;
        }
        else
        {
            info.TableOffsets.push_back(info.TableSize);
            info.TableSize += (std::max)(binding.Count, 1u);
        }
    return info;
}

void DirectX12BufferAllocator::RegisterDescriptorSetLayout(uint32_t layoutID, const ResourceLayout& layout, uint32_t maxSetIndex)
{
    std::map<uint32_t, std::vector<DescriptorBinding>> bindingsBySet;
    for (const DescriptorBinding& binding : layout.Bindings)
        bindingsBySet[binding.Set].push_back(binding);

    for (const auto& [setIndex, bindings] : bindingsBySet)
    {
        uint64_t key = MakeKey(layoutID, setIndex);
        if (!DescriptorSetLayouts.contains(key))
            DescriptorSetLayouts[key] = BuildSetLayoutInfo(bindings);
    }
}

void DirectX12BufferAllocator::ReleaseDescriptorSetLayouts(uint32_t layoutID)
{
    std::erase_if(DescriptorSetLayouts, [layoutID](const auto& entry) { return (entry.first >> 32) == layoutID; });
}

void DirectX12BufferAllocator::WriteDescriptor(const DescriptorBinding& layoutBinding, const DescriptorSetBinding& binding,
                                               D3D12_CPU_DESCRIPTOR_HANDLE destination)
{
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();

    switch (layoutBinding.Type)
    {
    case DescriptorType::SampledImage:
    {
        const ImageAllocation& imageAllocation = GetImageAllocation(binding.Image);
        const ImageDesc& desc = imageAllocation.Desc;

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = IsDepthFormat(desc.Format) ? SampledDepthFormat(desc.Format) : DXFormat(desc.Format);
        if (desc.ArrayLayers > 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MipLevels = (std::max)(desc.MipLevels, 1u);
            srvDesc.Texture2DArray.ArraySize = desc.ArrayLayers;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = (std::max)(desc.MipLevels, 1u);
        }
        device->CreateShaderResourceView(static_cast<DX12ImageData*>(imageAllocation.Image)->Image.Get(), &srvDesc, destination);
        break;
    }
    case DescriptorType::StorageBuffer:
    {
        if (layoutBinding.Stride == 0)
            throw std::runtime_error("A storage buffer binding needs a Stride.");

        const BufferAllocation& bufferAllocation = GetBufferAllocation(binding.Buffer);
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = static_cast<UINT>(bufferAllocation.Size / layoutBinding.Stride);
        srvDesc.Buffer.StructureByteStride = layoutBinding.Stride;
        device->CreateShaderResourceView(DXBuffer(bufferAllocation)->Buffer.Get(), &srvDesc, destination);
        break;
    }
    case DescriptorType::UniformBuffer:
    {
        const BufferAllocation& bufferAllocation = GetBufferAllocation(binding.Buffer);
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
        cbvDesc.BufferLocation = DXBuffer(bufferAllocation)->GPUAddress;
        cbvDesc.SizeInBytes = static_cast<UINT>((bufferAllocation.Size + 255) & ~255ull);
        device->CreateConstantBufferView(&cbvDesc, destination);
        break;
    }
    case DescriptorType::StorageImage:
    {
        const ImageAllocation& imageAllocation = GetImageAllocation(binding.Image);
        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXFormat(imageAllocation.Desc.Format);
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(static_cast<DX12ImageData*>(imageAllocation.Image)->Image.Get(), nullptr, &uavDesc, destination);
        break;
    }
    default:
        throw std::runtime_error("Descriptor type does not live in a descriptor table.");
    }
}

DescriptorSetHandle DirectX12BufferAllocator::AllocateDescriptorSet(uint32_t layoutID, uint32_t setIndex, const std::vector<DescriptorSetBinding>& bindings)
{
    auto found = DescriptorSetLayouts.find(MakeKey(layoutID, setIndex));
    if (found == DescriptorSetLayouts.end())
        throw std::runtime_error("No descriptor set layout for layout " + std::to_string(layoutID) + " set " + std::to_string(setIndex));

    const SetLayoutInfo& info = found->second;
    DescriptorTableData* tableData = new DescriptorTableData();
    tableData->RangeCount = info.TableSize;
    if (info.TableSize > 0)
    {
        tableData->RangeStart = ShaderResourceSlots.Claim(info.TableSize);
        tableData->BaseHandle = ShaderResourceHeap->GetGPUDescriptorHandleForHeapStart();
        tableData->BaseHandle.ptr += static_cast<UINT64>(tableData->RangeStart) * ShaderResourceIncrement;
    }

    for (size_t i = 0; i < info.Bindings.size(); i++)
    {
        const DescriptorBinding& layoutBinding = info.Bindings[i];
        auto bindingIt = std::find_if(bindings.begin(), bindings.end(),
            [&](const DescriptorSetBinding& b) { return b.Binding == layoutBinding.Slot; });
        if (bindingIt == bindings.end())
            throw std::runtime_error("Missing resource for binding " + std::to_string(layoutBinding.Slot));

        if (layoutBinding.Type == DescriptorType::DynamicUniformBuffer)
            tableData->DynamicAddresses.push_back(DXBuffer(GetBufferAllocation(bindingIt->Buffer))->GPUAddress);
        else
            WriteDescriptor(layoutBinding, *bindingIt, ShaderResourceCPU(tableData->RangeStart + info.TableOffsets[i]));
    }

    DescriptorSetAllocation allocation;
    allocation.DescriptorAddress = tableData->BaseHandle.ptr;
    allocation.SetKey = MakeKey(layoutID, setIndex);
    allocation.PlatformData = tableData;
    return DescriptorSets.Claim(allocation);
}

void DirectX12BufferAllocator::UpdateDescriptorSet(DescriptorSetHandle set, const std::vector<DescriptorSetBinding>& newBindings)
{
    if (!DescriptorSets.IsLive(set))
        return;

    const DescriptorSetAllocation& allocation = DescriptorSets.Get(set);
    DescriptorTableData* tableData = static_cast<DescriptorTableData*>(allocation.PlatformData);
    auto found = DescriptorSetLayouts.find(allocation.SetKey);
    if (!tableData || found == DescriptorSetLayouts.end())
        return;

    const SetLayoutInfo& info = found->second;
    uint32_t dynamicIndex = 0;
    for (size_t i = 0; i < info.Bindings.size(); i++)
    {
        const DescriptorBinding& layoutBinding = info.Bindings[i];
        bool isDynamic = layoutBinding.Type == DescriptorType::DynamicUniformBuffer;
        auto bindingIt = std::find_if(newBindings.begin(), newBindings.end(),
            [&](const DescriptorSetBinding& b) { return b.Binding == layoutBinding.Slot; });

        if (bindingIt != newBindings.end())
        {
            if (isDynamic)
                tableData->DynamicAddresses[dynamicIndex] = DXBuffer(GetBufferAllocation(bindingIt->Buffer))->GPUAddress;
            else
                WriteDescriptor(layoutBinding, *bindingIt, ShaderResourceCPU(tableData->RangeStart + info.TableOffsets[i]));
        }

        if (isDynamic)
            dynamicIndex++;
    }
}

void DirectX12BufferAllocator::ReleaseDescriptorSet(DescriptorSetAllocation& allocation)
{
    DescriptorTableData* tableData = static_cast<DescriptorTableData*>(allocation.PlatformData);
    if (!tableData)
        return;

    if (tableData->RangeCount > 0)
        ShaderResourceSlots.Release(tableData->RangeStart, tableData->RangeCount);
    delete tableData;
    allocation.PlatformData = nullptr;
}
