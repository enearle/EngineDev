#include "VulkanPipelineLayoutBuilder.h"

#include <map>
#include <stdexcept>

VkPipelineLayout VulkanPipelineLayoutBuilder::BuildPipelineLayout(
    VkDevice device,
    const ResourceLayout& layout)
{
    if (!device)
        throw std::runtime_error("Vulkan device is null");

    if (layout.Bindings.empty())
    {
        // Create an empty pipeline layout
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
    
    // Create descriptor set layouts in order
    std::map<uint32_t, std::vector<DescriptorSetLayoutBinding>> bindingsBySet;
    uint32_t bindingIndex = 0;
    
    for (const auto& binding : layout.Bindings)
    {
        auto setLayoutBinding = CreateDescriptorSetLayoutBinding(binding, bindingIndex);
        bindingsBySet[binding.Set].push_back(setLayoutBinding);
        bindingIndex++;
    }
    
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
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

        VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
        VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout);
        
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create descriptor set layout");
        
        descriptorSetLayouts.push_back(descriptorSetLayout);
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
    
    for (VkDescriptorSetLayout layout : descriptorSetLayouts)
    {
        vkDestroyDescriptorSetLayout(device, layout, nullptr);
    }

    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");

    return pipelineLayout;
}

VulkanPipelineLayoutBuilder::DescriptorSetLayoutBinding 
VulkanPipelineLayoutBuilder::CreateDescriptorSetLayoutBinding(
    const DescriptorBinding& binding,
    uint32_t& bindingIndex)
{
    DescriptorSetLayoutBinding result{};

    result.binding.binding = binding.Slot;
    result.binding.descriptorType = GetVulkanDescriptorType(binding.Type);
    result.binding.descriptorCount = binding.Count > 0 ? binding.Count : 1;
    result.binding.stageFlags = GetShaderStageFlags(binding.VisibleStages);
    result.binding.pImmutableSamplers = nullptr;

    return result;
}

VkDescriptorType VulkanPipelineLayoutBuilder::GetVulkanDescriptorType(DescriptorType type)
{
    switch (type)
    {
    case DescriptorType::UniformBuffer:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorType::StorageBuffer:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorType::SampledImage:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case DescriptorType::StorageImage:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    default:
        throw std::runtime_error("Unknown descriptor type");
    }
}

VkShaderStageFlags VulkanPipelineLayoutBuilder::GetShaderStageFlags(const ShaderStageMask& stages)
{
    VkShaderStageFlags flags = 0;
    
    if (stages.Vertex)      flags |= VK_SHADER_STAGE_VERTEX_BIT;
    if (stages.Fragment)    flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (stages.Geometry)    flags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (stages.TessControl) flags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (stages.TessEval)    flags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (stages.Compute)     flags |= VK_SHADER_STAGE_COMPUTE_BIT;
    
    return flags;
}
