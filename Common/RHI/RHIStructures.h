#pragma once
#include <cstdint>
#include <cstdint>
#include <array>
#include <d3d12.h>
#include <dxgiformat.h>
#include <vector>

#include "../Windows/WindowsHeaders.h"

namespace RHIStructures
{
    
    //=====================================//
    //  ----------  Pipeline  -----------  //
    //=====================================//
    
    enum class Format : uint8_t
    {
        Unknown = 0,
        R8G8B8A8_UNORM = 1,
        R8G8B8A8_UNORM_SRGB = 2,
        R16G16B16A16_FLOAT = 3,
        R32G32B32_FLOAT = 4,
        R32G32B32A32_FLOAT = 5,
        D16_UNORM = 6,
        D24_UNORM_S8_UINT = 7,
        D32_FLOAT = 8,
        D32_FLOAT_S8X24_UINT = 9,
        BC1_UNORM = 10,
        BC2_UNORM = 11,
        BC3_UNORM = 12,
        BC4_UNORM = 13,
        BC5_UNORM = 14,
        BC6H_UF16 = 15,
        BC7_UNORM = 16
    };
    VkFormat VulkanFormat(Format format);
    DXGI_FORMAT DXFormat(Format format);
    
    struct ShaderStage
    {
        const void* ByteCode;
        size_t ByteCodeSize;
        const char* EntryPoint;
    };
    VkShaderModule VulkanShaderModule(ShaderStage shaderStage);
    D3D12_SHADER_BYTECODE DXShaderBytecode(ShaderStage shaderStage);

    enum class SemanticName : uint8_t
    {
        Position = 0,
        Normal = 1,
        TexCoord = 2,
        Tangent = 3,
        Binormal = 4,
        Color = 5,
        BlendWeight = 6,
        BlendIndices = 7,
        PointSize = 8,
    };
    const char* SemanticNameString(SemanticName semanticName);

    struct VertexAttribute
    {
        uint32_t Binding;
        uint32_t Location;
        Format Format;
        uint32_t Offset;
        SemanticName SemanticName;
    };

    struct VertexBinding
    {
        uint32_t Binding;
        uint32_t Stride;
        bool Instanced;
    };

    enum class PrimitiveTopology : uint8_t
    {
        TriangleList = 0,
        TriangleStrip = 1,
        TriangleFan = 2,
        LineList = 3,
        LineStrip = 4,
        PointList = 5,
        PatchList1 = 6,
        PatchList2 = 7,
        PatchList3 = 8,
        PatchList4 = 9,
        PatchList5 = 10,
        PatchList6 = 11,
        PatchList7 = 12,
        PatchList8 = 13
    };
    VkPrimitiveTopology VulkanPrimitiveTopology(PrimitiveTopology primitiveTopology);
    D3D12_PRIMITIVE_TOPOLOGY DXPrimitiveTopology(PrimitiveTopology primitiveTopology);
    D3D12_PRIMITIVE_TOPOLOGY_TYPE DXPrimitiveTopologyType(PrimitiveTopology primitiveTopology);
    uint32_t GetPatchControlPoints(PrimitiveTopology primitiveTopology);

    enum class FillMode : uint8_t { Solid, Wireframe };
    VkPolygonMode VulkanFillMode(FillMode fillMode);
    D3D12_FILL_MODE DXFillMode(FillMode fillMode);
    
    enum class CullMode : uint8_t { None, Back, Front };
    VkCullModeFlags VulkanCullMode(CullMode cullMode);
    D3D12_CULL_MODE DXCullMode(CullMode cullMode);

