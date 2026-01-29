#pragma once
#include <cstdint>
#include <cstdint>
#include <array>
#include <d3d12.h>
#include <dxgiformat.h>
#include <string>
#include <vector>
#include "../Windows/WindowsHeaders.h"
#include <DirectXMath.h>

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
    VkImageLayout VulkanImageLayout(ImageLayout layout);


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

    enum BufferUsage : uint32_t
    {
        VertexBuffer = 1 << 0,
        IndexBuffer = 1 << 1,
        UniformBuffer = 1 << 2,
        StorageBuffer = 1 << 3,
        TransferSrcBuffer = 1 << 4,
        TransferDstBuffer = 1 << 5
    };

    enum MemoryAccess : uint32_t
    {
        CPUWrite = 1 << 0,
        CPURead = 1 << 1,
        GPUWrite = 1 << 2,
        GPURead = 1 << 3
    };

    struct BufferDesc
    {
        uint64_t Size = 0;
        uint32_t Usage = {};
        uint32_t Access = {};
        const void* InitialData = nullptr;
    };

    struct BufferAllocation
    {
        void* CPUAddress = nullptr;  // For CPU-accessible buffers
        uint64_t GPUAddress = 0;     // For GPU access
        uint64_t Size = 0;
        uint32_t Usage = {};
        uint32_t Access = {};
        
        // API-specific handles
        void* D3D12Resource = nullptr;
        void* VulkanBuffer = nullptr;
        void* VulkanMemory = nullptr;
    };

    enum ImageUsage : uint32_t
    {
        ColorAttachmentImage = 1 << 0,
        DepthAttachmentImage = 1 << 1,
        SampledImage = 1 << 2,
        StorageImage = 1 << 3,
        TransferSrcImage = 1 << 4,
        TransferDstImage = 1 << 5
    };

    struct ImageDesc
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;
        Format Format = {};
        uint32_t Usage = {};
        uint32_t MipLevels = 1;
        uint32_t ArrayLayers = 1;
    };

    struct ImageAllocation
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Depth = 1;
        Format Format = {};
        uint32_t Usage = {};
        uint32_t MipLevels = 1;
        uint32_t ArrayLayers = 1;
        
        // API-specific handles
        void* D3D12Resource = nullptr;
        void* VulkanImage = nullptr;
        void* VulkanImageView = nullptr;
        void* VulkanMemory = nullptr;
        
        // Descriptor handles for binding
        // For D3D12: Store the CPU descriptor handle directly
        D3D12_CPU_DESCRIPTOR_HANDLE D3D12CpuDescriptorHandle = {0};
        // For Vulkan: Store the image view
        void* VulkanImageViewHandle = nullptr;

    };

    struct ResourceBinding
    {
        uint32_t Slot = 0;
        uint32_t Set = 0;  // Vulkan descriptor set, D3D root parameter
        ResourceHandle Resource;
        uint32_t Offset = 0;
        uint32_t Size = 0;
    };

}