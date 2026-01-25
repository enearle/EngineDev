#include "RHIStructures.h"
#include <d3d12.h>

namespace RHIStructures
{
    //====================================//
    // ----------- Pipeline ------------- //
    //====================================//
    
    // Format Mappings
    constexpr std::array<VkFormat, 17> VULKAN_FORMATS = {
        VK_FORMAT_UNDEFINED,                // Unknown = 0
        VK_FORMAT_R8G8B8A8_UNORM,           // R8G8B8A8_UNORM = 1
        VK_FORMAT_R8G8B8A8_SRGB,            // R8G8B8A8_UNORM_SRGB = 2
        VK_FORMAT_R16G16B16A16_SFLOAT,      // R16G16B16A16_FLOAT = 3
        VK_FORMAT_R32G32B32_SFLOAT,         // R32G32B32_FLOAT = 4
        VK_FORMAT_R32G32B32A32_SFLOAT,      // R32G32B32A32_FLOAT = 5
        VK_FORMAT_D16_UNORM,                // D16_UNORM = 6
        VK_FORMAT_D24_UNORM_S8_UINT,        // D24_UNORM_S8_UINT = 7
        VK_FORMAT_D32_SFLOAT,               // D32_FLOAT = 8
        VK_FORMAT_D32_SFLOAT_S8_UINT,       // D32_FLOAT_S8X24_UINT = 9
        VK_FORMAT_BC1_RGB_UNORM_BLOCK,      // BC1_UNORM = 10
        VK_FORMAT_BC2_UNORM_BLOCK,          // BC2_UNORM = 11
        VK_FORMAT_BC3_UNORM_BLOCK,          // BC3_UNORM = 12
        VK_FORMAT_BC4_UNORM_BLOCK,          // BC4_UNORM = 13
        VK_FORMAT_BC5_UNORM_BLOCK,          // BC5_UNORM = 14
        VK_FORMAT_BC6H_UFLOAT_BLOCK,        // BC6H_UF16 = 15
        VK_FORMAT_BC7_UNORM_BLOCK           // BC7_UNORM = 16
    };

    constexpr std::array<DXGI_FORMAT, 17> DX_FORMATS = {
        DXGI_FORMAT_UNKNOWN,                // Unknown = 0
        DXGI_FORMAT_R8G8B8A8_UNORM,         // R8G8B8A8_UNORM = 1
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,    // R8G8B8A8_UNORM_SRGB = 2
        DXGI_FORMAT_R16G16B16A16_FLOAT,     // R16G16B16A16_FLOAT = 3
        DXGI_FORMAT_R32G32B32_FLOAT,        // R32G32B32_FLOAT = 4
        DXGI_FORMAT_R32G32B32A32_FLOAT,     // R32G32B32A32_FLOAT = 5
        DXGI_FORMAT_D16_UNORM,              // D16_UNORM = 6
        DXGI_FORMAT_D24_UNORM_S8_UINT,      // D24_UNORM_S8_UINT = 7
        DXGI_FORMAT_D32_FLOAT,              // D32_FLOAT = 8
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT,   // D32_FLOAT_S8X24_UINT = 9
        DXGI_FORMAT_BC1_UNORM,              // BC1_UNORM = 10
        DXGI_FORMAT_BC2_UNORM,              // BC2_UNORM = 11
        DXGI_FORMAT_BC3_UNORM,              // BC3_UNORM = 12
        DXGI_FORMAT_BC4_UNORM,              // BC4_UNORM = 13
        DXGI_FORMAT_BC5_UNORM,              // BC5_UNORM = 14
        DXGI_FORMAT_BC6H_UF16,              // BC6H_UF16 = 15
        DXGI_FORMAT_BC7_UNORM               // BC7_UNORM = 16
    };

    VkFormat VulkanFormat(Format format)
    {
        const auto index = static_cast<uint8_t>(format);
        if (index < VULKAN_FORMATS.size())
            return VULKAN_FORMATS[index];
        return VK_FORMAT_UNDEFINED;
    }

    DXGI_FORMAT DXFormat(Format format)
    {
        const auto index = static_cast<uint8_t>(format);
        if (index < DX_FORMATS.size())
            return DX_FORMATS[index];
        return DXGI_FORMAT_UNKNOWN;
    }

