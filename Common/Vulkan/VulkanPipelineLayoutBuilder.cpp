#include "../../Common/RHI/BufferAllocator.h"
#include "VulkanPipelineLayoutBuilder.h"
#include <map>
#include <stdexcept>
#include "VulkanCore.h"

VkPipelineLayout VulkanPipelineLayoutBuilder::BuildPipelineLayout(uint32_t pipelineID,
    const std::vector<ResourceLayout>& layouts, std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, const std::vector<PipelineConstant>& constants)
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
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
    for (uint32_t i = 0; i < layouts.size(); i++)
    {
        const ResourceLayout& layout = layouts[i];
        BufferAllocator::GetInstance()->RegisterDescriptorSetLayout(pipelineID, layout);
        
        std::map<uint32_t, std::vector<DescriptorSetLayoutBinding>> bindingsBySet;
        uint32_t bindingIndex = 0;
    
        for (const DescriptorBinding& binding : layout.Bindings)
        {
            DescriptorSetLayoutBinding setLayoutBinding = CreateDescriptorSetLayoutBinding(binding, layout.VisibleStages);
            bindingsBySet[binding.Set].push_back(setLayoutBinding);
            bindingIndex++;
        }
    
        descriptorSetLayouts.reserve(bindingsBySet.size());
    
        for (const auto& [setIndex, bindings] : bindingsBySet)
        {
            std::vector<VkDescriptorSetLayoutBinding> vkBindings;
            vkBindings.reserve(bindings.size());
        
            for (const auto& binding : bindings)
            {
                vkBindings.push_back(binding.binding);
            }

            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = static_cast<uint32_t>(vkBindings.size());
            layoutInfo.pBindings = vkBindings.data();
            layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;

            VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
            VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
        
            if (result != VK_SUCCESS)
                throw std::runtime_error("Failed to create descriptor set layout");
        
            descriptorSetLayouts.push_back(descriptorSetLayout);
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
    result.binding.pImmutableSamplers = VulkanCore::GetInstance().GetGenericSampler();

    return result;
}

