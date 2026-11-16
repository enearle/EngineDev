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
    desc.Usage.Access = UploadMemory;
    desc.Usage.Uniform = true;
    desc.Usage.Storage = true;
    desc.Usage.TransferSource = true;
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
    VkDevice device = VulkanCore::Instance().GetDevice();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = VulkanCore::Instance().GetQueueFamilyIndex();
    if (vkCreateCommandPool(device, &poolInfo, nullptr, &UploadCommandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create the upload command pool.");

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = UploadCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &allocInfo, &UploadCommands) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate the upload command buffer.");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (vkCreateFence(device, &fenceInfo, nullptr, &UploadFence) != VK_SUCCESS)
        throw std::runtime_error("Failed to create the upload fence.");
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
        for (VkDescriptorPool pool : info.Pools)
            vkDestroyDescriptorPool(device, pool, nullptr);
    }
    DescriptorSetLayouts.clear();

    vkDestroyFence(device, UploadFence, nullptr);
    vkDestroyCommandPool(device, UploadCommandPool, nullptr);
    Instance = nullptr;
}

ImageHandle VulkanBufferAllocator::CreateImage(const ImageDesc& imageDesc)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    VulkanImageData* vulkanImageData = new VulkanImageData();
    vulkanImageData->ImageHandle = CreateVulkanImage(imageDesc, &vulkanImageData->Memory);

    if (imageDesc.InitialData)
    {
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;
        CreateStagingBuffer(imageDesc.InitialData, imageDesc.Size, &stagingBuffer, &stagingMemory);

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = vulkanImageData->ImageHandle;
        barrier.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS };

        VkCommandBuffer commands = BeginUpload();

        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent = { imageDesc.Width, imageDesc.Height, 1 };
        vkCmdCopyBufferToImage(commands, stagingBuffer, vulkanImageData->ImageHandle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        SubmitUploadAndWait();
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        vkFreeMemory(device, stagingMemory, nullptr);
    }

    vulkanImageData->ImageView = CreateVulkanImageView(vulkanImageData->ImageHandle, imageDesc, ResolveRange(imageDesc, {}));

    ImageAllocation allocation;
    allocation.Image = vulkanImageData;
    allocation.Desc = imageDesc;
    allocation.Desc.InitialData = nullptr;
    return Images.Claim(allocation);
}

VkCommandBuffer VulkanBufferAllocator::BeginUpload()
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResetCommandBuffer(UploadCommands, 0);
    if (vkBeginCommandBuffer(UploadCommands, &beginInfo) != VK_SUCCESS)
        throw std::runtime_error("Failed to begin the upload command buffer.");
    return UploadCommands;
}

