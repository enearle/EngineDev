#include "VulkanCore.h"
#include "VulkanResource.h"
#include "../Window.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../MetaData.h"

using namespace Win32ErrorHandler;

VulkanCore& VulkanCore::GetInstance()
{
    static VulkanCore instance;
    return instance;
}

void VulkanCore::InitVulkan(Window* window)
{
    try
    {
        if (!window) throw std::runtime_error("Window pointer is null.");
        RenderWindow = window;

        CreateInstance();
        EnableDebugMessenger();
        CreateSurface();
        GetPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapchain();
    }
    catch (const std::runtime_error& error)
    {
        ErrorMessage(error.what());
    }
}

void VulkanCore::Cleanup()
{
    vkDestroySwapchainKHR(Device, Swapchain, nullptr);
    DestroySwapchainViews();
    vkDestroySurfaceKHR(VulkanInstance, Surface, nullptr);
    vkDestroyDevice(Device, nullptr);
    DestroyDebugUtilsMessengerEXT(VulkanInstance, DebugMessenger, nullptr);
    vkDestroyInstance(VulkanInstance, nullptr);

    Initialized = false;
}

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
    
    // Creat device features
    VkPhysicalDeviceFeatures deviceFeatures = {};
    deviceFeatures.samplerAnisotropy = VK_TRUE;    // Enable anisotropy feature
    deviceFeatures.geometryShader = VK_TRUE;        // Enable geometry shader feature
    deviceFeatures.depthClamp = VK_TRUE;             // Enable depth clamp feature

    // Info used to create the device (logical) including required queues, features, and device extensions
    VkDeviceCreateInfo deviceInfo = {};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    deviceInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    VkResult result = vkCreateDevice(PhysicalDevice, &deviceInfo, nullptr, &Device);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device.");

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


    // If maxImageCount is 0 then there is no limit
    // If not, we clamp the image count under maxImageCount
    uint32_t imageCount = details.Capabilities.minImageCount + 1;
    if (details.Capabilities.maxImageCount > 0)
        imageCount = std::min(imageCount, details.Capabilities.maxImageCount);
    
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
        SwapchainImageData newSwapchainImage = {};
        newSwapchainImage.ImageHandle = swapchainImages[i];

        // TODO: Setup resource allocation system
        newSwapchainImage.ImageView = VulkanResource::CreateImageView(Device, swapchainImages[i],
            SwapchainFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        
        SwapchainImages.push_back(newSwapchainImage);
    }

    MaxFramesInFlight = SwapchainImages.size();
}

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
        if((availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM || availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB)
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

void VulkanCore::GetPhysicalDevice()
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
    
    // FOR DYNAMIC UNIFORMS
    //MinUniformAlignment = properties.limits.minUniformBufferOffsetAlignment;
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