    // PrimitiveTopology Mappings
    constexpr std::array<VkPrimitiveTopology, 6> VULKAN_PRIMITIVE_TOPOLOGIES = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,   // TriangleList = 0
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,  // TriangleStrip = 1
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,    // TriangleFan = 2
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,       // LineList = 3
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,      // LineStrip = 4
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST       // PointList = 5
    };

    constexpr std::array<D3D12_PRIMITIVE_TOPOLOGY, 6> DX_PRIMITIVE_TOPOLOGIES = {
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,   // TriangleList = 0
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,  // TriangleStrip = 1
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN,    // TriangleFan = 2
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,       // LineList = 3
        D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,      // LineStrip = 4
        D3D_PRIMITIVE_TOPOLOGY_POINTLIST       // PointList = 5
    };

    VkPrimitiveTopology VulkanPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        const auto index = static_cast<uint8_t>(primitiveTopology);
        if (index < VULKAN_PRIMITIVE_TOPOLOGIES.size())
            return VULKAN_PRIMITIVE_TOPOLOGIES[index];
        return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    }

    D3D12_PRIMITIVE_TOPOLOGY DXPrimitiveTopology(PrimitiveTopology primitiveTopology)
    {
        const auto index = static_cast<uint8_t>(primitiveTopology);
        if (index < DX_PRIMITIVE_TOPOLOGIES.size())
            return DX_PRIMITIVE_TOPOLOGIES[index];
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }

    // FillMode Mappings
    constexpr std::array<VkPolygonMode, 2> VULKAN_FILL_MODES = {
        VK_POLYGON_MODE_FILL,   // Solid = 0
        VK_POLYGON_MODE_LINE    // Wireframe = 1
    };

    constexpr std::array<D3D12_FILL_MODE, 2> DX_FILL_MODES = {
        D3D12_FILL_MODE_SOLID,      // Solid = 0
        D3D12_FILL_MODE_WIREFRAME   // Wireframe = 1
    };

    VkPolygonMode VulkanFillMode(FillMode fillMode)
    {
        const auto index = static_cast<uint8_t>(fillMode);
        if (index < VULKAN_FILL_MODES.size())
            return VULKAN_FILL_MODES[index];
        return VK_POLYGON_MODE_FILL;
    }

    D3D12_FILL_MODE DXFillMode(FillMode fillMode)
    {
        const auto index = static_cast<uint8_t>(fillMode);
        if (index < DX_FILL_MODES.size())
            return DX_FILL_MODES[index];
        return D3D12_FILL_MODE_SOLID;
    }

    // CullMode Mappings
    constexpr std::array<VkCullModeFlags, 3> VULKAN_CULL_MODES = {
        VK_CULL_MODE_NONE,      // None = 0
        VK_CULL_MODE_BACK_BIT,  // Back = 1
        VK_CULL_MODE_FRONT_BIT  // Front = 2
    };

    constexpr std::array<D3D12_CULL_MODE, 3> DX_CULL_MODES = {
        D3D12_CULL_MODE_NONE,   // None = 0
        D3D12_CULL_MODE_BACK,   // Back = 1
        D3D12_CULL_MODE_FRONT   // Front = 2
    };

    VkCullModeFlags VulkanCullMode(CullMode cullMode)
    {
        const auto index = static_cast<uint8_t>(cullMode);
        if (index < VULKAN_CULL_MODES.size())
            return VULKAN_CULL_MODES[index];
        return VK_CULL_MODE_NONE;
    }

    D3D12_CULL_MODE DXCullMode(CullMode cullMode)
    {
        const auto index = static_cast<uint8_t>(cullMode);
        if (index < DX_CULL_MODES.size())
            return DX_CULL_MODES[index];
        return D3D12_CULL_MODE_NONE;
    }

    // CompareOp Mappings
    constexpr std::array<VkCompareOp, 8> VULKAN_COMPARE_OPS = {
        VK_COMPARE_OP_NEVER,             // Never = 0
        VK_COMPARE_OP_LESS,              // Less = 1
        VK_COMPARE_OP_EQUAL,             // Equal = 2
        VK_COMPARE_OP_LESS_OR_EQUAL,     // LessEqual = 3
        VK_COMPARE_OP_GREATER,           // Greater = 4
        VK_COMPARE_OP_NOT_EQUAL,         // NotEqual = 5
        VK_COMPARE_OP_GREATER_OR_EQUAL,  // GreaterEqual = 6
        VK_COMPARE_OP_ALWAYS             // Always = 7
    };

    constexpr std::array<D3D12_COMPARISON_FUNC, 8> DX_COMPARE_OPS = {
        D3D12_COMPARISON_FUNC_NEVER,             // Never = 0
        D3D12_COMPARISON_FUNC_LESS,              // Less = 1
        D3D12_COMPARISON_FUNC_EQUAL,             // Equal = 2
        D3D12_COMPARISON_FUNC_LESS_EQUAL,        // LessEqual = 3
        D3D12_COMPARISON_FUNC_GREATER,           // Greater = 4
        D3D12_COMPARISON_FUNC_NOT_EQUAL,         // NotEqual = 5
        D3D12_COMPARISON_FUNC_GREATER_EQUAL,     // GreaterEqual = 6
        D3D12_COMPARISON_FUNC_ALWAYS             // Always = 7
    };

    VkCompareOp VulkanCompareOp(CompareOp compareOp)
    {
        const auto index = static_cast<uint8_t>(compareOp);
        if (index < VULKAN_COMPARE_OPS.size())
            return VULKAN_COMPARE_OPS[index];
        return VK_COMPARE_OP_NEVER;
    }

    D3D12_COMPARISON_FUNC DXCompareOp(CompareOp compareOp)
    {
        const auto index = static_cast<uint8_t>(compareOp);
        if (index < DX_COMPARE_OPS.size())
            return DX_COMPARE_OPS[index];
        return D3D12_COMPARISON_FUNC_NEVER;
    }

    // BlendOp Mappings
    constexpr std::array<VkBlendOp, 5> VULKAN_BLEND_OPS = {
        VK_BLEND_OP_ADD,                 // Add = 0
        VK_BLEND_OP_SUBTRACT,            // Subtract = 1
        VK_BLEND_OP_REVERSE_SUBTRACT,    // ReverseSubtract = 2
        VK_BLEND_OP_MIN,                 // Min = 3
        VK_BLEND_OP_MAX                  // Max = 4
    };

    constexpr std::array<D3D12_BLEND_OP, 5> DX_BLEND_OPS = {
        D3D12_BLEND_OP_ADD,              // Add = 0
        D3D12_BLEND_OP_SUBTRACT,         // Subtract = 1
        D3D12_BLEND_OP_REV_SUBTRACT,     // ReverseSubtract = 2
        D3D12_BLEND_OP_MIN,              // Min = 3
        D3D12_BLEND_OP_MAX               // Max = 4
    };

    VkBlendOp VulkanBlendOp(BlendOp blendOp)
    {
        const auto index = static_cast<uint8_t>(blendOp);
        if (index < VULKAN_BLEND_OPS.size())
            return VULKAN_BLEND_OPS[index];
        return VK_BLEND_OP_ADD;
    }

    D3D12_BLEND_OP DXBlendOp(BlendOp blendOp)
    {
        const auto index = static_cast<uint8_t>(blendOp);
        if (index < DX_BLEND_OPS.size())
            return DX_BLEND_OPS[index];
        return D3D12_BLEND_OP_ADD;
    }

    // BlendFactor Mappings
    constexpr std::array<VkBlendFactor, 12> VULKAN_BLEND_FACTORS = {
        VK_BLEND_FACTOR_ZERO,                   // Zero = 0
        VK_BLEND_FACTOR_ONE,                    // One = 1
        VK_BLEND_FACTOR_SRC_COLOR,              // SrcColor = 2
        VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,    // InvSrcColor = 3
        VK_BLEND_FACTOR_SRC_ALPHA,              // SrcAlpha = 4
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,    // InvSrcAlpha = 5
        VK_BLEND_FACTOR_DST_ALPHA,              // DestAlpha = 6
        VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,    // InvDestAlpha = 7
        VK_BLEND_FACTOR_DST_COLOR,              // DestColor = 8
        VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,    // InvDestColor = 9
        VK_BLEND_FACTOR_CONSTANT_COLOR,         // ConstColor = 10
        VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR // InvConstColor = 11
    };

    constexpr std::array<D3D12_BLEND, 12> DX_BLEND_FACTORS = {
        D3D12_BLEND_ZERO,                       // Zero = 0
        D3D12_BLEND_ONE,                        // One = 1
        D3D12_BLEND_SRC_COLOR,                  // SrcColor = 2
        D3D12_BLEND_INV_SRC_COLOR,              // InvSrcColor = 3
        D3D12_BLEND_SRC_ALPHA,                  // SrcAlpha = 4
        D3D12_BLEND_INV_SRC_ALPHA,              // InvSrcAlpha = 5
        D3D12_BLEND_DEST_ALPHA,                 // DestAlpha = 6
        D3D12_BLEND_INV_DEST_ALPHA,             // InvDestAlpha = 7
        D3D12_BLEND_DEST_COLOR,                 // DestColor = 8
        D3D12_BLEND_INV_DEST_COLOR,             // InvDestColor = 9
        D3D12_BLEND_BLEND_FACTOR,               // ConstColor = 10
        D3D12_BLEND_INV_BLEND_FACTOR            // InvConstColor = 11
    };

    VkBlendFactor VulkanBlendFactor(BlendFactor blendFactor)
    {
        const auto index = static_cast<uint8_t>(blendFactor);
        if (index < VULKAN_BLEND_FACTORS.size())
            return VULKAN_BLEND_FACTORS[index];
        return VK_BLEND_FACTOR_ZERO;
    }

    D3D12_BLEND DXBlendFactor(BlendFactor blendFactor)
    {
        const auto index = static_cast<uint8_t>(blendFactor);
        if (index < DX_BLEND_FACTORS.size())
            return DX_BLEND_FACTORS[index];
        return D3D12_BLEND_ZERO;
    }
}





