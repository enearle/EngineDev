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

    VkCommandPool CommandPool                           = VK_NULL_HANDLE;
    
    std::vector<VkSemaphore> ImageAvailableSemaphores;
    std::vector<VkSemaphore> RenderFinishedSemaphores;
    std::vector<VkFence> InFlightFences;

    uint32_t SwapChainImageCount = 3;                                       // Maximum number of frames == swapchain size
    
    uint32_t CurrentFrameIndex = 0;                                         // Frame index for CPU work (cycles through command allocators/fences)
    uint32_t CurrentSwapchainImageIndex = 0;                                // Swapchain image index currently acquired for presentation
    
    VkQueue GraphicsQueue                               = VK_NULL_HANDLE;   // Queue for graphics operations.
    VkQueue PresentQueue                                = VK_NULL_HANDLE;   // Queue for presentation.

    VkDebugUtilsMessengerEXT DebugMessenger             = VK_NULL_HANDLE;   // Debug messenger.
    VkSurfaceKHR Surface                                = VK_NULL_HANDLE;   // Interface between Vulkan API and window.
    
    VkSwapchainKHR Swapchain                            = VK_NULL_HANDLE;   // The images to be displayed, written to, and swapped.
    VkExtent2D Extent2D;                                                    // Window dimensions used in current swapchain.
    VkFormat SwapchainFormat;                                               // Colour data packing and colour space.
    std::vector<SwapchainImageData> SwapchainImages;                        // Images and their views used in the swapchain.
    std::vector<VkCommandBuffer> CommandBuffers;                            // Command buffer for each swapchain image.

    
    // Debug
    const std::vector<const char*> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    };
    const std::vector<const char*> ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    bool SwapChainMSAA = false;
    UINT SwapChainMSAASamples = 1;

public:

    static VulkanCore& GetInstance();
    
    void InitVulkan(Window* window, struct CoreInitData data);
    void Cleanup();
    VkDevice GetDevice() const { return Device; }
    VkPhysicalDevice GetPhysicalDevice() const { return PhysicalDevice; }
    VkQueue GetGraphicsQueue() const { return GraphicsQueue; }
    VkExtent2D GetExtent() const { return Extent2D; }
    VkFormat GetSwapchainFormat() const { return SwapchainFormat; }
    const std::vector<SwapchainImageData>& GetSwapchainImages() const { return SwapchainImages; }
    VkCommandBuffer GetCommandBuffer() const { return CommandBuffers[CurrentFrameIndex]; }
    const std::vector<VkSemaphore>& GetImageAvailableSemaphores() const { return ImageAvailableSemaphores; }
    const std::vector<VkSemaphore>& GetRenderFinishedSemaphores() const { return RenderFinishedSemaphores; }
    const std::vector<VkFence>& GetInFlightFences() const { return InFlightFences; }
    uint32_t GetSwapchainImageCount() const { return SwapChainImageCount; }
    void BeginFrame();
    void EndFrame();
    uint32_t GetCurrentFrameIndex() const { return CurrentFrameIndex; }
    uint32_t GetCurrentSwapchainImageIndex() const { return CurrentSwapchainImageIndex; }
    VkImageView GetCurrentSwapchainImageView() const { return SwapchainImages[CurrentSwapchainImageIndex].ImageView; }
    VkImage GetCurrentSwapchainImage() const {return SwapchainImages[CurrentSwapchainImageIndex].ImageHandle; }
    void WaitForGPU();
    
private:
    
    void WaitForFrame(uint32_t frameIndex);
    void CreateInstance();
    void EnableDebugMessenger();
    void CreateSurface();
    void SelectPhysicalDevice();
    void CreateLogicalDevice();
    void CreateSynchronizationPrimitives();
    void CreateCommandPool();
    void CreateSwapchain();
    


    // Debug Support
    VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
        const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
    
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData);

    
    // Swapchain support
    SwapchainDetailsData GetSwapchainDetails();
    VkSurfaceFormatKHR SelectSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR SelectPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    SwapchainDetailsData GetSwapchainDetails(VkPhysicalDevice device);
    

    // Compatability support
    bool CheckInstanceExtensionSupport(std::vector<const char*> extensionsToCheck, uint32_t& erroneousIndex);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    bool CheckDeviceSuitability(VkPhysicalDevice device);
    bool CheckValidationLayerSupport();
    

    //Cleanup
    void DestroySwapchainViews();
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) ;
    
public:
    // Check each device for required queue families.
    QueueFamilyIndicesData FindQueueFamilies(VkPhysicalDevice device);

};
