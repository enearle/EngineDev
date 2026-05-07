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
    std::map<uint32_t, std::vector<DescriptorSetLayoutBinding>> bindingsBySet;

    for (uint32_t i = 0; i < layouts.size(); i++)
    {
        const ResourceLayout& layout = layouts[i];
        BufferAllocator::GetInstance()->RegisterDescriptorSetLayout(pipelineID, layout, i == layouts.size() - 1);
        
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

    // Reserve space for all sets from 0 to maxSetNumber (inclusive)
    descriptorSetLayouts.reserve(maxSetNumber + 1);

    VulkanBufferAllocator* bufferAlloc = static_cast<VulkanBufferAllocator*>(BufferAllocator::GetInstance());

    // Always create Set 0 first with global samplers
    {
        VkDescriptorSetLayout set0Layout = VulkanCore::Instance().GetGlobalSamplerSetLayout();
        if (set0Layout == VK_NULL_HANDLE)
            throw std::runtime_error("Global sampler set layout is not initialized");
        
        descriptorSetLayouts.push_back(set0Layout);
        outSetToLayoutMapping[0] = 0;
    }

    // Then create layouts for sets 1 through maxSetNumber
    for (uint32_t setIndex = 1; setIndex <= maxSetNumber; setIndex++)
    {
        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        
        // Try to reuse existing layout from BufferAllocator
        descriptorSetLayout = bufferAlloc->GetRegisteredDescriptorSetLayout(pipelineID, setIndex);
        
        if (descriptorSetLayout != VK_NULL_HANDLE)
        {
            // Found registered layout
            descriptorSetLayouts.push_back(descriptorSetLayout);
            outSetToLayoutMapping[setIndex] = setIndex;
        }
        else if (bindingsBySet.find(setIndex) != bindingsBySet.end())
        {
            // Set has bindings but no registered layout - this shouldn't happen normally
            // but we'll create it anyway for robustness
            std::vector<VkDescriptorSetLayoutBinding> vkBindings;
            for (const auto& binding : bindingsBySet[setIndex])
            {
                vkBindings.push_back(binding.binding);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
            layoutInfo.pBindings = vkBindings.data();

            VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
            if (result != VK_SUCCESS)
                throw std::runtime_error("Failed to create descriptor set layout for Set " + std::to_string(setIndex));

            descriptorSetLayouts.push_back(descriptorSetLayout);
            outSetToLayoutMapping[setIndex] = setIndex;
        }
        else
        {
            // Empty set - create an empty layout to maintain set indexing
            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = 0;
            layoutInfo.pBindings = nullptr;

            VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
            if (result != VK_SUCCESS)
                throw std::runtime_error("Failed to create empty descriptor set layout for Set " + std::to_string(setIndex));

            descriptorSetLayouts.push_back(descriptorSetLayout);
            outSetToLayoutMapping[setIndex] = setIndex;
        }
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

