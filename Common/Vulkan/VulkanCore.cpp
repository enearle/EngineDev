#include "VulkanCore.h"
#include "VulkanResource.h"
#include "../Window.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../MetaData.h"
#include "../RHI/Renderer.h"

using namespace Win32ErrorHandler;

VulkanCore& VulkanCore::GetInstance()
{
    static VulkanCore instance;
    return instance;
}

void VulkanCore::InitVulkan(Window* window, CoreInitData data)
{
    try
    {
        if (!window) throw std::runtime_error("Window pointer is null.");
        RenderWindow = window;
        if (Initialized)throw std::runtime_error("Vulkan already initialized.");
        Initialized = true;
        SwapChainMSAA = data.SwapchainMSAA;
        SwapChainMSAASamples = data.SwapchainMSAASamples;
        CreateInstance();
        EnableDebugMessenger();
        CreateSurface();
        SelectPhysicalDevice();
        CreateLogicalDevice();
        CreateCommandPool();
        CreateSwapchain();
        CreateSynchronizationPrimitives();
        CreateSamplers();
    }
    catch (const std::runtime_error& error)
    {
        ErrorMessage(error.what());
    }
}

void VulkanCore::Cleanup()
{
    vkQueueWaitIdle(GraphicsQueue);
    vkQueueWaitIdle(PresentQueue);
    
    vkDestroySampler(Device, PointSampler, nullptr);
    vkDestroySampler(Device, LinearSampler, nullptr);
    for (uint32_t i = 0; i < SwapChainImageCount; i++)
    {
        vkDestroySemaphore(Device, ImageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(Device, RenderFinishedSemaphores[i], nullptr);
        vkDestroyFence(Device, InFlightFences[i], nullptr);
    }
    vkDestroyFence(Device, TransferFence, nullptr);
    vkFreeCommandBuffers(Device, CommandPool, static_cast<uint32_t>(CommandBuffers.size()), CommandBuffers.data());
    vkFreeCommandBuffers(Device, CommandPool, 1, &TransferCommandBuffer);
    vkDestroyCommandPool(Device, CommandPool, nullptr);
    vkDestroySwapchainKHR(Device, Swapchain, nullptr);
    DestroySwapchainViews();
    vkDestroySurfaceKHR(VulkanInstance, Surface, nullptr);
    vkDestroyDevice(Device, nullptr);
    DestroyDebugUtilsMessengerEXT(VulkanInstance, DebugMessenger, nullptr);
    vkDestroyInstance(VulkanInstance, nullptr);

    Initialized = false;
}

//======================================================//
// Init Functions                                       //
//======================================================//

void VulkanCore::CreateInstance()
{
    // Application meta data
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = MetaData::GetAppName();
    appInfo.applicationVersion = VK_MAKE_VERSION(MetaData::GetAppMajor(), MetaData::GetAppMinor(), MetaData::GetAppPatch());
    appInfo.pEngineName = MetaData::GetEngineName();
    appInfo.engineVersion = VK_MAKE_VERSION(MetaData::GetEngineMajor(), MetaData::GetEngineMinor(), MetaData::GetEnginePatch());
    appInfo.apiVersion = VK_API_VERSION_1_3;
    
    // Extensions
    std::vector<const char*> extensions = {};

    // For glfw window support
    // TODO: Add back later
    
    //uint32_t glfwExtensionCount = 0;
    //const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    //extensions.reserve(glfwExtensionCount);
    //
    //for (size_t i= 0; i < glfwExtensionCount; i++)
    //    extensions.push_back(glfwExtensions[i]);
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
    
#if defined(DEBUG) || defined(_DEBUG) 
    if (!CheckValidationLayerSupport())
        throw std::runtime_error("Validation layers requested, but not available.");
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    
    uint32_t erroneousIndex;
    if (!CheckInstanceExtensionSupport(extensions, erroneousIndex))
        throw std::runtime_error("Failed to find supported extension: " + std::string(extensions[erroneousIndex]));   

    // Instance
    VkInstanceCreateInfo instInfo = {};
    instInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instInfo.pApplicationInfo = &appInfo;
#if defined(DEBUG) || defined(_DEBUG) 
        instInfo.enabledLayerCount = static_cast<uint32_t>(ValidationLayers.size());
        instInfo.ppEnabledLayerNames = ValidationLayers.data();
#else
        instInfo.enabledLayerCount = 0;
        instInfo.ppEnabledLayerNames = nullptr;
#endif
    instInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instInfo.ppEnabledExtensionNames = extensions.data();

    VkResult result = vkCreateInstance(&instInfo, nullptr, &VulkanInstance);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan instance.");
}

void VulkanCore::CreateLogicalDevice()
{
    // Get queue family indices for the chosen device
    QueueFamilyIndicesData indices = FindQueueFamilies(PhysicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<int> queueFamilies = {indices.GraphicsFamily, indices.PresentFamily};
    for (int queueFamily : queueFamilies)
    {
        // Create queues
        VkDeviceQueueCreateInfo queueInfo = {};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamily;
        queueInfo.queueCount = 1;
        // Vulkan uses a priority evaluation to determine how to handle multiple queues. (1 == highest priority);
        float queuePriority = 1.0f;
        queueInfo.pQueuePriorities = &queuePriority;
        
        queueCreateInfos.push_back(queueInfo);
    }
    VkPhysicalDeviceDescriptorBufferFeaturesEXT descriptorBufferFeatures = {};
    descriptorBufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_FEATURES_EXT;
    descriptorBufferFeatures.descriptorBuffer = VK_TRUE;
    
    VkPhysicalDeviceVulkan12Features deviceFeatures12 = {};
    deviceFeatures12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    deviceFeatures12.bufferDeviceAddress = VK_TRUE;
    deviceFeatures12.pNext = &descriptorBufferFeatures;
    
    // Creat device features
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;     // Enable anisotropy feature
    deviceFeatures.geometryShader = VK_TRUE;        // Enable geometry shader feature
    deviceFeatures.depthClamp = VK_TRUE;            // Enable depth clamp feature

    // Enable dynamic rendering feature
    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeature{};
    dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeature.dynamicRendering = VK_TRUE;
    dynamicRenderingFeature.pNext = &deviceFeatures12;

    // Info used to create the device (logical) including required queues, features, and device extensions
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    deviceInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    deviceInfo.pEnabledFeatures = &deviceFeatures;
    deviceInfo.pNext = &dynamicRenderingFeature;

    VkResult result = vkCreateDevice(PhysicalDevice, &deviceInfo, nullptr, &Device);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device.");
    
    // Assign extension functions.
    vkCmdBindDescriptorBuffersEXT_FnPtr =
    reinterpret_cast<PFN_vkCmdBindDescriptorBuffersEXT>(
        vkGetDeviceProcAddr(Device, "vkCmdBindDescriptorBuffersEXT"));

    vkCmdSetDescriptorBufferOffsetsEXT_FnPtr =
        reinterpret_cast<PFN_vkCmdSetDescriptorBufferOffsetsEXT>(
            vkGetDeviceProcAddr(Device, "vkCmdSetDescriptorBufferOffsetsEXT"));

    vkGetDescriptorEXT_FnPtr =
        reinterpret_cast<PFN_vkGetDescriptorEXT>(
            vkGetDeviceProcAddr(Device, "vkGetDescriptorEXT"));

    if (!vkCmdBindDescriptorBuffersEXT_FnPtr || !vkCmdSetDescriptorBufferOffsetsEXT_FnPtr || !vkGetDescriptorEXT_FnPtr)
        throw std::runtime_error("VK_EXT_descriptor_buffer functions not available (extension not enabled or unsupported).");

    // Assign queue handles
    vkGetDeviceQueue(Device, indices.GraphicsFamily, 0, &GraphicsQueue);
    vkGetDeviceQueue(Device, indices.PresentFamily, 0, &PresentQueue);
}

void VulkanCore::EnableDebugMessenger()
{
    // Configure debug messenger
#if defined(DEBUG) || defined(_DEBUG)
    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = DebugCallback;
    createInfo.pUserData = nullptr;

    if (CreateDebugUtilsMessengerEXT(VulkanInstance, &createInfo, nullptr, &DebugMessenger) != VK_SUCCESS)
        throw std::runtime_error("Failed to set up debug messenger.");
#endif
}

VkResult VulkanCore::CreateDebugUtilsMessengerEXT(VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else
        return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanCore::CreateSurface()
{
    // Creates platform-agnostic surface for Vulkan to draw to
    // GLFW version TODO: Addback later
    // VkResult result = glfwCreateWindowSurface(VulkanInstance, Window, nullptr, &Surface);

    VkWin32SurfaceCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    createInfo.hwnd = RenderWindow->GetWindowHandle();
    createInfo.hinstance = RenderWindow->GetInstance();

    VkResult result = vkCreateWin32SurfaceKHR(VulkanInstance, &createInfo, nullptr, &Surface);


    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create window surface.");
    
}

void VulkanCore::CreateSwapchain()
{
    SwapchainDetailsData details = GetSwapchainDetails();

    // Choose optimal swapchain settings from available
    VkSurfaceFormatKHR surfaceFormat = SelectSwapchainSurfaceFormat(details.Formats);
    VkPresentModeKHR presentMode = SelectPresentMode(details.PresentModes);
    VkExtent2D extent2D = SelectExtent(details.Capabilities);
    
    // Swapchain imageCount = 3 Clamped to min and max supported
    uint32_t imageCount = SwapChainImageCount;
    if (details.Capabilities.maxImageCount > 0)
        imageCount = std::min(imageCount, details.Capabilities.maxImageCount);
    imageCount = std::max(imageCount, details.Capabilities.minImageCount);
    
    VkSwapchainCreateInfoKHR swapchainInfo = {};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.imageFormat = surfaceFormat.format;                   // how colour data is packed 
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;           // How range of colour is displayed
    swapchainInfo.presentMode = presentMode;                            // How data is sent to the monitor
    swapchainInfo.imageExtent = extent2D;                               // Window dimensions
    swapchainInfo.minImageCount = imageCount;                           // Min number of images in the swap chain
    swapchainInfo.imageArrayLayers = 1;                                 // Number of layers for each image chain
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;     // What attachment images will be used as
    swapchainInfo.preTransform = details.Capabilities.currentTransform; // Image transform
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;   // How to handle blending images with multiple layers
    swapchainInfo.clipped = VK_TRUE;                                    // Allow image to be clipped if section of window is not visible
    swapchainInfo.surface = Surface;

    // If the queue family index is different for graphics and the presentation, then we share them
    QueueFamilyIndicesData indices = FindQueueFamilies(PhysicalDevice);
    if(indices.GraphicsFamily != indices.PresentFamily)
    {
        uint32_t queueFamilyIndices[] = {
            static_cast<uint32_t>(indices.GraphicsFamily),
            static_cast<uint32_t>(indices.PresentFamily)
        };
        
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;// Tell Vulkan we need to share between multiple queues
        swapchainInfo.queueFamilyIndexCount = 2;                    // Number of queues to share images between
        swapchainInfo.pQueueFamilyIndices = queueFamilyIndices;     // Array of queues to share between
    }
    else
    {
        // Else we use a single queue family
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainInfo.queueFamilyIndexCount = 0;
        swapchainInfo.pQueueFamilyIndices = nullptr;
    }

    // Check if old swapchain exists
    bool newSwapchain = false;
    if(Swapchain != VK_NULL_HANDLE)
    {
        swapchainInfo.oldSwapchain = Swapchain;
        newSwapchain = true;
    }
    else
        swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    // Create new swapchain and destroy old one if necessary
    VkResult result = vkCreateSwapchainKHR(Device, &swapchainInfo, nullptr, &Swapchain);
    
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create a swapchain.");
    
    if (newSwapchain)
        vkDestroySwapchainKHR(Device, swapchainInfo.oldSwapchain, nullptr);

    SwapchainFormat = surfaceFormat.format;
    Extent2D = extent2D;

    // Swapchain image count
    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(Device, Swapchain, &swapchainImageCount, nullptr);

    // Get swapchain images
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    vkGetSwapchainImagesKHR(Device, Swapchain, &swapchainImageCount, swapchainImages.data());

    SwapchainImages.clear();
    SwapchainImages.reserve(swapchainImageCount);

    // Store swapchain images and views
    for (uint32_t i = 0; i < swapchainImageCount; i++)
    {
        VulkanImageData newSwapchainImage = {};
        newSwapchainImage.ImageHandle = swapchainImages[i];

        // TODO: Setup resource allocation system
        newSwapchainImage.ImageView = VulkanResource::CreateImageView(Device, swapchainImages[i],
            SwapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        
        SwapchainImages.push_back(newSwapchainImage);
    }

    SwapChainImageCount = SwapchainImages.size();
    
    VkCommandBuffer cmdBuffer = CommandBuffers[0];  // Use existing buffer
    
    vkResetCommandBuffer(cmdBuffer, 0);
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    // Transition all swapchain images to PRESENT_SRC_KHR
    for (auto& swapchainImage : SwapchainImages)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapchainImage.ImageHandle;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;

        vkCmdPipelineBarrier(
            cmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }
    
    vkEndCommandBuffer(cmdBuffer);
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmdBuffer;

    vkQueueSubmit(GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(GraphicsQueue);
}

void VulkanCore::CreateSamplers()
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // or FLT_MAX

    samplerInfo.anisotropyEnable = /* if supported */ VK_TRUE;
    samplerInfo.maxAnisotropy = /* clamp to limits */ 16.0f;

    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    
    VkResult result = vkCreateSampler(Device, &samplerInfo, nullptr, &LinearSampler);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler.");
    
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    
    
    result = vkCreateSampler(Device, &samplerInfo, nullptr, &PointSampler);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create texture sampler.");
}


void VulkanCore::CreateSynchronizationPrimitives()
{
    ImageAvailableSemaphores.resize(SwapChainImageCount);
    RenderFinishedSemaphores.resize(SwapChainImageCount);
    InFlightFences.resize(SwapChainImageCount);
    
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    for (uint32_t i = 0; i < SwapChainImageCount; i++)
    {
        if (vkCreateSemaphore(Device, &semaphoreInfo, nullptr, &ImageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(Device, &semaphoreInfo, nullptr, &RenderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(Device, &fenceInfo, nullptr, &InFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create synchronization primitives.");
        }
    }

    VkResult result = vkCreateFence(Device, &fenceInfo, nullptr, &TransferFence);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create transfer fence.");

}

void VulkanCore::CreateCommandPool()
{
    QueueFamilyIndicesData queueFamilyIndices = FindQueueFamilies(PhysicalDevice);
    
    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = queueFamilyIndices.GraphicsFamily;

    VkResult result = vkCreateCommandPool(Device, &commandPoolInfo, nullptr, &CommandPool);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool.");

    // Allocate command buffers
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = SwapChainImageCount;

    CommandBuffers.resize(SwapChainImageCount);
    result = vkAllocateCommandBuffers(Device, &allocInfo, CommandBuffers.data());
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers.");

    VkCommandBufferAllocateInfo transferAllocInfo{};
    transferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    transferAllocInfo.commandPool = CommandPool;
    transferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    transferAllocInfo.commandBufferCount = 1;

    result = vkAllocateCommandBuffers(Device, &transferAllocInfo, &TransferCommandBuffer);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate transfer command buffer.");

}

//======================================================//
// Support Functions                                    //
//======================================================//

SwapchainDetailsData VulkanCore::GetSwapchainDetails()
{
    SwapchainDetailsData swapChainSupport;

    // Gat surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &swapChainSupport.Capabilities);

    // Get formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        swapChainSupport.Formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevice, Surface, &formatCount, swapChainSupport.Formats.data());
    }

    // Get presentation modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        swapChainSupport.PresentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevice, Surface, &presentModeCount,
                                                  swapChainSupport.PresentModes.data());
    }
    
    return swapChainSupport;
}

VkSurfaceFormatKHR VulkanCore::SelectSwapchainSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    // TODO: Make both swapchains take explicit formats
    // If the only available format is VK_FORMAT_UNDEFINED then all formats are available
    // So we just return our preference
    VkSurfaceFormatKHR surfaceFormat;
    if(availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
    {
        surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
        surfaceFormat.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        return surfaceFormat;
    }

    // Otherwise we just search for our preferred format
    for (auto availableFormat : availableFormats)
    {
        if((availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM)
            && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return availableFormat;
    }

    // Or just return the first index
    return availableFormats[0];
}

VkPresentModeKHR VulkanCore::SelectPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    // Look for mailbox present mode
    for (auto availablePresentMode : availablePresentModes)
    {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            return availablePresentMode;
    }

    // FIFO should always be available in the Vulkan spec
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanCore::SelectExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    // If current extent is at numeric limit, then extent cna vary
    // Or else it's the size of the window
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }
    else
    {
        // Manually get window width and height
        int width, height;

        // TODO: Add glfw support back later.
        //glfwGetFramebufferSize(Window, &width, &height);

        width = RenderWindow->GetWidth();
        height = RenderWindow->GetHeight();

        VkExtent2D extent = {};
        extent.width = width;
        extent.height = height;

        // Clamp values to surface capabilities
        extent.width = std::min(extent.width, capabilities.maxImageExtent.width);
        extent.width = std::max(extent.width, capabilities.minImageExtent.width);
        extent.height = std::min(extent.height, capabilities.maxImageExtent.height);
        extent.height = std::max(extent.height, capabilities.minImageExtent.height);
        
        return extent;
    }
}

bool VulkanCore::CheckInstanceExtensionSupport(std::vector<const char*> extensionsToCheck, uint32_t& erroneousIndex)
{
    // Get count (needed to get extensions)
    uint32_t extensionCount;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    
    // Get extensions
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());
    
    // Check for support
    for (size_t i = 0; i < extensionsToCheck.size(); i++)
    {
        bool supported = false;
        for (const auto& availableExtension : availableExtensions)
        {
            if (strcmp(availableExtension.extensionName, extensionsToCheck[i]) == 0)
                supported = true;
        }

        if (!supported)
        {
            erroneousIndex = static_cast<uint32_t>(i);
            return false;
        }
    }

    return true;
}

bool VulkanCore::CheckDeviceExtensionSupport(VkPhysicalDevice device)
{
    // Get device extension count
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    
    // Get available extensions
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    for (auto deviceExtension : DEVICE_EXTENSIONS)
    {
        bool supported = false;
        for (const auto& availableExtension : availableExtensions)
        {
            if (strcmp(availableExtension.extensionName, deviceExtension) == 0)
            {
                supported = true;
                break;
            }
        }
        
        if (!supported)
            return false;
    }

    return true;
}

bool VulkanCore::CheckDeviceSuitability(VkPhysicalDevice device)
{
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    
    QueueFamilyIndicesData indices = FindQueueFamilies(device);

    SwapchainDetailsData swapChainSupport = GetSwapchainDetails(device);

    bool swapChainValid = swapChainSupport.IsValid();
    bool indicesValid = indices.IsValid();
    bool extensionsSupported = CheckDeviceExtensionSupport(device);
    bool isDGPU = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    
    // TODO: make this work on APU/Integrated Graphics
    return indicesValid && extensionsSupported && swapChainValid && isDGPU && deviceFeatures.samplerAnisotropy && deviceFeatures.geometryShader && deviceFeatures.depthClamp;
}

bool VulkanCore::CheckValidationLayerSupport()
{
    // Get number of available layers
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    // Get layers
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    // Verify that required layers are supported
    // Return when false immediately when a required layer isn't found in the supported layers
    for (const char* layerName : ValidationLayers) {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers) {
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }

        if (!layerFound) 
            return false;
    }

    return true;
}

void VulkanCore::SelectPhysicalDevice()
{
    // Get count of devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(VulkanInstance, &deviceCount, nullptr);

    if (deviceCount == 0)
        throw std::runtime_error("Failed to find GPU with Vulkan support.");

    // Get devices
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(VulkanInstance, &deviceCount, devices.data());

    // Fine device that is appropriate for rendering
    for (const auto& device : devices)
    {
        if (CheckDeviceSuitability(device))
        {
            PhysicalDevice = device;
            break;
        }
    }

    // Set minimum alignment
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(PhysicalDevice, &properties);
    
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

    DescriptorBufferProperties = {};
    DescriptorBufferProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_BUFFER_PROPERTIES_EXT;
    DescriptorBufferProperties.pNext = nullptr;

    props2.pNext = &DescriptorBufferProperties;
    vkGetPhysicalDeviceProperties2(PhysicalDevice, &props2);
}

QueueFamilyIndicesData VulkanCore::FindQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndicesData indices;

    // Get count of families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    
    // Get queue families
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    // Find a graphics family
    // Check each queue family for one that's the required queue type
    for (int i = 0; i < static_cast<long>(queueFamilies.size()); i++)
    {
        if (queueFamilies[i].queueCount > 0 && queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.GraphicsFamily = i;

        // Check if queue family supports presentation
        VkBool32 presentationSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, Surface, &presentationSupport);
        if (queueFamilies[i].queueCount > 0 && presentationSupport)
            indices.PresentFamily = i;
        
        
        // If all queue family indices are valid, break out of search.
        if (indices.IsValid()) break;  
    }

    return indices;
}

