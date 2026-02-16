#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "../RHI/RHIStructures.h"

using namespace RHIStructures;

class VulkanPipelineLayoutBuilder
{
public:
    static VkPipelineLayout BuildPipelineLayout(
        uint32_t pipelineID,
        const std::vector<ResourceLayout>& layouts, std::vector<VkDescriptorSetLayout>& descriptorSetLayouts, const std::
        vector<PipelineConstant>& constants
    );

private:
    struct DescriptorSetLayoutBinding
    {
        VkDescriptorSetLayoutBinding binding;
        std::vector<VkSampler> samplers;  // For keeping samplers alive
    };

    static DescriptorSetLayoutBinding CreateDescriptorSetLayoutBinding(
        const DescriptorBinding& binding,
        const ShaderStageMask& visibleStages
    );
};
