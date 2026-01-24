#include "RHIStructures.h"

VkFormat RHIStructures::VulkanFormat(Format format)
{
    switch (format)
    {
        case Format::Unknown: return VK_FORMAT_UNDEFINED;
        case Format::R8G8B8A8_UNORM: return VK_FORMAT_R8G8B8A8_UNORM;
        case Format::R8G8B8A8_UNORM_SRGB: return VK_FORMAT_R8G8B8A8_SRGB;
        case Format::R16G16B16A16_FLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case Format::R32G32B32_FLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case Format::R32G32B32A32_FLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case Format::D16_UNORM: return VK_FORMAT_D16_UNORM;
        case Format::D24_UNORM_S8_UINT: return VK_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32_FLOAT: return VK_FORMAT_D32_SFLOAT;
        case Format::D32_FLOAT_S8X24_UINT: return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case Format::BC1_UNORM: return VK_FORMAT_BC1_RGB_UNORM_BLOCK;
        case Format::BC2_UNORM: return VK_FORMAT_BC2_UNORM_BLOCK;
        case Format::BC3_UNORM: return VK_FORMAT_BC3_UNORM_BLOCK;
        case Format::BC4_UNORM: return VK_FORMAT_BC4_UNORM_BLOCK;
        case Format::BC5_UNORM: return VK_FORMAT_BC5_UNORM_BLOCK;
        case Format::BC6H_UF16: return VK_FORMAT_BC6H_UFLOAT_BLOCK;
        case Format::BC7_UNORM: return VK_FORMAT_BC7_UNORM_BLOCK;
        default: return VK_FORMAT_UNDEFINED;
    }
}

DXGI_FORMAT RHIStructures::DXGIFormat(Format format)
{
    switch (format)
    {
        case Format::Unknown: return DXGI_FORMAT_UNKNOWN;
        case Format::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case Format::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case Format::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case Format::R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
        case Format::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case Format::D16_UNORM: return DXGI_FORMAT_D16_UNORM;
        case Format::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case Format::D32_FLOAT: return DXGI_FORMAT_D32_FLOAT;
        case Format::D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
        case Format::BC1_UNORM: return DXGI_FORMAT_BC1_UNORM;
        case Format::BC2_UNORM: return DXGI_FORMAT_BC2_UNORM;
        case Format::BC3_UNORM: return DXGI_FORMAT_BC3_UNORM;
        case Format::BC4_UNORM: return DXGI_FORMAT_BC4_UNORM;
        case Format::BC5_UNORM: return DXGI_FORMAT_BC5_UNORM;
        case Format::BC6H_UF16: return DXGI_FORMAT_BC6H_UF16;
        case Format::BC7_UNORM: return DXGI_FORMAT_BC7_UNORM;
        default: return DXGI_FORMAT_UNKNOWN;
    }
}
