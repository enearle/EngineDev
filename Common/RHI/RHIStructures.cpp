#include "RHIStructures.h"
#include <d3d12.h>
#include <fstream>
#include <string>
#include <filesystem>
#include "../Vulkan/VulkanStructs.h"
#include "../Vulkan/VulkanCore.h"
#include "../GraphicsSettings.h"
#include "../DirectX12/D3D12Structs.h"
#include "../Windows/Win32ErrorHandler.h"

using namespace Win32ErrorHandler;
using namespace VulkanStructs;

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

    VkImageAspectFlags VulkanAspects(Format format)
    {
        switch (format)
        {
        case Format::D16_UNORM:
        case Format::D32_FLOAT:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        case Format::D24_UNORM_S8_UINT:
        case Format::D32_FLOAT_S8X24_UINT:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case Format::R8G8B8A8_UNORM:
        case Format::R8G8B8A8_UNORM_SRGB:
        case Format::R16G16B16A16_FLOAT:
        case Format::R32G32B32_FLOAT:
        case Format::R32G32B32A32_FLOAT:
        case Format::BC1_UNORM:
        case Format::BC2_UNORM:
        case Format::BC3_UNORM:
        case Format::BC4_UNORM:
        case Format::BC5_UNORM:
        case Format::BC6H_UF16:
        case Format::BC7_UNORM:
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
        }
    }

    // Semantic names
    constexpr std::array<std::string_view, 9> SEMANTIC_NAMES = {
        "POSITION",
        "NORMAL",
        "TEXCOORD",
        "TANGENT",
        "BINORMAL",
        "COLOR",
        "BLENDWEIGHT",
        "BLENDINDICES",
        "PSIZE"
        
    };

    ShaderStage ImportShader(const std::string& filename, const char* entryPoint)
    {
        ShaderStage shaderStage;
        std::string path;
        if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        {
            path = "../../Common/DirectX12/Shaders/CSO/";
            path += filename;
            path += ".cso";
        }
        else if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        {
            path = "../../Common/Vulkan/Shaders/SPIRV/";
            path += filename;
            path += ".spv";
        }
        else
            throw std::runtime_error("Invalid API selected!");

        std::filesystem::path absolutePath = std::filesystem::absolute(path);
        
        std::ifstream shaderFile(absolutePath, std::ios::binary | std::ios::ate);
        if (!shaderFile.is_open())
        {
            std::string log = "Failed to open shader file.\n"
                "Expected at: " + absolutePath.string() + "\n"
                "Current working directory: " + std::filesystem::current_path().string();
            ErrorMessage(log.c_str());
            throw std::runtime_error(log);
        }
    
        std::streamsize fileSize = shaderFile.tellg();
        shaderFile.seekg(0, std::ios::beg);
    
        void* shaderBytecode = malloc(fileSize);
        if (!shaderFile.read(static_cast<char*>(shaderBytecode), fileSize))
        {
            free(shaderBytecode);
            throw std::runtime_error("Failed to read shader file: " + path);
        }
        shaderFile.close();

        shaderStage.ByteCode = shaderBytecode;
        shaderStage.ByteCodeSize = fileSize;
        shaderStage.EntryPoint = entryPoint;
    
        return shaderStage;
    }

    VkShaderModule VulkanShaderModule(ShaderStage shaderStage)
    {
        VkShaderModuleCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shaderStage.ByteCodeSize;
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderStage.ByteCode);
        VkShaderModule shaderModule;
        VkResult result;
        result = vkCreateShaderModule(VulkanCore::GetInstance().GetDevice(), &createInfo, nullptr, &shaderModule);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create shader module!");
        return shaderModule;
    }

    D3D12_SHADER_BYTECODE DXShaderBytecode(ShaderStage shaderStage)
    {
        D3D12_SHADER_BYTECODE bytecode = {};
        bytecode.pShaderBytecode = shaderStage.ByteCode;
        bytecode.BytecodeLength = shaderStage.ByteCodeSize;
        return bytecode;
    }

    const char* SemanticNameString(SemanticName semanticName)
    {
        const auto index = static_cast<uint8_t>(semanticName);
        if (index < SEMANTIC_NAMES.size())
            return SEMANTIC_NAMES[index].data();
        return nullptr;
    }

    // PrimitiveTopology Mappings
    constexpr std::array<VkPrimitiveTopology, 14> VULKAN_PRIMITIVE_TOPOLOGIES = {
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,   // TriangleList = 0
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,  // TriangleStrip = 1
        VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,    // TriangleFan = 2
        VK_PRIMITIVE_TOPOLOGY_LINE_LIST,       // LineList = 3
        VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,      // LineStrip = 4
        VK_PRIMITIVE_TOPOLOGY_POINT_LIST,      // PointList = 5
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // PatchList1 = 6
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // PatchList2 = 7
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // PatchList3 = 8
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // PatchList4 = 9
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // PatchList5 = 10
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // PatchList6 = 11
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // PatchList7 = 12
        VK_PRIMITIVE_TOPOLOGY_PATCH_LIST,      // PatchList8 = 13
    };

    constexpr std::array<D3D12_PRIMITIVE_TOPOLOGY, 14> DX_PRIMITIVE_TOPOLOGIES = {
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,                    // TriangleList = 0
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,                   // TriangleStrip = 1
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLEFAN,                     // TriangleFan = 2
        D3D_PRIMITIVE_TOPOLOGY_LINELIST,                        // LineList = 3
        D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,                       // LineStrip = 4
        D3D_PRIMITIVE_TOPOLOGY_POINTLIST,                       // PointList = 5
        D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST,       // PatchList1 = 6
        D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST,       // PatchList2 = 7
        D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST,       // PatchList3 = 8
        D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,       // PatchList4 = 9
        D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST,       // PatchList5 = 10
        D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST,       // PatchList6 = 11
        D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST,       // PatchList7 = 12
        D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST,       // PatchList8 = 13
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

    D3D12_PRIMITIVE_TOPOLOGY_TYPE DXPrimitiveTopologyType(PrimitiveTopology primitiveTopology)
    {
        const auto index = static_cast<uint8_t>(primitiveTopology);
        
        if (index >= static_cast<uint8_t>(PrimitiveTopology::PatchList1) && 
            index <= static_cast<uint8_t>(PrimitiveTopology::PatchList8))
        {
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
        }
        
        switch (primitiveTopology)
        {
        case PrimitiveTopology::TriangleList:
        case PrimitiveTopology::TriangleStrip:
        case PrimitiveTopology::TriangleFan:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        case PrimitiveTopology::LineList:
        case PrimitiveTopology::LineStrip:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
        case PrimitiveTopology::PointList:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
        default:
            return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        }
    }

    uint32_t GetPatchControlPoints(PrimitiveTopology primitiveTopology)
    {
        const auto index = static_cast<uint8_t>(primitiveTopology);
        if (index >= static_cast<uint8_t>(PrimitiveTopology::PatchList1) && 
            index <= static_cast<uint8_t>(PrimitiveTopology::PatchList8))
        {
            return index - static_cast<uint8_t>(PrimitiveTopology::PatchList1) + 1;
        }
        return 0;
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

    // StencilOP Mappings
    constexpr std::array<VkStencilOp, 8> VULKAN_STENCIL_OPS = {
        VK_STENCIL_OP_KEEP,
        VK_STENCIL_OP_ZERO,
        VK_STENCIL_OP_REPLACE,
        VK_STENCIL_OP_INCREMENT_AND_CLAMP,
        VK_STENCIL_OP_DECREMENT_AND_CLAMP,
        VK_STENCIL_OP_INVERT,
        VK_STENCIL_OP_INCREMENT_AND_WRAP,
        VK_STENCIL_OP_DECREMENT_AND_WRAP
    };

    constexpr std::array<D3D12_STENCIL_OP, 8> DX_STENCIL_OPS = {
        D3D12_STENCIL_OP_KEEP,
        D3D12_STENCIL_OP_ZERO,
        D3D12_STENCIL_OP_REPLACE,
        D3D12_STENCIL_OP_INCR_SAT,
        D3D12_STENCIL_OP_DECR_SAT,
        D3D12_STENCIL_OP_INVERT,
        D3D12_STENCIL_OP_INCR,
        D3D12_STENCIL_OP_DECR
    };

    VkStencilOp VulkanStencilOp(StencilOp stencilOp)
    {
        const auto index = static_cast<uint8_t>(stencilOp);
        if (index < VULKAN_STENCIL_OPS.size())
            return VULKAN_STENCIL_OPS[index];
        return VK_STENCIL_OP_KEEP;
    }

    D3D12_STENCIL_OP DXStencilOp(StencilOp stencilOp)
    {
        const auto index = static_cast<uint8_t>(stencilOp);
        if (index < DX_STENCIL_OPS.size())
            return DX_STENCIL_OPS[index];
        return D3D12_STENCIL_OP_KEEP;
    }

    VkStencilOpState VulkanStencilOpState(StencilOpState stencilOpState)
    {
        VkStencilOpState state = {};
        state.failOp = VulkanStencilOp(stencilOpState.FailOp);
        state.passOp = VulkanStencilOp(stencilOpState.PassOp);
        state.depthFailOp = VulkanStencilOp(stencilOpState.DepthFailOp);
        state.compareOp = VulkanCompareOp(stencilOpState.CompareOp);
        uint32_t CompareMask = 0xFFFFFFFF;
        uint32_t WriteMask = 0xFFFFFFFF;
        uint32_t Reference = 0;
        return state;
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

    VkDescriptorType VulkanDescriptorType(DescriptorType type)
    {
        constexpr std::array<VkDescriptorType, 4> TYPES = {
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
        };
        return TYPES[static_cast<uint8_t>(type)];
    }

    D3D12_DESCRIPTOR_HEAP_TYPE DXDescriptorType(DescriptorType descriptorType)
    {
        return D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    }

    D3D12_SHADER_VISIBILITY DXShaderStageFlags(ShaderStageMask stages)
    {
        // DirectX 12 does not support fine-grained shader visibility.
        // Parameters are either visible to one stage or all stages...
        int stageCount = 0;
        if (stages.GetVertex()) stageCount++;
        if (stages.GetFragment()) stageCount++;
        if (stages.GetGeometry()) stageCount++;
        if (stages.GetTessControl()) stageCount++;
        if (stages.GetTessEval()) stageCount++;
        if (stages.GetCompute()) stageCount++;
    
        if (stageCount > 1 || stages.GetCompute())
            return D3D12_SHADER_VISIBILITY_ALL;
    
        if (stages.GetVertex())
            return D3D12_SHADER_VISIBILITY_VERTEX;
        if (stages.GetFragment())
            return D3D12_SHADER_VISIBILITY_PIXEL;
        if (stages.GetGeometry())
            return D3D12_SHADER_VISIBILITY_GEOMETRY;
        if (stages.GetTessControl())
            return D3D12_SHADER_VISIBILITY_HULL;
        if (stages.GetTessEval())
            return D3D12_SHADER_VISIBILITY_DOMAIN;
    
        return D3D12_SHADER_VISIBILITY_ALL;
    }

    VkShaderStageFlags VulkanShaderStageFlags(const ShaderStageMask flags)
    {
        VkShaderStageFlags vk = 0;

        if (flags.GetVertex())      vk |= VK_SHADER_STAGE_VERTEX_BIT;
        if (flags.GetTessControl()) vk |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        if (flags.GetTessEval())    vk |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        if (flags.GetGeometry())    vk |= VK_SHADER_STAGE_GEOMETRY_BIT;
        if (flags.GetFragment())    vk |= VK_SHADER_STAGE_FRAGMENT_BIT;
        if (flags.GetCompute())     vk |= VK_SHADER_STAGE_COMPUTE_BIT;

        return vk;
    }

    D3D12_RESOURCE_STATES ConvertLayoutToResourceState(ImageLayout layout)
    {
        switch (layout)
        {
        case ImageLayout::Undefined: return D3D12_RESOURCE_STATE_COMMON;
        case ImageLayout::General: return D3D12_RESOURCE_STATE_COMMON;
        case ImageLayout::ColorAttachment: return D3D12_RESOURCE_STATE_RENDER_TARGET;
        case ImageLayout::DepthStencilAttachment: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
        case ImageLayout::DepthStencilReadOnly: return D3D12_RESOURCE_STATE_DEPTH_READ;
        case ImageLayout::ShaderReadOnly: return D3D12_RESOURCE_STATE_GENERIC_READ;
        case ImageLayout::TransferSrc: return D3D12_RESOURCE_STATE_COPY_SOURCE;
        case ImageLayout::TransferDst: return D3D12_RESOURCE_STATE_COPY_DEST;
        case ImageLayout::Present: return D3D12_RESOURCE_STATE_PRESENT;
        default: return D3D12_RESOURCE_STATE_COMMON;
        }
    }

    void DXRenderTargetFormats(const std::vector<Format>& formats, DXGI_FORMAT outFormats[8])
    {
        for (int i = 0; i < formats.size(); i++)
        {
            outFormats[i] = DXFormat(formats[i]);
        }
    }

    VkPipelineStageFlags ConvertPipelineStage(PipelineStage stage)
    {
        switch (stage)
        {
        case PipelineStage::TopOfPipe: 
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        case PipelineStage::DrawIndirect: 
            return VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
        case PipelineStage::VertexInput: 
            return VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
        case PipelineStage::VertexShader: 
            return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
        case PipelineStage::FragmentShader: 
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case PipelineStage::EarlyFragmentTests: 
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        case PipelineStage::LateFragmentTests: 
            return VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case PipelineStage::ColorAttachmentOutput: 
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case PipelineStage::ComputeShader: 
            return VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case PipelineStage::Transfer: 
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case PipelineStage::BottomOfPipe: 
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        case PipelineStage::AllGraphics: 
            return VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
        case PipelineStage::AllCommands: 
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        default: 
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        }
    }
    
    VkImageLayout VulkanImageLayout(ImageLayout layout)
    {
        switch (layout)
        {
        case ImageLayout::Undefined: return VK_IMAGE_LAYOUT_UNDEFINED;
        case ImageLayout::General: return VK_IMAGE_LAYOUT_GENERAL;
        case ImageLayout::ColorAttachment: return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilAttachment: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        case ImageLayout::DepthStencilReadOnly: return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        case ImageLayout::ShaderReadOnly: return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        case ImageLayout::TransferSrc: return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        case ImageLayout::TransferDst: return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        case ImageLayout::Present: return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        default: return VK_IMAGE_LAYOUT_UNDEFINED;
        }
    }
    
    VkBufferUsageFlags VulkanBufferUsage(BufferUsage usage)
    {
        VkBufferUsageFlags flags = 0;
        if (usage.TransferDestination) flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if (usage.TransferSource) flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        switch (usage.Type)
        {
        case BufferType::Vertex:
            flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case BufferType::Index:
            flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case BufferType::Constant:
            flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
        case BufferType::ShaderStorage:
            flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
            break;
        }
        
        return flags;
    }

    VkImageUsageFlags VulkanImageUsage(ImageUsage usage)
    {
        VkImageUsageFlags flags = 0;
        if (usage.TransferSource) flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (usage.TransferDestination) flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        switch (usage.Type)
        {
        case ImageType::Sampled:
            flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
            break;
        case ImageType::Storage:
            flags |= VK_IMAGE_USAGE_STORAGE_BIT;
            break;
        case ImageType::RenderTarget:
            flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            break;
        case ImageType::DepthStencil:
            flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            break;
        }
        
        return flags;
    }

    D3D12_RESOURCE_FLAGS DXImageUsage(ImageUsage usage)
    {
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
        if (usage.TransferSource) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        if (usage.TransferDestination) flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        switch (usage.Type)
        {
        case ImageType::RenderTarget:
            flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            break;
        case ImageType::DepthStencil:
            flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            break;
        case ImageType::Storage:
            flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            break;
        default:
            break;
        }
        
        return flags;
    }


    D3D12_HEAP_TYPE DXMemoryType(MemoryAccess access)
    {
        return D3D12_HEAP_TYPE_DEFAULT;
    }

    VkMemoryPropertyFlags VulkanMemoryType(MemoryAccess access)
    {
        VkMemoryPropertyFlags flags = 0;

        // If CPU visible
        if (access.GetCPUWrite() || access.GetCPURead())
        {
            flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
            
            if (access.GetCPUWrite())
                flags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        }

        // If GPU visible
        if ((access.GetGPUWrite() || access.GetGPURead()))
        {
            flags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        }
    
        return flags;
    }

    UINT GetBufferDeviceAddress(const BufferAllocation& bufferAllocation)
    {
        return reinterpret_cast<UINT>(bufferAllocation.Address);
    }

    VulkanBufferData* VulkanBuffer(const BufferAllocation& bufferAllocation)
    {
        return static_cast<VulkanBufferData*>(bufferAllocation.Buffer);
    }

    ID3D12Resource* DXBuffer(const BufferAllocation& bufferAllocation)
    {
        return static_cast<ID3D12Resource*>(bufferAllocation.Buffer);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DXDescriptor(const BufferAllocation& bufferAllocation)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = {};
        handle.ptr = bufferAllocation.Descriptor;
        return handle;
    }

    VkImageViewType VulkanImageViewType(ImageDesc desc)
    {
        VkImageViewType type = VK_IMAGE_VIEW_TYPE_2D;
        if (desc.ArrayLayers > 1)
            type = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        if (desc.Depth > 1)
            type = VK_IMAGE_VIEW_TYPE_3D;
        return type;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DXDescriptor(const ImageAllocation& imageAllocation)
    {
        return static_cast<D3D12Structs::DX12ImageData*>(imageAllocation.Image)->Descriptor;
    }
}





