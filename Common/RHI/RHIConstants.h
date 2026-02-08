#pragma once
#include <iostream>

#include "Pipeline.h"
#include "RHIStructures.h"

using namespace RHIStructures;

namespace RHIConstants
{
    inline constexpr BlendAttachmentState DisabledBlendAttachmentState {
        .BlendEnable = false,
        .SrcColorBlendFactor = BlendFactor::SrcAlpha,
        .DestColorBlendFactor = BlendFactor::InvSrcAlpha,
        .ColorBlendOp = BlendOp::Add,
        .SrcAlphaBlendFactor = BlendFactor::One,
        .DestAlphaBlendFactor = BlendFactor::Zero,
        .AlphaBlendOp = BlendOp::Add
    };
    
    static Pipeline* CreateRainbowTrianglePipeline()
    {
        PipelineDesc rainbowTrianglePipeline;

        // 1. Shader stages
        rainbowTrianglePipeline.VertexShader = ImportShader("vs_rainbow", "main");
        rainbowTrianglePipeline.FragmentShader = ImportShader("ps_rainbow", "main");

        if (!rainbowTrianglePipeline.VertexShader.ByteCode || rainbowTrianglePipeline.VertexShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load vertex shader!");
        if (!rainbowTrianglePipeline.FragmentShader.ByteCode || rainbowTrianglePipeline.FragmentShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load fragment shader!");
        
        // 2. Vertex input layout
        rainbowTrianglePipeline.VertexAttributes = {}; // Empty - shader uses SV_VertexID
        rainbowTrianglePipeline.VertexBindings = {};   // Empty - no bindings needed

        // 3. Primitive topology
        rainbowTrianglePipeline.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // 4. Rasterizer state
        rainbowTrianglePipeline.RasterizerState = {
            FillMode::Solid,                        // Solid fill
            CullMode::None,                         // Don't cull any faces
            false,                                  // Front face clockwise
            0.0f,                                   // No depth bias
            0.0f,                                   // No slope depth bias
            0.0f,                                   // No depth bias clamp
            true                                    // Enable depth clipping
        };

        // 5. Depth/stencil state
        rainbowTrianglePipeline.DepthStencilState = {
            false,                                  // Depth test disabled
            false,                                  // Depth write disabled
            CompareOp::Always,                      // Comparison op
            false,                                  // No depth bounds test
            0.0f,                                   // Min depth
            1.0f,                                   // Max depth
            false,                                  // Stencil test disabled
            0xFF,                                   // Stencil read mask
            0xFF,                                   // Stencil write mask
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep},  // Front
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep}   // Back
        };

        // 6. Blend state
        rainbowTrianglePipeline.BlendAttachmentStates = {
            {
                BlendOp::Add,                       // Color blend op
                BlendFactor::SrcAlpha,              // Source color factor
                BlendFactor::InvSrcAlpha,           // Dest color factor
                BlendOp::Add,                       // Alpha blend op
                BlendFactor::One,                   // Source alpha factor
                BlendFactor::Zero,                  // Dest alpha factor
                false                               // Blending DISABLED
            }
        };

        // 7. Render target format
        rainbowTrianglePipeline.RenderTargetFormats = {
            Format::R8G8B8A8_UNORM               // Standard RGBA color format
        };

        // 8. No depth stencil
        rainbowTrianglePipeline.DepthStencilFormat = Format::Unknown;

        // 9. Multisampling
        rainbowTrianglePipeline.MultisampleState = {
            1,                                      // Sample count (no MSAA)
            false                                   // No alpha to coverage
        };

        // 10. No resource bindings needed for simple triangle
        rainbowTrianglePipeline.ResourceLayout = {};

        // 11. Attachment load/store operations
        rainbowTrianglePipeline.ColorLoadOps = {AttachmentLoadOp::Clear};
        rainbowTrianglePipeline.ColorStoreOps = {AttachmentStoreOp::Store};
        rainbowTrianglePipeline.DepthLoadOp = AttachmentLoadOp::Load;
        rainbowTrianglePipeline.DepthStoreOp = AttachmentStoreOp::Store;

        return Pipeline::Create(rainbowTrianglePipeline);
    }
    