void VulkanBufferAllocator::SubmitUploadAndWait()
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    vkEndCommandBuffer(UploadCommands);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &UploadCommands;
    if (vkQueueSubmit(VulkanCore::Instance().GetGraphicsQueue(), 1, &submitInfo, UploadFence) != VK_SUCCESS)
        throw std::runtime_error("Failed to submit an upload.");
    if (vkWaitForFences(device, 1, &UploadFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
        throw std::runtime_error("Failed to wait for an upload.");
    vkResetFences(device, 1, &UploadFence);
}

void VulkanBufferAllocator::CreateStagingBuffer(const void* data, VkDeviceSize size, VkBuffer* buffer, VkDeviceMemory* memory)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create a staging buffer.");

    VkMemoryRequirements requirements;
    vkGetBufferMemoryRequirements(device, *buffer, &requirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = requirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (vkAllocateMemory(device, &allocInfo, nullptr, memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate staging memory.");
    vkBindBufferMemory(device, *buffer, *memory, 0);

    void* mapped;
    vkMapMemory(device, *memory, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(device, *memory);
}

VkImage VulkanBufferAllocator::CreateVulkanImage(const ImageDesc& imageDesc, VkDeviceMemory* imageMemory)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    VkFormatProperties formatProperties;
    vkGetPhysicalDeviceFormatProperties(VulkanCore::Instance().GetPhysicalDevice(), VulkanFormat(imageDesc.Format), &formatProperties);
    VkFormatFeatureFlags features = formatProperties.optimalTilingFeatures;
    auto require = [&](bool isRequested, VkFormatFeatureFlags feature, const char* use)
    {
        if (isRequested && !(features & feature))
            throw std::runtime_error("Format " + std::to_string(static_cast<int>(imageDesc.Format)) + " cannot be used as " + use + " on this device.");
    };
    require(imageDesc.Usage.Sampled, VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT, "a sampled image");
    require(imageDesc.Usage.RenderTarget, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT, "a render target");
    require(imageDesc.Usage.DepthStencil, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT, "a depth target");
    require(imageDesc.Usage.Storage, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT, "a storage image");

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

VkImageView VulkanBufferAllocator::CreateVulkanImageView(VkImage image, const ImageDesc& imageDesc, const SubresourceRange& resolved)
{
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VulkanImageViewType(imageDesc);
    viewInfo.format = VulkanFormat(imageDesc.Format);
    viewInfo.subresourceRange.aspectMask = IsDepthFormat(imageDesc.Format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = resolved.BaseMip;
    viewInfo.subresourceRange.levelCount = resolved.MipCount;
    viewInfo.subresourceRange.baseArrayLayer = resolved.BaseLayer;
    viewInfo.subresourceRange.layerCount = resolved.LayerCount;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(VulkanCore::Instance().GetDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view.");
    return imageView;
}

VkImageView VulkanBufferAllocator::GetImageView(ImageHandle image, SubresourceRange range)
{
    ImageAllocation& allocation = Images.Get(image);
    VulkanImageData* imageData = static_cast<VulkanImageData*>(allocation.Image);
    SubresourceRange resolved = ResolveRange(allocation.Desc, range);
    if (IsWholeImage(allocation.Desc, resolved))
        return imageData->ImageView;

    uint64_t key = RangeKey(resolved);
    for (const VulkanRangeView& view : imageData->RangeViews)
        if (view.Key == key)
            return view.View;

    VkImageView view = CreateVulkanImageView(imageData->ImageHandle, allocation.Desc, resolved);
    imageData->RangeViews.push_back({ key, view });
    return view;
}

void VulkanBufferAllocator::DestroyRangeViews(void* vulkanImageData)
{
    VulkanImageData* imageData = static_cast<VulkanImageData*>(vulkanImageData);
    for (const VulkanRangeView& view : imageData->RangeViews)
        vkDestroyImageView(VulkanCore::Instance().GetDevice(), view.View, nullptr);
    imageData->RangeViews.clear();
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
                vkBindings.push_back(vkBinding);
                info.PoolSizes.push_back({ vkBinding.descriptorType, vkBinding.descriptorCount * DescriptorSetsPerPool });
            }
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
        layoutInfo.pBindings = vkBindings.empty() ? nullptr : vkBindings.data();
        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &info.Layout) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor set layout");

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
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageView = GetImageView(bindingIt->Image, bindingIt->Range);
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
    VkDevice device = VulkanCore::Instance().GetDevice();
    auto found = DescriptorSetLayouts.find(MakeKey(layoutID, setIndex));
    if (found == DescriptorSetLayouts.end() || found->second.PoolSizes.empty())
        throw std::runtime_error("No descriptor set layout with bindings for layout " + std::to_string(layoutID) + " set " + std::to_string(setIndex));

    DescriptorSetLayoutInfo& layoutInfo = found->second;

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layoutInfo.Layout;

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkResult result = VK_ERROR_OUT_OF_POOL_MEMORY;
    if (!layoutInfo.Pools.empty())
    {
        allocInfo.descriptorPool = layoutInfo.Pools.back();
        result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    }

    if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
    {
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(layoutInfo.PoolSizes.size());
        poolInfo.pPoolSizes = layoutInfo.PoolSizes.data();
        poolInfo.maxSets = DescriptorSetsPerPool;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

        VkDescriptorPool pool;
        if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor pool");
        layoutInfo.Pools.push_back(pool);

        allocInfo.descriptorPool = pool;
        result = vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
    }

    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set");

    WriteDescriptors(descriptorSet, layoutInfo.Bindings, bindings, true);

    DescriptorSetAllocation allocation;
    allocation.DescriptorAddress = reinterpret_cast<uint64_t>(descriptorSet);
    allocation.SetKey = MakeKey(layoutID, setIndex);
    allocation.PlatformData = allocInfo.descriptorPool;
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
    // A set whose layouts were already retired went away with its pool.
    if (!DescriptorSetLayouts.contains(allocation.SetKey))
        return;

    VkDescriptorSet descriptorSet = reinterpret_cast<VkDescriptorSet>(allocation.DescriptorAddress);
    vkFreeDescriptorSets(VulkanCore::Instance().GetDevice(), static_cast<VkDescriptorPool>(allocation.PlatformData), 1, &descriptorSet);
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
        for (VkDescriptorPool pool : it->second.Pools)
            vkDestroyDescriptorPool(device, pool, nullptr);
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

    DestroyRangeViews(imageData);
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
    VulkanImageData* imageData = static_cast<VulkanImageData*>(Images.Get(image).Image);
    DestroyRangeViews(imageData);
    delete imageData;
    Images.Release(image);
}

BufferHandle VulkanBufferAllocator::CreateBuffer(const BufferDesc& bufferDesc)
{
    VulkanBufferData* vulkanBufferData = new VulkanBufferData();
    VkBufferUsageFlags bufferFlags = VulkanBufferUsage(bufferDesc.Usage);
    VkMemoryPropertyFlags memoryFlags = VulkanMemoryType(bufferDesc.Usage.Access);
    VkDevice device = VulkanCore::Instance().GetDevice();
    VkPhysicalDevice physicalDevice = VulkanCore::Instance().GetPhysicalDevice();


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

    VkMemoryAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memoryFlags);

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

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    CreateStagingBuffer(srcData, size, &stagingBuffer, &stagingMemory);

    VkCommandBuffer commands = BeginUpload();
    VkBufferCopy copyRegion = { 0, 0, size };
    vkCmdCopyBuffer(commands, stagingBuffer, dstBuffer, 1, &copyRegion);
    SubmitUploadAndWait();

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
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
    D3D12_HEAP_TYPE heapType = DXMemoryType(bufferDesc.Usage.Access);

    // Upload and readback heaps fix the resource state; default buffers start in COMMON and are
    // promoted on first use.
    D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
    if (heapType == D3D12_HEAP_TYPE_UPLOAD)
        initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
    else if (heapType == D3D12_HEAP_TYPE_READBACK)
        initialState = D3D12_RESOURCE_STATE_COPY_DEST;

    uint64_t size = (bufferDesc.Size + 255) & ~255ull;
    CD3DX12_HEAP_PROPERTIES heapProperties(heapType);
    D3D12_RESOURCE_FLAGS flags = heapType == D3D12_HEAP_TYPE_DEFAULT && bufferDesc.Usage.Storage
        ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE;
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(size, flags);

    ComPtr<ID3D12Resource> buffer;
    device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, initialState, nullptr,
        IID_PPV_ARGS(buffer.GetAddressOf())) >> ERROR_HANDLER;

    void* mappedAddress = nullptr;
    if (heapType != D3D12_HEAP_TYPE_DEFAULT)
    {
        D3D12_RANGE noRead = { 0, 0 };
        buffer->Map(0, heapType == D3D12_HEAP_TYPE_UPLOAD ? &noRead : nullptr, &mappedAddress) >> ERROR_HANDLER;
        if (bufferDesc.InitialData && heapType == D3D12_HEAP_TYPE_UPLOAD)
            memcpy(mappedAddress, bufferDesc.InitialData, bufferDesc.Size);
    }
    else if (bufferDesc.InitialData)
    {
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        ComPtr<ID3D12Resource> uploadBuffer;
        device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())) >> ERROR_HANDLER;

        void* uploadAddress = nullptr;
        uploadBuffer->Map(0, nullptr, &uploadAddress) >> ERROR_HANDLER;
        memcpy(uploadAddress, bufferDesc.InitialData, bufferDesc.Size);
        uploadBuffer->Unmap(0, nullptr);

        ID3D12GraphicsCommandList* commands = BeginUpload();
        commands->CopyBufferRegion(buffer.Get(), 0, uploadBuffer.Get(), 0, bufferDesc.Size);
        SubmitUploadAndWait();
    }

    DX12BufferData* bufferData = new DX12BufferData();
    bufferData->Buffer = buffer;
    bufferData->GPUAddress = buffer->GetGPUVirtualAddress();

    BufferAllocation allocation;
    allocation.Buffer = bufferData;
    allocation.Size = bufferDesc.Size;
    allocation.Address = mappedAddress;
    allocation.Usage = bufferDesc.Usage;
    allocation.IsMapped = mappedAddress != nullptr;
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

    device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(UploadAllocator.GetAddressOf())) >> ERROR_HANDLER;
    device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, UploadAllocator.Get(), nullptr, IID_PPV_ARGS(UploadList.GetAddressOf())) >> ERROR_HANDLER;
    UploadList->Close() >> ERROR_HANDLER;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(UploadFence.GetAddressOf())) >> ERROR_HANDLER;
    UploadEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
}

