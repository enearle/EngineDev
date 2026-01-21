#pragma once
#include "../Windows/WindowsHeaders.h"
#include <algorithm>
#include <limits>
#include <vector>
#include <iostream>
#include <set>
#include "../Vulkan/Structs.h"

using namespace VulkanStructs;

class Window;

class VulkanCore
{
    VulkanCore() = default;
    bool Initialized = false;
    
    Window* RenderWindow;

    VkInstance VulkanInstance                           = VK_NULL_HANDLE;   // Instance of Vulkan API.
    VkPhysicalDevice PhysicalDevice                     = VK_NULL_HANDLE;   // GPU.
    VkDevice Device                                     = VK_NULL_HANDLE;   // Interface for GPU.

    VkQueue GraphicsQueue                               = VK_NULL_HANDLE;   // Queue for graphics operations.
    VkQueue PresentQueue                                = VK_NULL_HANDLE;   // Queue for presentation.

    VkDebugUtilsMessengerEXT DebugMessenger             = VK_NULL_HANDLE;   // Debug messenger.
    VkSurfaceKHR Surface                                = VK_NULL_HANDLE;   // Interface between Vulkan API and window.
    
    VkSwapchainKHR Swapchain                            = VK_NULL_HANDLE;   // The images to be displayed, written to, and swapped.
    size_t MaxFramesInFlight;                                               // Maximum number of frames == swapchain size
    VkExtent2D Extent2D;                                                    // Window dimensions used in current swapchain.
    VkFormat SwapchainFormat;                                               // Colour data packing and colour space.
    std::vector<SwapchainImageData> SwapchainImages;                           // Images and their views used in the swapchain.
    std::vector<VkFramebuffer> SwapchainFramebuffers;                       // Framebuffers for each swapchain image.
    std::vector<VkCommandBuffer> SwapchainCommandBuffers;                   // Command buffer for each swapchain image.

    
    // Validation constants
    //################
    const std::vector<const char*> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    const std::vector<const char*> ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

public:

    static VulkanCore& GetInstance();
    
    void InitVulkan(Window* window);

    void Cleanup();
    
private:
    // Create an instance of Vulkan API.
    void CreateInstance();

    // Create a logical device to interface with the GPU.
    void CreateLogicalDevice();

    // Set up a debug messenger.
    void EnableDebugMessenger();

    // Create debug messenger.
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);

    // Create a surface.
    void CreateSurface();

    // Create swapchain
    void CreateSwapchain();

    // Get swapchain details
    SwapchainDetailsData GetSwapchainDetails();

    // Select swapchain surfac format
    VkSurfaceFormatKHR SelectSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

    // Select swapchain present mode
    VkPresentModeKHR SelectPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    // Select swapchain extent
    VkExtent2D SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities);

    // Check the Vulkan instance for required extension support.
    bool CheckInstanceExtensionSupport(std::vector<const char*> extensionsToCheck, uint32_t& erroneousIndex);

    // Check device Extension support.
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);

    // Check that a device supports Vulkan.
    bool CheckDeviceSuitability(VkPhysicalDevice device);

    // Check validation layer is supported.
    bool CheckValidationLayerSupport();
    
    // --- Assignment

    // Find GPU that supports Vulkan.
    void GetPhysicalDevice();
    
    SwapchainDetailsData GetSwapchainDetails(VkPhysicalDevice device);

    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    // --- Cleanup
    void DestroySwapchainViews();
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) ;
    
public:
    // Check each device for required queue families.
    QueueFamilyIndicesData FindQueueFamilies(VkPhysicalDevice device);

};
