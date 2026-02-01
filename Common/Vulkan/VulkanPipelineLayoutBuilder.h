#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "../RHI/RHIStructures.h"

using namespace RHIStructures;

class VulkanPipelineLayoutBuilder
{
public:
    static VkPipelineLayout BuildPipelineLayout(
        VkDevice device,
        const ResourceLayout& layout
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