DirectX12BufferAllocator::~DirectX12BufferAllocator()
{
    ReleaseAllPending();
    Buffers.ForEachLive([this](BufferAllocation& allocation) { ReleaseBuffer(allocation); });
    Images.ForEachLive([this](ImageAllocation& allocation) { ReleaseImage(allocation); });
    DescriptorSets.ForEachLive([this](DescriptorSetAllocation& allocation) { ReleaseDescriptorSet(allocation); });
    CloseHandle(UploadEvent);
    Instance = nullptr;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12BufferAllocator::ShaderResourceCPU(uint32_t index) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = ShaderResourceHeap->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(index) * ShaderResourceIncrement;
    return handle;
}

ID3D12GraphicsCommandList* DirectX12BufferAllocator::BeginUpload()
{
    UploadAllocator->Reset() >> ERROR_HANDLER;
    UploadList->Reset(UploadAllocator.Get(), nullptr) >> ERROR_HANDLER;
    return UploadList.Get();
}

void DirectX12BufferAllocator::SubmitUploadAndWait()
{
    UploadList->Close() >> ERROR_HANDLER;
    ID3D12CommandList* commandLists[] = { UploadList.Get() };
    ID3D12CommandQueue* queue = D3DCore::Instance().GetCommandQueue().Get();
    queue->ExecuteCommandLists(1, commandLists);

    UploadFenceValue++;
    queue->Signal(UploadFence.Get(), UploadFenceValue) >> ERROR_HANDLER;
    if (UploadFence->GetCompletedValue() < UploadFenceValue)
    {
        UploadFence->SetEventOnCompletion(UploadFenceValue, UploadEvent) >> ERROR_HANDLER;
        WaitForSingleObject(UploadEvent, INFINITE);
    }
}

