#pragma once
#include "../Windows/WindowsHeaders.h"

class VulkanResource
{
public:
    static VkImageView CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
};
