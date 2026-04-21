#include "VulkanResource.h"

#include <stdexcept>

VkImageView VulkanResource::CreateImageView(VkDevice logicalDevice, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;                                     // Image to create view for
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;                  // Essentially, the number of dimensions in the image data 1D, 2D, 3D, 2D_ARRAY, etc. 
    viewInfo.format = format;                                   // Colour data packing and colour space of the image
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;      // Map RGBA data
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // Subresources allow the view to view only a part of an image
    viewInfo.subresourceRange.aspectMask = aspectFlags;         // Which aspect of image to view colour, stencil, etc. 
    viewInfo.subresourceRange.baseMipLevel = 0;                 // Start mipmap level to view from
    viewInfo.subresourceRange.levelCount = 1;                   // Number of mipmap levels to view
    viewInfo.subresourceRange.baseArrayLayer = 0;               // Start array level to view from
    viewInfo.subresourceRange.layerCount = 1;                   // Number of array levels to view

    VkImageView imageView = VK_NULL_HANDLE;
    VkResult result = vkCreateImageView(logicalDevice, &viewInfo, nullptr, &imageView);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create image view.");
    
    return imageView;
}
