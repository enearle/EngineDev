#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <map> 
#include "../RHI/RHIStructures.h"

class VulkanPipelineLayoutBuilder
{
public:
    static VkPipelineLayout BuildPipelineLayout(
        uint32_t pipelineID,
        const std::vector<RHIStructures::ResourceLayout>& layouts, std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, 
        const std::vector<RHIStructures::PipelineConstant>& constants, std::map<uint32_t, uint32_t>& outSetToLayoutMapping
    );

private:
    struct DescriptorSetLayoutBinding
    {
        VkDescriptorSetLayoutBinding binding;
        std::vector<VkSampler> samplers; 
    };

    static DescriptorSetLayoutBinding CreateDescriptorSetLayoutBinding(
        const RHIStructures::DescriptorBinding& binding,
        const RHIStructures::ShaderStageMask& visibleStages
    );
};