    struct RasterizerState
    {
        FillMode FillMode;
        CullMode CullMode;
        bool FrontCounterClockwise;
        float DepthBias;
        float SlopeScaledDepthBias;
        float DepthBiasClamp;
        bool DepthClipEnable;
    };
    enum class CompareOp : uint8_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };
    VkCompareOp VulkanCompareOp(CompareOp compareOp);
    D3D12_COMPARISON_FUNC DXCompareOp(CompareOp compareOp);
    
    enum class StencilOp : uint8_t
    {
        Keep,
        Zero,
        Replace,
        IncrementAndClamp,
        DecrementAndClamp,
        Invert,
        IncrementAndWrap,
        DecrementAndWrap
    };
    VkStencilOp VulkanStencilOp(StencilOp stencilOp);
    D3D12_STENCIL_OP DXStencilOp(StencilOp stencilOp);

    struct StencilOpState
    {
        CompareOp CompareOp;
        StencilOp FailOp;
        StencilOp DepthFailOp;
        StencilOp PassOp;
    };
    VkStencilOpState VulkanStencilOpState(StencilOpState stencilOpState);
    
    struct DepthStencilState
    {
        bool DepthTestEnable;
        bool DepthWriteEnable;
        CompareOp DepthCompareOp;
        bool DepthBoundsTestEnable;
        float MinDepthBounds;
        float MaxDepthBounds;
        bool StencilTestEnable;
        uint32_t StencilReadMask;
        uint32_t StencilWriteMask;
        StencilOpState FrontStencil;
        StencilOpState BackStencil;
    };

    enum class BlendOp : uint8_t { Add, Subtract, ReverseSubtract, Min, Max };
    VkBlendOp VulkanBlendOp(BlendOp blendOp);
    D3D12_BLEND_OP DXBlendOp(BlendOp blendOp);
    
    enum class BlendFactor : uint8_t
    {
        Zero,       One,
        SrcColor,   InvSrcColor,
        SrcAlpha,   InvSrcAlpha,
        DestAlpha,  InvDestAlpha,
        DestColor,  InvDestColor,
        ConstColor, InvConstColor
    };
    VkBlendFactor VulkanBlendFactor(BlendFactor blendFactor);
    D3D12_BLEND DXBlendFactor(BlendFactor blendFactor);

    struct BlendAttachmentState
    {
        BlendOp ColorBlendOp;
        BlendFactor SrcColorBlendFactor;
        BlendFactor DestColorBlendFactor;
        BlendOp AlphaBlendOp;
        BlendFactor SrcAlphaBlendFactor;
        BlendFactor DestAlphaBlendFactor;
        bool BlendEnable;
    };

    struct MultisampleState
    {
        uint32_t SampleCount;
        bool AlphaToCoverageEnable;
    };

    enum class DescriptorType : uint8_t
    {
        UniformBuffer = 0,          // CBV in D3D12, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER in Vulkan
        StorageBuffer = 1,          // SRV buffer in D3D12, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER in Vulkan
        StorageImage = 2,           // UAV in D3D12, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE in Vulkan
        SampledImage = 3            // SRV+Sampler in D3D12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER in Vulkan
    };
    VkDescriptorType VulkanDescriptorType(DescriptorType descriptorType);
    D3D12_DESCRIPTOR_HEAP_TYPE DXDescriptorType(DescriptorType descriptorType);

    struct ShaderStageMask
    {
        bool Vertex : 1 = false;
        bool Fragment : 1 = false;
        bool Geometry : 1 = false;
        bool TessControl : 1 = false;
        bool TessEval : 1 = false;   
        bool Compute : 1 = false;
    };
    VkShaderStageFlags VulkanShaderStageFlags(ShaderStageMask flags);
    D3D12_SHADER_VISIBILITY DXShaderStageFlags(ShaderStageMask flags);
    
    struct DescriptorBinding
    {
        DescriptorType Type;
        uint32_t Slot;                  // "Binding index" (replaces Register)
        uint32_t Set;                   // "Descriptor set" (replaces Space) - 0 for most cases
        uint32_t Count;                 // For arrays
        ShaderStageMask VisibleStages;
    };

    struct ResourceLayout               // RootSignatureDesc 
    {
        std::vector<DescriptorBinding> Bindings;
    };
    
    struct PipelineDesc
    {
        ShaderStage VertexShader;
        ShaderStage FragmentShader;
        ShaderStage GeometryShader;
        ShaderStage HullShader;
        ShaderStage DomainShader;

        std::vector<VertexAttribute> VertexAttributes;
        std::vector<VertexBinding> VertexBindings;
        PrimitiveTopology PrimitiveTopology;
        RasterizerState RasterizerState;
        DepthStencilState DepthStencilState;
        std::vector<BlendAttachmentState> BlendAttachmentStates;
        std::vector<Format> RenderTargetFormats;
        Format DepthStencilFormat;
        MultisampleState MultisampleState;
        ResourceLayout ResourceLayout;
        const void* CachedPipelineData = nullptr;
        size_t CachedPipelineDataSize = 0;
    };

    void DXRenderTargetFormats(const std::vector<Format>& formats, DXGI_FORMAT outFormats[8]);
    
    
}