ImageHandle DirectX12BufferAllocator::CreateImage(const ImageDesc& imageDesc)
{
    ID3D12Device* device = D3DCore::Instance().GetDevice().Get();
    bool isDepth = IsDepthFormat(imageDesc.Format);
    uint32_t arrayLayers = (std::max)(imageDesc.ArrayLayers, 1u);

    D3D12_FEATURE_DATA_FORMAT_SUPPORT support = {};
    support.Format = isDepth && imageDesc.Usage.Sampled ? SampledDepthFormat(imageDesc.Format) : DXFormat(imageDesc.Format);
    device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &support, sizeof(support)) >> ERROR_HANDLER;
    D3D12_FEATURE_DATA_FORMAT_SUPPORT targetSupport = { DXFormat(imageDesc.Format) };
    device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &targetSupport, sizeof(targetSupport)) >> ERROR_HANDLER;
    auto require = [&](bool isRequested, bool isSupported, const char* use)
    {
        if (isRequested && !isSupported)
            throw std::runtime_error("Format " + std::to_string(static_cast<int>(imageDesc.Format)) + " cannot be used as " + use + " on this device.");
    };
    require(imageDesc.Usage.Sampled, support.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE, "a sampled image");
    require(imageDesc.Usage.RenderTarget, targetSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET, "a render target");
    require(imageDesc.Usage.DepthStencil, targetSupport.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL, "a depth target");
    require(imageDesc.Usage.Storage, (targetSupport.Support1 & D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW)
        && (targetSupport.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE), "a storage image");

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
        if (arrayLayers != 1)
            throw std::runtime_error("Image upload supports one layer; it fills mip 0.");


        const UINT64 uploadBufferSize = GetRequiredIntermediateSize(imageResource.Get(), 0, 1);
        CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

        ComPtr<ID3D12Resource> uploadBuffer;
        device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())) >> ERROR_HANDLER;

        const uint64_t bytesPerPixel = FormatSize(imageDesc.Format);
        if (imageDesc.Size < static_cast<uint64_t>(imageDesc.Width) * imageDesc.Height * bytesPerPixel)
            throw std::runtime_error("ImageDesc::Size is smaller than Width * Height * the format size.");

        D3D12_SUBRESOURCE_DATA subresource = {};
        subresource.pData = imageDesc.InitialData;
        subresource.RowPitch = static_cast<LONG_PTR>(imageDesc.Width * bytesPerPixel);
        subresource.SlicePitch = subresource.RowPitch * imageDesc.Height;
        ID3D12GraphicsCommandList* cmdList = BeginUpload();
        UpdateSubresources(cmdList, imageResource.Get(), uploadBuffer.Get(), 0, 0, 1, &subresource);

        CD3DX12_RESOURCE_BARRIER toShaderRead = CD3DX12_RESOURCE_BARRIER::Transition(imageResource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
        cmdList->ResourceBarrier(1, &toShaderRead);
        SubmitUploadAndWait();
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

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12BufferAllocator::GetRenderTargetView(ImageHandle image, SubresourceRange range)
{
    ImageAllocation& allocation = Images.Get(image);
    DX12ImageData* imageData = static_cast<DX12ImageData*>(allocation.Image);
    SubresourceRange resolved = ResolveRange(allocation.Desc, range);
    if (resolved.BaseMip == 0 && resolved.BaseLayer == 0 && resolved.LayerCount == (std::max)(allocation.Desc.ArrayLayers, 1u))
        return imageData->RenderTargetView;

    uint64_t key = RangeKey(resolved);
    for (const DX12RangeView& view : imageData->RangeViews)
        if (!view.IsDepth && view.Key == key)
            return view.Handle;

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.Format = DXFormat(allocation.Desc.Format);
    if (allocation.Desc.ArrayLayers > 1)
    {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
        rtvDesc.Texture2DArray.MipSlice = resolved.BaseMip;
        rtvDesc.Texture2DArray.FirstArraySlice = resolved.BaseLayer;
        rtvDesc.Texture2DArray.ArraySize = resolved.LayerCount;
    }
    else
    {
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = resolved.BaseMip;
    }

    DX12RangeView view;
    view.Key = key;
    view.Slot = RenderTargetSlots.Claim(1);
    view.Handle = RenderTargetHeap->GetCPUDescriptorHandleForHeapStart();
    view.Handle.ptr += static_cast<SIZE_T>(view.Slot) * RenderTargetIncrement;
    D3DCore::Instance().GetDevice()->CreateRenderTargetView(imageData->Image.Get(), &rtvDesc, view.Handle);
    imageData->RangeViews.push_back(view);
    return view.Handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DirectX12BufferAllocator::GetDepthStencilView(ImageHandle image, SubresourceRange range)
{
    ImageAllocation& allocation = Images.Get(image);
    DX12ImageData* imageData = static_cast<DX12ImageData*>(allocation.Image);
    SubresourceRange resolved = ResolveRange(allocation.Desc, range);
    if (resolved.BaseMip == 0 && resolved.BaseLayer == 0 && resolved.LayerCount == (std::max)(allocation.Desc.ArrayLayers, 1u))
        return imageData->DepthStencilView;

    uint64_t key = RangeKey(resolved);
    for (const DX12RangeView& view : imageData->RangeViews)
        if (view.IsDepth && view.Key == key)
            return view.Handle;

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXFormat(allocation.Desc.Format);
    if (allocation.Desc.ArrayLayers > 1)
    {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.MipSlice = resolved.BaseMip;
        dsvDesc.Texture2DArray.FirstArraySlice = resolved.BaseLayer;
        dsvDesc.Texture2DArray.ArraySize = resolved.LayerCount;
    }
    else
    {
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Texture2D.MipSlice = resolved.BaseMip;
    }

    DX12RangeView view;
    view.Key = key;
    view.IsDepth = true;
    view.Slot = DepthStencilSlots.Claim(1);
    view.Handle = DepthStencilHeap->GetCPUDescriptorHandleForHeapStart();
    view.Handle.ptr += static_cast<SIZE_T>(view.Slot) * DepthStencilIncrement;
    D3DCore::Instance().GetDevice()->CreateDepthStencilView(imageData->Image.Get(), &dsvDesc, view.Handle);
    imageData->RangeViews.push_back(view);
    return view.Handle;
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
    for (const DX12RangeView& view : imageData->RangeViews)
        (view.IsDepth ? DepthStencilSlots : RenderTargetSlots).Release(view.Slot, 1);
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
        SubresourceRange range = ResolveRange(desc, binding.Range);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format = IsDepthFormat(desc.Format) ? SampledDepthFormat(desc.Format) : DXFormat(desc.Format);
        if (desc.ArrayLayers > 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MostDetailedMip = range.BaseMip;
            srvDesc.Texture2DArray.MipLevels = range.MipCount;
            srvDesc.Texture2DArray.FirstArraySlice = range.BaseLayer;
            srvDesc.Texture2DArray.ArraySize = range.LayerCount;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MostDetailedMip = range.BaseMip;
            srvDesc.Texture2D.MipLevels = range.MipCount;
        }
        device->CreateShaderResourceView(static_cast<DX12ImageData*>(imageAllocation.Image)->Image.Get(), &srvDesc, destination);
        break;
    }
    case DescriptorType::StorageImage:
    {
        const ImageAllocation& imageAllocation = GetImageAllocation(binding.Image);
        const ImageDesc& desc = imageAllocation.Desc;
        SubresourceRange range = ResolveRange(desc, binding.Range);

        D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
        uavDesc.Format = DXFormat(desc.Format);
        if (desc.ArrayLayers > 1)
        {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            uavDesc.Texture2DArray.MipSlice = range.BaseMip;
            uavDesc.Texture2DArray.FirstArraySlice = range.BaseLayer;
            uavDesc.Texture2DArray.ArraySize = range.LayerCount;
        }
        else
        {
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            uavDesc.Texture2D.MipSlice = range.BaseMip;
        }
        device->CreateUnorderedAccessView(static_cast<DX12ImageData*>(imageAllocation.Image)->Image.Get(), nullptr, &uavDesc, destination);
        break;
    }
    case DescriptorType::StorageBuffer:
    case DescriptorType::ReadWriteStorageBuffer:
    {
        if (layoutBinding.Stride == 0)
            throw std::runtime_error("A storage buffer binding needs a Stride.");

        const BufferAllocation& bufferAllocation = GetBufferAllocation(binding.Buffer);
        UINT elementCount = static_cast<UINT>(bufferAllocation.Size / layoutBinding.Stride);
        ID3D12Resource* resource = DXBuffer(bufferAllocation)->Buffer.Get();

        if (layoutBinding.Type == DescriptorType::StorageBuffer)
        {
            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
            srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srvDesc.Format = DXGI_FORMAT_UNKNOWN;
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            srvDesc.Buffer.NumElements = elementCount;
            srvDesc.Buffer.StructureByteStride = layoutBinding.Stride;
            device->CreateShaderResourceView(resource, &srvDesc, destination);
        }
        else
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
            uavDesc.Format = DXGI_FORMAT_UNKNOWN;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.NumElements = elementCount;
            uavDesc.Buffer.StructureByteStride = layoutBinding.Stride;
            device->CreateUnorderedAccessView(resource, nullptr, &uavDesc, destination);
        }
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
