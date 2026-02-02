#pragma once
#include <cstdint>
#include <array>
#include <bit>
#include <d3d12.h>
#include <dxgiformat.h>
#include <string>
#include <vector>
#include "../Windows/WindowsHeaders.h"
#include <DirectXMath.h>
#include <stdexcept>

namespace VulkanStructs
{
    struct VulkanBufferData;
}


namespace RHIStructures
{
    //=====================================//
    //  ----------  Universal  ----------  //
    //=====================================//

    class Mask
    {
    protected:
        uint32_t Value = 0;
    public:

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
    VkImageAspectFlags VulkanAspects(Format format);

    
    struct ShaderStage
    {
        const void* ByteCode = nullptr;
        size_t ByteCodeSize = 0;
        const char* EntryPoint = nullptr;
    };
    
    ShaderStage ImportShader(const std::string& filename, const char* entryPoint);
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

    class ShaderStageMask : public Mask
    {
    public:
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
    
    struct DescriptorBinding
    {
        DescriptorType Type;
        uint32_t Slot;                  // "Binding index" (replaces Register)
        uint32_t Set;                   // "Descriptor set" (replaces Space) - 0 for most cases
        uint32_t Count;                 // For arrays
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

        std::vector<AttachmentLoadOp> ColorLoadOps;
        std::vector<AttachmentStoreOp> ColorStoreOps;
        AttachmentLoadOp DepthLoadOp = AttachmentLoadOp::Load;
        AttachmentStoreOp DepthStoreOp = AttachmentStoreOp::Store;
        
        const void* CachedPipelineData = nullptr;
        size_t CachedPipelineDataSize = 0;
    };
    void DXRenderTargetFormats(const std::vector<Format>& formats, DXGI_FORMAT outFormats[8]);

    //====================================//
    //  -----  Render Target Info  -----  //
    //====================================//

    struct RenderTargetAttachment
    {
        void* ImageView;                    // VkImageView* or D3D12 RTV handle
        Format Format;
        AttachmentLoadOp LoadOp;
        AttachmentStoreOp StoreOp;
        ImageLayout InitialLayout;
        ImageLayout FinalLayout;
        DirectX::XMFLOAT4 ClearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    };

    struct DepthStencilAttachment
    {
        void* ImageView;                    // VkImageView* or D3D12 DSV handle
        Format Format;
        AttachmentLoadOp DepthLoadOp;
        AttachmentStoreOp DepthStoreOp;
        AttachmentLoadOp StencilLoadOp;
        AttachmentStoreOp StencilStoreOp;
        ImageLayout InitialLayout;
        ImageLayout FinalLayout;
        float ClearDepth = 1.0f;
        uint32_t ClearStencil = 0;
    };

    struct RenderPassDesc
    {
        std::string Name;                           // For debugging
        std::vector<RenderTargetAttachment> ColorAttachments;
        DepthStencilAttachment* DepthAttachment = nullptr;
        uint32_t Width;
        uint32_t Height;
        uint32_t Layers = 1;
    };

    //===================================//
    //  ------  Memory Barriers  ------  //
    //===================================//

    enum class PipelineStage : uint32_t
    {
        TopOfPipe = 0,
        DrawIndirect = 1,
        VertexInput = 2,
        VertexShader = 3,
        FragmentShader = 7,
        EarlyFragmentTests = 8,
        LateFragmentTests = 9,
        ColorAttachmentOutput = 10,
        ComputeShader = 11,
        Transfer = 12,
        BottomOfPipe = 13,
        AllGraphics = 15,
        AllCommands = 16
    };

    enum class AccessFlag : uint32_t
    {
        IndirectCommandRead = 1 << 0,
        IndexBufferRead = 1 << 1,
        VertexAttributeRead = 1 << 2,
        UniformBufferRead = 1 << 3,
        InputAttachmentRead = 1 << 4,
        ShaderRead = 1 << 5,
        ShaderWrite = 1 << 6,
        ColorAttachmentRead = 1 << 7,
        ColorAttachmentWrite = 1 << 8,
        DepthStencilAttachmentRead = 1 << 9,
        DepthStencilAttachmentWrite = 1 << 10,
        TransferRead = 1 << 11,
        TransferWrite = 1 << 12,
        MemoryRead = 1 << 15,
        MemoryWrite = 1 << 16
    };

    struct MemoryBarrier
    {
        PipelineStage SrcStage;
        PipelineStage DstStage;
        uint32_t SrcAccessMask;
        uint32_t DstAccessMask;
    };

    struct ImageMemoryBarrier
    {
        PipelineStage SrcStage;
        PipelineStage DstStage;
        uint32_t SrcAccessMask;
        uint32_t DstAccessMask;
        ImageLayout OldLayout;
        ImageLayout NewLayout;
        void* VkImage;                        // VkImage* or D3D12 resource*
        uint32_t BaseMipLevel = 0;
        uint32_t MipLevelCount = 1;
        uint32_t BaseArrayLayer = 0;
        uint32_t ArrayLayerCount = 1;
    };
    VkPipelineStageFlags ConvertPipelineStage(PipelineStage stage);


