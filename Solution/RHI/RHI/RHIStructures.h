#pragma once
#include <cstdint>
#include <d3d12.h>
#include <dxgiformat.h>
#include <string>
#include <vector>
#include "../RHI_API_Macro.h"
#include "Handle.h"
#include "../Windows/WindowsHeaders.h"
#include <DirectXMath.h>
#include <stdexcept>

class Pipeline;
class PipelineLayout;

namespace VulkanStructs
{
    struct VulkanBufferData;
}

namespace D3D12Structs
{
    struct DX12BufferData;
}

namespace RHIStructures
{
    //=====================================//
    //  ----------  Universal  ----------  //
    //=====================================//
    using BufferHandle = Handle<struct BufferHandleTag>;
    using ImageHandle = Handle<struct ImageHandleTag>;
    using DescriptorSetHandle = Handle<struct DescriptorSetHandleTag>;

    struct CoreInitData
    {
        bool SwapchainMSAA = false;
        uint32_t SwapchainMSAASamples = 1;
    };
    
    struct Vertex
    {
        DirectX::XMFLOAT3 Position = {0,0,0};
        DirectX::XMFLOAT3 Normal = {0,0,0};
        DirectX::XMFLOAT3 Tangent = {0,0,0};
        DirectX::XMFLOAT3 Bitangent = {0,0,0};
        DirectX::XMFLOAT2 TexCoord = {0,0};
    };
    
    struct SkinnedVertex
    {
        DirectX::XMFLOAT3 Position = {0,0,0};
        DirectX::XMFLOAT3 Normal = {0,0,0};
        DirectX::XMFLOAT3 Tangent = {0,0,0};
        DirectX::XMFLOAT3 Bitangent = {0,0,0};
        DirectX::XMFLOAT2 TexCoord = {0,0};
        DirectX::XMFLOAT4 BoneWeights = {0,0,0,0};
        DirectX::XMUINT4 BoneIndices = {0,0,0,0};
    };
    
    class Mask
    {
    protected:
        uint32_t Value = 0;
    public:

        Mask() = default;
        constexpr Mask(uint32_t value = 0) : Value(value) {}
        virtual ~Mask() = default;
        
        uint32_t Get() const { return Value; }
        void Set(uint32_t value) { Value = value; }

        operator uint32_t() const { return Value; }
        Mask& operator=(uint32_t v) { Value = v; return *this; }
        Mask& operator|=(uint32_t v) { Value |= v; return *this; }
        Mask& operator&=(uint32_t v) { Value &= v; return *this; }
        Mask& operator^=(uint32_t v) { Value ^= v; return *this; }
    };
    
    //=====================================//
    //  ----------  Pipeline  -----------  //
    //=====================================//
    
    enum class Format : uint8_t
    {
        Unknown = 0,
        R8G8B8A8_UNORM = 1,
        R8G8B8A8_UNORM_SRGB = 2,
        R16G16B16A16_FLOAT = 3,
        R32G32_FLOAT = 4,
        R32G32B32_FLOAT = 5,
        R32G32B32A32_FLOAT = 6,
        D16_UNORM = 7,
        D24_UNORM_S8_UINT = 8,
        D32_FLOAT = 9,
        D32_FLOAT_S8X24_UINT = 10,
        BC1_UNORM = 11,
        BC2_UNORM = 12,
        BC3_UNORM = 13,
        BC4_UNORM = 14,
        BC5_UNORM = 15,
        BC6H_UF16 = 16,
        BC7_UNORM = 17,
        R32G32B32A32_UINT = 18,
        R16G16_UNORM = 19,
        R32_FLOAT = 20,
        B8G8R8A8_UNORM = 21,
    };
    VkFormat VulkanFormat(Format format);
    DXGI_FORMAT DXFormat(Format format);
    VkImageAspectFlags VulkanAspects(Format format);
    size_t FormatSize(Format format);
    RHI_API bool IsDepthFormat(Format format);

    
    struct ShaderStage
    {
        const void* ByteCode = nullptr;
        size_t ByteCodeSize = 0;
        const char* EntryPoint = nullptr;
    };
    
