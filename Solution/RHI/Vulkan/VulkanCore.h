#pragma once
#include "../Windows/WindowsHeaders.h"
#include <vector>
#include "../RHI/RHIStructures.h"
#include <map>
#include "../GraphicsSettings.h"
#include "../Vulkan/VulkanStructs.h"


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
    uint32_t CurrentSwapchainImageIndex = 0;
    uint64_t FrameNumber = 0;                                // Swapchain image index currently acquired for presentation

    VkQueue GraphicsQueue                               = VK_NULL_HANDLE;   // Queue for graphics operations.
    VkQueue PresentQueue                                = VK_NULL_HANDLE;   // Queue for presentation.

    VkDebugUtilsMessengerEXT DebugMessenger             = VK_NULL_HANDLE;   // Debug messenger.
    VkSurfaceKHR Surface                                = VK_NULL_HANDLE;   // Interface between Vulkan API and window.

    VkSwapchainKHR Swapchain                            = VK_NULL_HANDLE;   // The images to be displayed, written to, and swapped.
    VkExtent2D Extent2D;                                                    // Window dimensions used in current swapchain.
    VkFormat SwapchainFormat;                                               // Colour data packing and colour space.
    std::vector<VulkanImageData> SwapchainImages;                           // Images and their views used in the swapchain.
    std::vector<VkCommandBuffer> CommandBuffers;                            // Command buffer for each swapchain image.

    VkSampler GlobalSamplerObjects[RHIStructures::GlobalSamplerCount] = {};
    bool IsSwapchainRecreateNeeded = false;
    VkDescriptorSetLayout GlobalSamplerSetLayout        = VK_NULL_HANDLE;
    VkDescriptorPool GlobalSamplerDescriptorPool        = VK_NULL_HANDLE;
    VkDescriptorSet GlobalSamplerSet                    = VK_NULL_HANDLE;

    VkRenderPass OverlayRenderPass          = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> OverlayFramebuffers;
    std::vector<VkImage> OverlayStencilImages;
    std::vector<VkDeviceMemory> OverlayStencilMemory;
    std::vector<VkImageView> OverlayStencilImageViews;
    VkFormat OverlayStencilFormat                        = VK_FORMAT_S8_UINT;

    const std::vector<const char*> DEVICE_EXTENSIONS = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_MAINTENANCE1_EXTENSION_NAME
    };
    const std::vector<const char*> ValidationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

    bool SwapChainMSAA = false;
    UINT SwapChainMSAASamples = 1;

public:

    static VulkanCore& Instance();

    void InitVulkan(Window* window, RHIStructures::CoreInitData data);
    void Cleanup();
    // False when the swapchain had to be recreated and this frame is skipped.
    bool BeginFrame();
    void EndFrame();
    void WaitForGpu();
    void ResetWindow();
    bool IsSwapchainOutOfDate() const { return IsSwapchainRecreateNeeded; }

    // Getters
    Window* GetWindow() const { return RenderWindow; }
    uint64_t GetFrameNumber() const { return FrameNumber; }
    VkFormat GetOverlayStencilFormat() const { return OverlayStencilFormat; }
    VkDevice GetDevice() const { return Device; }
    VkPhysicalDevice GetPhysicalDevice() const { return PhysicalDevice; }
    VkSurfaceKHR GetSurface() const { return Surface; }
    VkInstance GetInstance() const { return VulkanInstance; }
    VkQueue GetGraphicsQueue() const { return GraphicsQueue; }
    VkCommandPool GetCommandPool() const { return CommandPool; }
    VkExtent2D GetExtent() const { return Extent2D; }
    VkFormat GetSwapchainFormat() const { return SwapchainFormat; }
    const std::vector<VulkanImageData>& GetSwapchainImages() const { return SwapchainImages; }
    VkCommandBuffer GetCommandBuffer() const { return CommandBuffers[CurrentFrameIndex]; }
    const std::vector<VkSemaphore>& GetImageAvailableSemaphores() const { return ImageAvailableSemaphores; }
    const std::vector<VkSemaphore>& GetRenderFinishedSemaphores() const { return RenderFinishedSemaphores; }
    const std::vector<VkFence>& GetInFlightFences() const { return InFlightFences; }
    uint32_t GetFramesInFlight() const { return SwapChainImageCount; }
    uint32_t GetCurrentFrameIndex() const { return CurrentFrameIndex; }
    uint32_t GetCurrentSwapchainImageIndex() const { return CurrentSwapchainImageIndex; }
    VkImageView GetCurrentSwapchainImageView() const { return SwapchainImages[CurrentSwapchainImageIndex].ImageView; }
    VkImage GetCurrentSwapchainImage() const {return SwapchainImages[CurrentSwapchainImageIndex].ImageHandle; }
    uint32_t GetQueueFamilyIndex() { return FindQueueFamilies(PhysicalDevice).GraphicsFamily; }
    VkRenderPass GetOverlayRenderPass() { return OverlayRenderPass; }
    VkFramebuffer GetCurrentOverlayFramebuffer() { return OverlayFramebuffers[CurrentSwapchainImageIndex]; }
    VkDescriptorSet GetGlobalSamplerSet() const { return GlobalSamplerSet; }
    VkDescriptorSetLayout GetGlobalSamplerSetLayout() const { return GlobalSamplerSetLayout; }

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
    void CreateSamplers();
    void CreateOverlayRenderPass();
    void CreateOverlayStencilImages();
    void CreateOverlayFramebuffers();


    uint32_t FindMemoryType(uint32_t allowedTypes, VkMemoryPropertyFlags properties);

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

public:

    QueueFamilyIndicesData FindQueueFamilies(VkPhysicalDevice device);

private:
    //Cleanup
    void DestroySwapchainViews();
    void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) ;

};