SwapchainDetailsData VulkanCore::GetSwapchainDetails(VkPhysicalDevice device)
{
    SwapchainDetailsData swapChainSupport;

    // Gat surface capabilities
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, Surface, &swapChainSupport.Capabilities);

    // Get formats
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, Surface, &formatCount, nullptr);
    if (formatCount != 0)
    {
        swapChainSupport.Formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, Surface, &formatCount, swapChainSupport.Formats.data());
    }

    // Get presentation modes
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, Surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        swapChainSupport.PresentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, Surface, &presentModeCount,
                                                  swapChainSupport.PresentModes.data());
    }
    
    return swapChainSupport;
}

VkBool32 VulkanCore::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                       VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                       void* pUserData)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
    
    return VK_FALSE;
}

void VulkanCore::DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                                   const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) 
        func(instance, debugMessenger, pAllocator);
}

void VulkanCore::DestroySwapchainViews()
{
    for (auto& swapchainImage : SwapchainImages)
        vkDestroyImageView(Device, swapchainImage.ImageView, nullptr);
}

//======================================================//
// Command Recording                                    //
//======================================================//

void VulkanCore::BeginFrame()
{
    WaitForFrame(CurrentFrameIndex);
    
    VkResult result = vkAcquireNextImageKHR(
        Device,
        Swapchain,
        UINT64_MAX,
        ImageAvailableSemaphores[CurrentFrameIndex],
        VK_NULL_HANDLE,
        &CurrentSwapchainImageIndex
    );

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // TODO: Recreate swapchain for window resize
        throw std::runtime_error("Swapchain out of date - recreation needed");
    }
    else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swapchain image");
    }
    
    result = vkResetCommandBuffer(CommandBuffers[CurrentFrameIndex], 0);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to reset command buffer");
    
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; 
    
    result = vkBeginCommandBuffer(CommandBuffers[CurrentFrameIndex], &beginInfo);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to begin recording command buffer");
}