    static Pipeline* TexturedQuadPipeline()
    {
        PipelineDesc TexturedQuadDesc;

        // 1. Shader stages
        TexturedQuadDesc.VertexShader = ImportShader("vs_quad", "main");
        TexturedQuadDesc.FragmentShader = ImportShader("ps_quad", "main");

        if (!TexturedQuadDesc.VertexShader.ByteCode || TexturedQuadDesc.VertexShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load vertex shader!");
        if (!TexturedQuadDesc.FragmentShader.ByteCode || TexturedQuadDesc.FragmentShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load fragment shader!");
        
        // 2. Vertex input layout
        TexturedQuadDesc.VertexAttributes = {}; // Empty - shader uses SV_VertexID
        TexturedQuadDesc.VertexBindings = {};   // Empty - no bindings needed

        // 3. Primitive topology
        TexturedQuadDesc.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // 4. Rasterizer state
        TexturedQuadDesc.RasterizerState = {
            FillMode::Solid,                        // Solid fill
            CullMode::None,                         // Don't cull any faces
            false,                                  // Front face clockwise
            0.0f,                                   // No depth bias
            0.0f,                                   // No slope depth bias
            0.0f,                                   // No depth bias clamp
            true                                    // Enable depth clipping
        };

        // 5. Depth/stencil state
        TexturedQuadDesc.DepthStencilState = {
            false,                                  // Depth test disabled
            false,                                  // Depth write disabled
            CompareOp::Always,                      // Comparison op
            false,                                  // No depth bounds test
            0.0f,                                   // Min depth
            1.0f,                                   // Max depth
            false,                                  // Stencil test disabled
            0xFF,                                   // Stencil read mask
            0xFF,                                   // Stencil write mask
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep},  // Front
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep}   // Back
        };

        // 6. Blend state
        TexturedQuadDesc.BlendAttachmentStates = {
            {
                BlendOp::Add,                       // Color blend op
                BlendFactor::SrcAlpha,              // Source color factor
                BlendFactor::InvSrcAlpha,           // Dest color factor
                BlendOp::Add,                       // Alpha blend op
                BlendFactor::One,                   // Source alpha factor
                BlendFactor::Zero,                  // Dest alpha factor
                false                               // Blending DISABLED
            }
        };

        // 7. Render target format
        TexturedQuadDesc.RenderTargetFormats = {
            Format::R8G8B8A8_UNORM               // Standard RGBA color format
        };

        // 8. No depth stencil
        TexturedQuadDesc.DepthStencilFormat = Format::Unknown;

        // 9. Multisampling
        TexturedQuadDesc.MultisampleState = {
            1,                                      // Sample count (no MSAA)
            false                                   // No alpha to coverage
        };
        
        // 10. Binding texture
        std::vector<DescriptorBinding> bindings {
                {
                    .Type = DescriptorType::SampledImage,
                   .Slot = 0,
                   .Set = 0,
                   .Count = 1
                }
        };
        
        ShaderStageMask visibleStages;
        visibleStages.SetFragment(true);
        
        TexturedQuadDesc.ResourceLayout = {
            .Bindings = bindings,
            .VisibleStages = visibleStages
        };

        // 11. Attachment load/store operations
        TexturedQuadDesc.ColorLoadOps = {AttachmentLoadOp::Clear};
        TexturedQuadDesc.ColorStoreOps = {AttachmentStoreOp::Store};
        TexturedQuadDesc.DepthLoadOp = AttachmentLoadOp::Load;
        TexturedQuadDesc.DepthStoreOp = AttachmentStoreOp::Store;

        return Pipeline::Create(TexturedQuadDesc);
    }
    
