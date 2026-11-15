#pragma once

namespace VulkanStructs
{
    struct SwapchainDetailsData
    {
        VkSurfaceCapabilitiesKHR Capabilities;          // Surface dimensions, etc. 
        std::vector<VkSurfaceFormatKHR> Formats;        // Colour data packing and colour space.
        std::vector<VkPresentModeKHR> PresentModes;     // How the swap chain passes data to the monitor
                                                        // IMMEDIATE, MAILBOX, FIFO, etc.
                                                        // Methods to balance between taring and latency.
        bool IsValid()
        {
            return !Formats.empty() && !PresentModes.empty();
        }
    };

    struct VulkanRangeView
    {
        uint64_t Key = 0;
        VkImageView View = VK_NULL_HANDLE;
    };

    struct VulkanImageData
    {
        VkImage ImageHandle =   VK_NULL_HANDLE;     // Handle to image.
        VkImageView ImageView = VK_NULL_HANDLE;     // View of the whole image.
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        std::vector<VulkanRangeView> RangeViews;    // Views of parts of the image, made on first use.
    };

    struct QueueFamilyIndicesData
    {
        int GraphicsFamily = -1;        // Index of graphics queue family.
        int PresentFamily = -1;         // Index of presentation queue family.

        bool IsValid()
        {
            return GraphicsFamily != -1 && PresentFamily != -1;
        }
    };

    struct VulkanBufferData
    {
        VkBuffer Buffer = VK_NULL_HANDLE;
        VkDeviceMemory Memory = VK_NULL_HANDLE;
    };
}