void VulkanCore::EndFrame()
{
    VkResult result = vkEndCommandBuffer(CommandBuffers[CurrentFrameIndex]);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to end recording command buffer");
    
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &CommandBuffers[CurrentFrameIndex];
    
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &ImageAvailableSemaphores[CurrentFrameIndex];
    submitInfo.pWaitDstStageMask = waitStages;
    
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &RenderFinishedSemaphores[CurrentFrameIndex];
    
    result = vkQueueSubmit(
        GraphicsQueue,
        1,
        &submitInfo,
        InFlightFences[CurrentFrameIndex]
    );

    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to submit command buffer");

    // Present the rendered image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &RenderFinishedSemaphores[CurrentFrameIndex];
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &Swapchain;
    presentInfo.pImageIndices = &CurrentSwapchainImageIndex;
    presentInfo.pResults = nullptr;

    result = vkQueuePresentKHR(PresentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Swapchain out of date or suboptimal - recreation needed");
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swapchain image");
    }

    // Advance to next frame
    CurrentFrameIndex = (CurrentFrameIndex + 1) % SwapChainImageCount;
}

void VulkanCore::WaitForFrame(uint32_t frameIndex)
{
    VkResult result = vkWaitForFences(
        Device,
        1,
        &InFlightFences[frameIndex],
        VK_TRUE,
        UINT64_MAX
    );

    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to wait for frame fence");

    // Reset fence for next use
    vkResetFences(Device, 1, &InFlightFences[frameIndex]);
}

void VulkanCore::WaitForGPU()
{
    for (uint32_t i = 0; i < SwapChainImageCount; i++)
        WaitForFrame(i);
}
