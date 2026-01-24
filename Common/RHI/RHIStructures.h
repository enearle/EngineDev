#pragma once
#include <cstdint>
#include <dxgiformat.h>
#include <vector>

#include "../Windows/WindowsHeaders.h"

namespace RHIStructures
{
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
    DXGI_FORMAT DXGIFormat(Format format);
    
    struct ShaderStage
    {
        const void* ByteCode;
        size_t ByteCodeSize;
        const char* EntryPoint;
    };

    struct VertexAttribute
    {
        uint32_t Binding;
        uint32_t Location;
        Format Format;
        uint32_t Offset;
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
        PointList = 5
    };

    enum class FillMode : uint8_t { Solid, Wireframe };
    enum class CullMode : uint8_t { None, Back, Front };

    struct RasterizerState
    {
        FillMode FillMode;
        CullMode CullMode;
        bool FrontCounterClockwise;
        float DepthBias;
        float SlopeScaledDepthBias;
        bool DepthClipEnable;
    };

    enum class CompareOp : uint8_t { Never, Less, Equal, LessEqual, Greater, NotEqual, GreaterEqual, Always };

    struct DepthStencilState
    {
        CompareOp CompareOp;
        bool DepthTestEnable;
        bool DepthWriteEnable;
        bool StencilTestEnable;
    };

    enum class BlendOp : uint8_t { Add, Subtract, ReverseSubtract, Min, Max };
    enum class BlendFactor : uint8_t
    {
        Zero,       One,
        SrcColor,   InvSrcColor,
        SrcAlpha,   InvSrcAlpha,
        DestAlpha,  InvDestAlpha,
        DestColor,  InvDestColor,
        ConstColor, InvConstColor
    };

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
        bool MultiSampleEnable;
        uint32_t SampleCount;
        
        
        
    
    };
    
};