    // Reads <compiledDirectory>/DXIL/<name>.cso or <compiledDirectory>/SPIRV/<name>.spv for the active backend.
    RHI_API ShaderStage ImportShader(const std::string& compiledDirectory, const std::string& name, const char* entryPoint);
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
        uint32_t SemanticIndex;
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
        SampledImage = 3,           // SRV+Sampler in D3D12, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER in Vulkan
        DynamicUniformBuffer = 4
    };
    VkDescriptorType VulkanDescriptorType(DescriptorType descriptorType);
    D3D12_DESCRIPTOR_HEAP_TYPE DXDescriptorType(DescriptorType descriptorType);

    class ShaderStageMask : public Mask
    {
    public:
        ShaderStageMask() : Mask(0) {}
        constexpr ShaderStageMask(uint32_t value) : Mask(value) {}
        bool GetVertex() const { return (Value >> 0) & 1; }
        bool GetTessControl() const { return (Value >> 1) & 1; }
        bool GetTessEval() const { return (Value >> 2) & 1; }
        bool GetGeometry() const { return (Value >> 3) & 1; }
        bool GetFragment() const { return (Value >> 4) & 1; }
        bool GetCompute() const { return (Value >> 5) & 1; }

        void SetVertex(bool b) { b ? Value |= (1 << 0) : Value &= ~(1 << 0); }
        void SetTessControl(bool b) { b ? Value |= (1 << 1) : Value &= ~(1 << 1); }
        void SetTessEval(bool b) { b ? Value |= (1 << 2) : Value &= ~(1 << 2); }
        void SetGeometry(bool b) { b ? Value |= (1 << 3) : Value &= ~(1 << 3); }
        void SetFragment(bool b) { b ? Value |= (1 << 4) : Value &= ~(1 << 4); }
        void SetCompute(bool b) { b ? Value |= (1 << 5) : Value &= ~(1 << 5); }
    };

    VkShaderStageFlags VulkanShaderStageFlags(ShaderStageMask flags);
    D3D12_SHADER_VISIBILITY DXShaderStageFlags(ShaderStageMask flags);
    
    enum class SamplerType
    {
        Linear,
        Nearest
    };
    
    struct DescriptorBinding
    {
        DescriptorType Type;
        uint32_t Slot;                  // "Binding index" (replaces Register)
        uint32_t Set;                   // "Descriptor set" (replaces Space) - 0 for most cases
        uint32_t Count;                 // For arrays
        SamplerType Sampler;
        uint32_t Size = 0;              // Bytes visible through a dynamic uniform buffer
        uint32_t Stride = 0;            // Element size of a storage buffer
    };
    

    struct ResourceLayout               // RootSignatureDesc 
    {
        std::vector<DescriptorBinding> Bindings;
        ShaderStageMask VisibleStages;
    };

    enum class ImageLayout : uint8_t
    {
        Undefined = 0,
        General = 1,
        ColorAttachment = 2,
        DepthStencilAttachment = 3,
        DepthStencilReadOnly = 4,
        ShaderReadOnly = 5,
        TransferSrc = 6,
        TransferDst = 7,
        Present = 8
    };
    D3D12_RESOURCE_STATES ConvertLayoutToResourceState(ImageLayout layout);
    VkImageLayout VulkanImageLayout(ImageLayout layout);

    enum class AttachmentLoadOp : uint8_t
    {
        Load = 0,       // Preserve existing contents
        Clear = 1,      // Clear to clear value
        DontCare = 2    // Contents undefined
    };

    enum class AttachmentStoreOp : uint8_t
    {
        Store = 0,      // Write results
        DontCare = 1    // Results not needed
    };
    
    struct PipelineConstant
    {
        size_t Size;
        ShaderStageMask VisibleStages;
    };
    
    // A pipeline either shares Layout with other pipelines or, when Layout is null, builds and owns
    // one from ResourceLayout and Constants.
    struct PipelineDesc
    {
        PipelineLayout* Layout = nullptr;
        ResourceLayout ResourceLayout = {};
        std::vector<PipelineConstant> Constants = {};

        ShaderStage VertexShader = {};
        ShaderStage FragmentShader = {};
        ShaderStage GeometryShader = {};
        ShaderStage HullShader = {};
        ShaderStage DomainShader = {};

        std::vector<VertexAttribute> VertexAttributes = {};
        std::vector<VertexBinding> VertexBindings = {};
        PrimitiveTopology PrimitiveTopology = PrimitiveTopology::TriangleList;
        RasterizerState RasterizerState = {};
        DepthStencilState DepthStencilState = {};
        MultisampleState MultisampleState = { 1, false };

        std::vector<BlendAttachmentState> BlendAttachmentStates = {};
        std::vector<Format> RenderTargetFormats = {};
        Format DepthStencilFormat = Format::Unknown;
        uint32_t ViewMask = 0;

        const void* CachedPipelineData = nullptr;
        size_t CachedPipelineDataSize = 0;
    };
    void DXRenderTargetFormats(const std::vector<Format>& formats, DXGI_FORMAT outFormats[8]);

    //====================================//
    //  ---------  Rendering  ----------  //
    //====================================//

    // Clear values come from the image's ImageDesc, which is also its optimized clear value on D3D12,
    // unless HasClearColour overrides it.
    struct ColorAttachment
    {
        ImageHandle Image = {};
        AttachmentLoadOp LoadOp = AttachmentLoadOp::Load;
        bool HasClearColour = false;
        DirectX::XMFLOAT4 ClearColour = {};
    };

    struct DepthAttachment
    {
        ImageHandle Image = {};
        AttachmentLoadOp LoadOp = AttachmentLoadOp::Load;
    };

    struct RenderingDesc
    {
        std::vector<ColorAttachment> ColorAttachments;
        DepthAttachment Depth;
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t ViewMask = 0;
    };

    // Stages and access masks are derived from the two layouts by the backend.
    struct ImageTransition
    {
        ImageHandle Image = {};
        ImageLayout From = ImageLayout::Undefined;
        ImageLayout To = ImageLayout::Undefined;
    };

    //===================================//
    //  ---------  Resorces  ----------  //
    //===================================//

    enum class BufferType : uint8_t
    {
        Vertex,
        Index,
        Constant,
        ShaderStorage,
        Upload,
        FrameData       // Uniform and storage use, host visible: the per-frame ring
    };

    struct ImageUsage
    {
        bool Sampled = false;
        bool RenderTarget = false;
        bool DepthStencil = false;
        bool Storage = false;
        bool TransferSource = false;
        bool TransferDestination = false;
    };
    VkImageUsageFlags VulkanImageUsage(ImageUsage usage);
    D3D12_RESOURCE_FLAGS DXImageUsage(ImageUsage usage);

    class MemoryAccess : public Mask
    {
    public:
        constexpr MemoryAccess(uint32_t value) : Mask(value) {}
        
        bool GetCPUWrite() const { return (Value >> 0) & 1; }
        bool GetCPURead() const { return (Value >> 1) & 1; }
        bool GetGPUWrite() const { return (Value >> 2) & 1; }
        bool GetGPURead() const { return (Value >> 3) & 1; }

        void SetCPUWrite(bool b) { b ? Value |= (1 << 0) : Value &= ~(1 << 0); }
        void SetCPURead(bool b) { b ? Value |= (1 << 1) : Value &= ~(1 << 1); }
        void SetGPUWrite(bool b) { b ? Value |= (1 << 2) : Value &= ~(1 << 2); }
        void SetGPURead(bool b) { b ? Value |= (1 << 3) : Value &= ~(1 << 3); }
    };
    D3D12_HEAP_TYPE DXMemoryType(MemoryAccess access);
    VkMemoryPropertyFlags VulkanMemoryType(MemoryAccess access);

    struct BufferUsage
    {
        bool TransferSource = false;
        bool TransferDestination = false;
        MemoryAccess Access = MemoryAccess(0);
        BufferType Type = BufferType::Constant;
    };
    VkBufferUsageFlags VulkanBufferUsage(BufferUsage usage);
    
    struct BufferDesc
    {
        uint64_t Size = 0;
        BufferUsage Usage = {};
        const void* InitialData = nullptr;
    };

    struct BufferAllocation
    {
        void* Address = nullptr;
        uint64_t Size = 0;
        BufferUsage Usage = {};
        void* Buffer  = nullptr;
        bool IsMapped = false;
        uint64_t Descriptor = 0;
        uint8_t DescriptorType = 0;
    };
    VulkanStructs::VulkanBufferData* VulkanBuffer(const BufferAllocation& bufferAllocation);
    D3D12Structs::DX12BufferData* DXBuffer(const BufferAllocation& bufferAllocation);
    D3D12_CPU_DESCRIPTOR_HANDLE DXDescriptor(const BufferAllocation& bufferAllocation);

    struct ImageDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;
        uint32_t ArrayLayers = 1;
        uint32_t MipLevels = 1;
        uint32_t SampleCount = 1;
        bool TilingLinear = false;
        uint64_t Size = 0;
        Format Format = {};
        ImageUsage Usage = {};
        MemoryAccess Access = MemoryAccess(8);
        ImageLayout Layout = ImageLayout::Undefined;
        const void* InitialData = nullptr;
        DirectX::XMFLOAT4 ClearColour = { 0.0f, 0.0f, 0.0f, 1.0f };
        float ClearDepth = 1.0f;
    };
    VkImageViewType VulkanImageViewType(ImageDesc desc);
    
    struct ImageAllocation
    {
        void* Address = nullptr;
        ImageDesc Desc = {};
        void* Image = nullptr;
        uint64_t Descriptor = 0;
        uint8_t DescriptorType = 0;
    };
    
    //===================================//
    //  ------  Descriptor Sets  ------  //
    //===================================//

    struct DescriptorSetBinding
    {
        uint32_t Binding = 0;
        BufferHandle Buffer = {};
        ImageHandle Image = {};

        static DescriptorSetBinding ForBuffer(uint32_t binding, BufferHandle buffer)
        {
            return DescriptorSetBinding{ .Binding = binding, .Buffer = buffer };
        }

        static DescriptorSetBinding ForImage(uint32_t binding, ImageHandle image)
        {
            return DescriptorSetBinding{ .Binding = binding, .Image = image };
        }
    };
    
    struct DescriptorSetAllocation
    {
        uint64_t DescriptorAddress = 0;         // VkDeviceAddress for Vulkan, GPU descriptor handle for DX12
        uint64_t SetKey = 0;                    // Layout ID << 32 + Set Index
        void* PlatformData = nullptr;
    };
    
    //====================================//
    //  ----------  Renderer  ----------  //
    //====================================//

    struct LightData
    {
        DirectX::XMFLOAT3 Position = {0.0f, 0.0f, 0.0f};
        float _pad0;
        DirectX::XMFLOAT3 Colour = {1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
        float Radius = 1.0f;
        uint32_t Type = 0;
        float Angle = DirectX::XM_PIDIV2;
        uint32_t ShadowIndex = 0;
    };
}