    //===================================//
    //  ---------  Resorces  ----------  //
    //===================================//

    struct ResourceHandle
    {
        uint64_t Handle = 0;
        
        bool IsValid() const { return Handle != 0; }
        bool operator==(const ResourceHandle& other) const { return Handle == other.Handle; }
    };

    enum ResourceType : uint8_t
    {
        Buffer,
        Image,
        DescriptorSet,
        Sampler
    };

    class BufferUsage : public Mask
    {
    public:
        bool GetTransferSrc() const { return (Value >> 0) & 1; }
        bool GetTransferDst() const { return (Value >> 1) & 1; }
        bool GetUniformTexel() const { return (Value >> 2) & 1; }
        bool GetStorageTexel() const { return (Value >> 3) & 1; }
        bool GetUniform() const { return (Value >> 4) & 1; }
        bool GetStorage() const { return (Value >> 5) & 1; }
        bool GetIndex() const { return (Value >> 6) & 1; }
        bool GetVertex() const { return (Value >> 7) & 1; }

        void SetTransferSrc(bool b) { b ? Value |= (1 << 0) : Value &= ~(1 << 0); }
        void SetTransferDst(bool b) { b ? Value |= (1 << 1) : Value &= ~(1 << 1); }
        void SetUniformTexel(bool b) { b ? Value |= (1 << 2) : Value &= ~(1 << 2); }
        void SetStorageTexel(bool b) { b ? Value |= (1 << 3) : Value &= ~(1 << 3); }
        void SetUniform(bool b) { b ? Value |= (1 << 4) : Value &= ~(1 << 4); }
        void SetStorage(bool b) { b ? Value |= (1 << 5) : Value &= ~(1 << 5); }
        void SetIndex(bool b) { b ? Value |= (1 << 6) : Value &= ~(1 << 6); }
        void SetVertex(bool b) { b ? Value |= (1 << 7) : Value &= ~(1 << 7); }
    };
    VkBufferUsageFlags VulkanBufferUsage(BufferUsage usage);

    class ImageUsage : public Mask
    {
    public:
        bool GetTransferSrc() const { return (Value >> 0) & 1; }
        bool GetTransferDst() const { return (Value >> 1) & 1; }
        bool GetSampledImage() const { return (Value >> 2) & 1; }
        bool GetStorageImage() const { return (Value >> 3) & 1; }
        bool GetColorAttachment() const { return (Value >> 4) & 1; }
        bool GetDepthAttachment() const { return (Value >> 5) & 1; }
        
        void SetTransferSrc(bool b) { b ? Value |= (1 << 0) : Value &= ~(1 << 0); }
        void SetTransferDst(bool b) { b ? Value |= (1 << 1) : Value &= ~(1 << 1); }
        void SetSampledImage(bool b) { b ? Value |= (1 << 2) : Value &= ~(1 << 2); }
        void SetStorageImage(bool b) { b ? Value |= (1 << 3) : Value &= ~(1 << 3); }
        void SetColorAttachmentImage(bool b) { b ? Value |= (1 << 4) : Value &= ~(1 << 4); }
        void SetDepthAttachmentImage(bool b) { b ? Value |= (1 << 5) : Value &= ~(1 << 5); }
    };
    VkImageUsageFlags VulkanImageUsage(ImageUsage usage);
    D3D12_RESOURCE_FLAGS DXImageUsage(ImageUsage usage);

    class MemoryAccess : public Mask
    {
    public:
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

    struct BufferDesc
    {
        uint64_t Size = 0;
        BufferUsage Usage = {};
        MemoryAccess Access = {};
        const void* InitialData = nullptr;
    };

    struct BufferAllocation
    {
        void* Address = nullptr;
        uint64_t Size = 0;
        BufferUsage Usage = {};
        MemoryAccess Access = {};
        void* Buffer  = nullptr;
        bool IsMapped = false;
    };
    UINT GetBufferDeviceAddress(const BufferAllocation& bufferAllocation);
    VulkanStructs::VulkanBufferData* VulkanBuffer(const BufferAllocation& bufferAllocation);
    ID3D12Resource* DXBuffer(const BufferAllocation& bufferAllocation);

    struct ImageDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;         // 3D Textures (ie. volumetrics, lattices, voxels)
        uint32_t ArrayLayers = 1;   // Number of elements in 2D Array (ie. atlases, cubemaps)
        uint32_t MipLevels = 1;
        uint32_t SampleCount = 1;
        bool TilingLinear = false;
        uint64_t Size = 0;
        Format Format = {};
        ImageUsage Usage = {};
        MemoryAccess Access = {};
        ImageLayout InitialLayout = ImageLayout::Undefined;
        const void* InitialData = nullptr;
    };
    VkImageViewType VulkanImageViewType(ImageDesc desc);
    
    struct ImageAllocation
    {
        void* Address = nullptr;
        ImageDesc Desc = {};
        void* Image = nullptr;
    };

}