    static Pipeline* PBRPipeline()
    {
        PipelineDesc PBRDesc;

        // 1. Shader stages
        PBRDesc.VertexShader = ImportShader("vs_pbr", "main");
        PBRDesc.FragmentShader = ImportShader("ps_pbr", "main");

        if (!PBRDesc.VertexShader.ByteCode || PBRDesc.VertexShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load vertex shader!");
        if (!PBRDesc.FragmentShader.ByteCode || PBRDesc.FragmentShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load fragment shader!");
        
        struct VertexPBR
        {
            float Position[3];
            float Normal[3];
            float Tangent[3];
            float Bitangent[3];
            float UV[2];
        };
        
        // 2. Vertex input layout
        PBRDesc.VertexBindings = {
            VertexBinding{
                .Binding   = 0,
                .Stride    = sizeof(VertexPBR),
                .Instanced = false
            }
        };
        
        PBRDesc.VertexAttributes = {
            VertexAttribute{ .SemanticName = SemanticName::Position, .Location = 0, .Binding = 0, .Format = Format::R32G32B32_FLOAT, .Offset = 0  },
            VertexAttribute{ .SemanticName = SemanticName::Normal,   .Location = 1, .Binding = 0, .Format = Format::R32G32B32_FLOAT, .Offset = 12 },
            VertexAttribute{ .SemanticName = SemanticName::Tangent,  .Location = 2, .Binding = 0, .Format = Format::R32G32B32_FLOAT, .Offset = 24 },
            VertexAttribute{ .SemanticName = SemanticName::Binormal, .Location = 2, .Binding = 0, .Format = Format::R32G32B32_FLOAT, .Offset = 36 },
            VertexAttribute{ .SemanticName = SemanticName::TexCoord, .Location = 4, .Binding = 0, .Format = Format::R32G32_FLOAT,    .Offset = 48 }
        };

        // 3. Primitive topology
        PBRDesc.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // 4. Rasterizer state
        PBRDesc.RasterizerState = {
            FillMode::Solid,                        // Solid fill
            CullMode::None,                         // Don't cull any faces
            false,                                  // Front face clockwise
            0.0f,                                   // No depth bias
            0.0f,                                   // No slope depth bias
            0.0f,                                   // No depth bias clamp
            true                                    // Enable depth clipping
        };

        // 5. Depth/stencil state
        PBRDesc.DepthStencilState = {
            false,                                  // Depth test disabled
            false,                                  // Depth write disabled
            CompareOp::Less,                        // Comparison op
            false,                                  // No depth bounds test
            0.0f,                                   // Min depth
            1.0f,                                   // Max depth
            false,                                  // Stencil test disabled
            0xFF,                                   // Stencil read mask
            0xFF,                                   // Stencil write mask
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep},  // Front
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep}   // Back
        };
        
        // 6. Blend state
        PBRDesc.BlendAttachmentStates = {
            DisabledBlendAttachmentState,
            DisabledBlendAttachmentState,
            DisabledBlendAttachmentState
        };

        // 7. Render target format
        PBRDesc.RenderTargetFormats = {
            Format::R8G8B8A8_UNORM,                 // Albedo
            Format::R16G16B16A16_FLOAT,             // Normal (high quality)
            Format::R8G8B8A8_UNORM                 // Standard RGBA color format
        };

        // 8. No depth stencil
        PBRDesc.DepthStencilFormat = Format::D32_FLOAT;

        // 9. Multisampling
        PBRDesc.MultisampleState = {
            1,                                      // Sample count (no MSAA)
            false                                   // No alpha to coverage
        };
        
        // 10. Binding texture
        std::vector<DescriptorBinding> bindings {
        { .Type = DescriptorType::UniformBuffer, .Slot = 0, .Set = 0, .Count = 1 }, // Camera/VP
        { .Type = DescriptorType::UniformBuffer, .Slot = 1, .Set = 0, .Count = 1 }, // Model
            
        { .Type = DescriptorType::SampledImage,  .Slot = 0, .Set = 0, .Count = 1 }, // Albedo
        { .Type = DescriptorType::SampledImage,  .Slot = 1, .Set = 0, .Count = 1 }, // Normal
        { .Type = DescriptorType::SampledImage,  .Slot = 2, .Set = 0, .Count = 1 }, // MetallicRoughness
        };
        
        ShaderStageMask visibleStages;
        visibleStages.SetFragment(true);
        
        PBRDesc.ResourceLayout = {
            .Bindings = bindings,
            .VisibleStages = visibleStages
        };

        // 11. Attachment load/store operations
        PBRDesc.ColorLoadOps = {AttachmentLoadOp::Clear};
        PBRDesc.ColorStoreOps = {AttachmentStoreOp::Store};
        PBRDesc.DepthLoadOp = AttachmentLoadOp::Load;
        PBRDesc.DepthStoreOp = AttachmentStoreOp::Store;

        return Pipeline::Create(PBRDesc);
    }
    
    inline constexpr ImageMemoryBarrier PRE_BARRIER{
        .SrcStage      = PipelineStage::TopOfPipe,
        .DstStage      = PipelineStage::ColorAttachmentOutput,
        .SrcAccessMask = 0u,
        .DstAccessMask = static_cast<uint32_t>(AccessFlag::ColorAttachmentWrite),
        .OldLayout     = ImageLayout::Present,
        .NewLayout     = ImageLayout::ColorAttachment,
    };

    inline constexpr ImageMemoryBarrier POST_BARRIER{
        .SrcStage      = PipelineStage::ColorAttachmentOutput,
        .DstStage      = PipelineStage::BottomOfPipe,
        .SrcAccessMask = static_cast<uint32_t>(AccessFlag::ColorAttachmentWrite),
        .DstAccessMask = 0u,
        .OldLayout     = ImageLayout::ColorAttachment,
        .NewLayout     = ImageLayout::Present,
    };
    
    inline constexpr std::vector<std::vector<uint8_t>> DefaultMetalnessRoughnessOcclusion = 
    {
        { 255 },
        { 255 },
        { 255 }
    };
    
    inline constexpr ImageUsage DefaultUploadImageUsage
    {
        .TransferSource = false,
        .TransferDestination = true,
        .Type = ImageType::Sampled
    };
    
    inline constexpr ImageDesc DefaultTextureDesc 
    {
        .Width = 0,
        .Height = 0,
        .Size = 0,
        .Format = Format::R8G8B8A8_UNORM,
        .Usage = DefaultUploadImageUsage,
        .Type = ImageType::Sampled,
        .Access = MemoryAccess(2),
        .Layout = ImageLayout::General,
        .InitialData = nullptr
    };
    
    
    
};
