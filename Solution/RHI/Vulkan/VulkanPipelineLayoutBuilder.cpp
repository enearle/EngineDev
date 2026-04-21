#include "../RHI/BufferAllocator.h"
#include "VulkanPipelineLayoutBuilder.h"

#include <iostream>
#include <map>
#include <stdexcept>
#include "VulkanCore.h"

using namespace RHIStructures;

VkPipelineLayout VulkanPipelineLayoutBuilder::BuildPipelineLayout(
    uint32_t pipelineID,
    const std::vector<ResourceLayout>& layouts, 
    std::vector<VkDescriptorSetLayout>& descriptorSetLayouts,
    const std::vector<PipelineConstant>& constants, 
    std::map<uint32_t, uint32_t>& outSetToLayoutMapping)
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    if (!device)
        throw std::runtime_error("Vulkan device is null");
    
    // Create an empty pipeline layout if needed
    if (layouts.size() == 1 && layouts[0].Bindings.empty() && constants.empty())
    {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount = 0;
        pipelineLayoutInfo.pPushConstantRanges = nullptr;

        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
        
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create empty pipeline layout");
        
        return pipelineLayout;
    }

    // Create descriptor set layouts
    // Collect all bindings from all resource layouts into a single map
    // All layouts and descriptors are tracked in the buffer allocator for cleanup
    std::map<uint32_t, std::vector<DescriptorSetLayoutBinding>> bindingsBySet;

    for (uint32_t i = 0; i < layouts.size(); i++)
    {
        const ResourceLayout& layout = layouts[i];
        BufferAllocator::GetInstance()->RegisterDescriptorSetLayout(pipelineID, layout);
        
        for (const DescriptorBinding& binding : layout.Bindings)
        {
            DescriptorSetLayoutBinding setLayoutBinding = CreateDescriptorSetLayoutBinding(binding, layout.VisibleStages);
            bindingsBySet[binding.Set].push_back(setLayoutBinding);
        }
    }

    // Find the maximum set number to determine array size
    uint32_t maxSetNumber = 0;
    for (const auto& [setIndex, bindings] : bindingsBySet)
    {
        if (setIndex > maxSetNumber)
            maxSetNumber = setIndex;
    }

    // Create descriptor set layout array with proper indexing
    descriptorSetLayouts.reserve(maxSetNumber + 1);

    VulkanBufferAllocator* bufferAlloc = static_cast<VulkanBufferAllocator*>(BufferAllocator::GetInstance());

    for (uint32_t setIndex = 0; setIndex <= maxSetNumber; setIndex++)
    {
        // Try to reuse existing layout from BufferAllocator
        VkDescriptorSetLayout descriptorSetLayout = bufferAlloc->GetRegisteredDescriptorSetLayout(pipelineID, setIndex);
    
        if (descriptorSetLayout != VK_NULL_HANDLE)
        {
            // Reuse set from main pipeline
            outSetToLayoutMapping[setIndex] = setIndex;
        }
        else
        {
            // Layout should have been created in BufferAllocator
            throw std::runtime_error("Descriptor set layout not found for pipeline " 
                + std::to_string(pipelineID) + " set " + std::to_string(setIndex));
        }
    
        descriptorSetLayouts.push_back(descriptorSetLayout);
    }
    
    // Define push constant ranges
    std::vector<VkPushConstantRange> pushConstantRanges;
    size_t pushConstantRangeOffset = 0;
    for (const PipelineConstant& constant : constants)
    {
        VkPushConstantRange range{};
        range.stageFlags = VulkanShaderStageFlags(constant.VisibleStages);
        range.offset = pushConstantRangeOffset;
        range.size = constant.Size;
        pushConstantRanges.push_back(range);
        pushConstantRangeOffset += constant.Size;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.empty() ? nullptr : descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
    pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.empty() ? nullptr : pushConstantRanges.data();

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);

    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    return pipelineLayout;
}

VulkanPipelineLayoutBuilder::DescriptorSetLayoutBinding 
VulkanPipelineLayoutBuilder::CreateDescriptorSetLayoutBinding(const DescriptorBinding& binding, const ShaderStageMask& visibleStages)
{
    DescriptorSetLayoutBinding result{};

    result.binding.binding = binding.Slot;
    result.binding.descriptorType = VulkanDescriptorType(binding.Type);
    result.binding.descriptorCount = binding.Count > 0 ? binding.Count : 1;
    result.binding.stageFlags = VulkanShaderStageFlags(visibleStages);
    
    switch (binding.Sampler)
    {
    case SamplerType::Linear:
        result.binding.pImmutableSamplers = VulkanCore::Instance().GetLinearSampler();
        break;
    case SamplerType::Nearest:
        result.binding.pImmutableSamplers = VulkanCore::Instance().GetNearestSampler();
        break;
    }

    return result;
}

