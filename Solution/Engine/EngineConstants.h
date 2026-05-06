#pragma once
#include "RHI/RHIStructures.h"
#include "RHI/RHIConstants.h"


namespace EngineConstants
{    
    using namespace RHIStructures;
    using namespace RHIConstants;
    
    struct ModelData 
    {
        DirectX::XMFLOAT4X4 ModelMatrix;
        DirectX::XMFLOAT4X4 NormalMatrix;
    };
    
    static PipelineDesc SkinnedPBRVariant()
    {
        PipelineDesc skinnedVariant = {};
        
        skinnedVariant.PipelineID = 1;
        skinnedVariant.IsVariant = true;
        skinnedVariant.VertexShader = ImportShader("vs_pbr_skinned", "main");
        
        skinnedVariant.VertexBindings = {
            VertexBinding{
                .Binding = 0,
                .Stride = sizeof(SkinnedVertex),
                .Instanced = false
            }
        };
        
        skinnedVariant.VertexAttributes = {
            VertexAttribute{ .Binding = 0, .Location = 0, .Format = Format::R32G32B32_FLOAT,    .Offset = 0,   .SemanticName = SemanticName::Position,     .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 1, .Format = Format::R32G32B32_FLOAT,    .Offset = 12,  .SemanticName = SemanticName::Normal,       .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 2, .Format = Format::R32G32B32_FLOAT,    .Offset = 24,  .SemanticName = SemanticName::Tangent,      .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 3, .Format = Format::R32G32B32_FLOAT,    .Offset = 36,  .SemanticName = SemanticName::Binormal,     .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 4, .Format = Format::R32G32_FLOAT,       .Offset = 48,  .SemanticName = SemanticName::TexCoord,     .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 5, .Format = Format::R32G32B32A32_FLOAT, .Offset = 56,  .SemanticName = SemanticName::BlendWeight,  .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 6, .Format = Format::R32G32B32A32_UINT,  .Offset = 72,  .SemanticName = SemanticName::BlendIndices, .SemanticIndex = 0 }
        };
        
        ShaderStageMask boneStages = ShaderStageMask(0);
        boneStages.SetVertex(true);
        
        skinnedVariant.VariantResourceLayout.Bindings = {
            { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Count = 1 }
        };
        skinnedVariant.VariantResourceLayout.VisibleStages = boneStages;
        
        return skinnedVariant;
    }

    
    static PipelineDesc PBRGeometryPipeline()
    {
        PipelineDesc PBRDescGeometry = {};
                
        PBRDescGeometry.PipelineID = 1;
        PBRDescGeometry.PipelineVariants.push_back(SkinnedPBRVariant());
        
        PBRDescGeometry.CreateOwnAttachments = true;
        PBRDescGeometry.OutputDescriptorSetIndex = 0;
        PBRDescGeometry.AttachmentWidth = 1280;
        PBRDescGeometry.AttachmentHeight = 720;

        // 1. Shader stages
        PBRDescGeometry.VertexShader = ImportShader("vs_pbr", "main");
        PBRDescGeometry.FragmentShader = ImportShader("ps_pbr", "main");

        if (!PBRDescGeometry.VertexShader.ByteCode || PBRDescGeometry.VertexShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load vertex shader!");
        if (!PBRDescGeometry.FragmentShader.ByteCode || PBRDescGeometry.FragmentShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load fragment shader!");

        // 2. Vertex input layout
        PBRDescGeometry.VertexBindings = {
            VertexBinding{
                .Binding   = 0,
                .Stride    = sizeof(Vertex),
                .Instanced = false
            }
        };
        
        PBRDescGeometry.VertexAttributes = {
            VertexAttribute{ .Binding = 0, .Location = 0, .Format = Format::R32G32B32_FLOAT, .Offset = 0,   .SemanticName = SemanticName::Position,  .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 1, .Format = Format::R32G32B32_FLOAT, .Offset = 12,  .SemanticName = SemanticName::Normal,    .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 2, .Format = Format::R32G32B32_FLOAT, .Offset = 24,  .SemanticName = SemanticName::Tangent,   .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 3, .Format = Format::R32G32B32_FLOAT, .Offset = 36,  .SemanticName = SemanticName::Binormal,  .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 4, .Format = Format::R32G32_FLOAT,    .Offset = 48,  .SemanticName = SemanticName::TexCoord,  .SemanticIndex = 0 }
        };

        // 3. Primitive topology
        PBRDescGeometry.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // 4. Rasterizer state
        PBRDescGeometry.RasterizerState = {
            FillMode::Solid,                        // Solid fill
            CullMode::Back,                         // Don't cull any faces
            false,                                  // Front face clockwise
            0.0f,                                   // No depth bias
            0.0f,                                   // No slope depth bias
            0.0f,                                   // No depth bias clamp
            true                                    // Enable depth clipping
        };

        // 5. Depth/stencil state
        PBRDescGeometry.DepthStencilState = {
            true,
            true,
            CompareOp::LessEqual,                   // Comparison op
            false,                                  // No depth bounds test
            0.0f,                                   // Min depth
            1.0f,                                   // Max depth
            false,                                  // Stencil test disabled
            0xFF,                                   // Stencil read mask
            0xFF,                                   // Stencil write mask
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep},  // Front
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep}   // Back
        };
        
        // Will combine these later
        // 6. Blend state 
        PBRDescGeometry.BlendAttachmentStates = {
            DisabledBlendAttachmentState,
            DisabledBlendAttachmentState,
            DisabledBlendAttachmentState,
            DisabledBlendAttachmentState
        };

        // 7. Render target format
        PBRDescGeometry.RenderTargetFormats = {
            Format::R8G8B8A8_UNORM,                 // Albedo
            Format::R32G32B32A32_FLOAT,             // Normal (high quality)
            Format::R8G8B8A8_UNORM,                 // Mask for Metal, Rough, and AO
            Format::R32G32B32A32_FLOAT              // Position buffer
        };
        
        PBRDescGeometry.AttachmentSamplers = {
            SamplerType::Linear,
            SamplerType::Linear,
            SamplerType::Linear,
            SamplerType::Linear
        };
        
        PBRDescGeometry.AttachmentClearValues = {
            {0,0,0,1}, 
            {0,0,0,1}, 
            {0,0,0,1}, 
            {0,0,0,1}
        };

        // 8. Depth
        PBRDescGeometry.DepthStencilFormat = Format::D32_FLOAT;
        PBRDescGeometry.CreateDepthImage = true;
        PBRDescGeometry.DepthClearValue = 1.0f;

        // 9. Multisampling
        PBRDescGeometry.MultisampleState = {
            1,                                      // Sample count (no MSAA)
            false                                   // No alpha to coverage
        };
        
        // 10. Binding texture
        std::vector<DescriptorBinding> bindings {
        { .Type = DescriptorType::SampledImage,         .Slot = 0, .Set = 1, .Count = 1, .Sampler = SamplerType::Linear }, // Albedo
        { .Type = DescriptorType::SampledImage,         .Slot = 1, .Set = 1, .Count = 1, .Sampler = SamplerType::Linear }, // Normal
        { .Type = DescriptorType::SampledImage,         .Slot = 2, .Set = 1, .Count = 1, .Sampler = SamplerType::Linear }, // MetallicRoughness
        { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = 0, .Count = 1, .Sampler = SamplerType::Linear }  // Camera VP data
        };
        
        ShaderStageMask visibleStages = ShaderStageMask(0);
        visibleStages.SetFragment(true);
        visibleStages.SetVertex(true);
        
        PBRDescGeometry.ResourceLayout = {
            .Bindings = bindings,
            .VisibleStages = visibleStages
        };

        // 11. Attachment load/store operations
        PBRDescGeometry.ColorLoadOps = {AttachmentLoadOp::Clear, AttachmentLoadOp::Clear, AttachmentLoadOp::Clear, AttachmentLoadOp::Clear};
        PBRDescGeometry.ColorStoreOps = {AttachmentStoreOp::Store, AttachmentStoreOp::Store, AttachmentStoreOp::Store, AttachmentStoreOp::Store};
        PBRDescGeometry.DepthLoadOp = AttachmentLoadOp::Clear;
        PBRDescGeometry.DepthStoreOp = AttachmentStoreOp::DontCare;
        
        // 12. Constants (ViewProjection & Model Matrices)
        ShaderStageMask constantVisibleStages = ShaderStageMask(0);
        constantVisibleStages.SetVertex(true);
        std::vector<PipelineConstant> constants {
                {
                    .Size = sizeof(ModelData),
                    .VisibleStages = constantVisibleStages
                }
        };
        PBRDescGeometry.Constants = constants;
        
        PBRDescGeometry.IsPresented = false;
        PBRDescGeometry.IsQuad = false;
        
        return PBRDescGeometry;
    }
    
    static PipelineDesc DeferredLightingPipeline()
    {
        PipelineDesc lightingDesc = {};

        lightingDesc.PipelineID = 2;
        lightingDesc.UseOwnResourceLayout = true;
        lightingDesc.InputPipelineIDs = {0, 1};
        
        // 1. Shader stages - fullscreen quad shaders
        lightingDesc.VertexShader = ImportShader("vs_lighting", "main");
        lightingDesc.FragmentShader = ImportShader("ps_lighting", "main");

        if (!lightingDesc.VertexShader.ByteCode || lightingDesc.VertexShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load fullscreen vertex shader!");
        if (!lightingDesc.FragmentShader.ByteCode || lightingDesc.FragmentShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load deferred lighting fragment shader!");

        // 2. No vertex input - fullscreen triangle generated in vertex shader
        lightingDesc.VertexBindings = {};
        lightingDesc.VertexAttributes = {};
        // 3. Primitive topology
        lightingDesc.PrimitiveTopology = PrimitiveTopology::TriangleList;

        // 4. Rasterizer state
        lightingDesc.RasterizerState = {
            FillMode::Solid,                        // Solid fill
            CullMode::None,                         // No culling for fullscreen quad
            false,                                  // Front face clockwise
            0.0f,                                   // No depth bias
            0.0f,                                   // No slope depth bias
            0.0f,                                   // No depth bias clamp
            true                                    // Enable depth clipping
        };

        // 5. Depth/stencil state - no depth testing for fullscreen pass
        lightingDesc.DepthStencilState = {
            false,                                  // No depth test
            false,                                  // No depth write
            CompareOp::Always,                      // Comparison op (ignored)
            false,                                  // No depth bounds test
            0.0f,                                   // Min depth
            1.0f,                                   // Max depth
            false,                                  // Stencil test disabled
            0xFF,                                   // Stencil read mask
            0xFF,                                   // Stencil write mask
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep},  // Front
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep}   // Back
        };

        // 6. Blend state - no blending, direct output
        lightingDesc.BlendAttachmentStates = {
            DisabledBlendAttachmentState
        };

        // 7. Single render target for final lit output
        lightingDesc.RenderTargetFormats = {
            Format::R8G8B8A8_UNORM                  // Final HDR/LDR output
        };

        lightingDesc.AttachmentClearValues = {{0,0,0,1}};
        
        // 8. No depth buffer needed for lighting pass
        lightingDesc.DepthStencilFormat = Format::Unknown;
        
        lightingDesc.DepthClearValue = 1.0f;

        // 9. No multisampling
        lightingDesc.MultisampleState = {
            1,                                      // Sample count
            false                                   // No alpha to coverage
        };

        // 10. Resource layout
        std::vector<DescriptorBinding> bindings {
            { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = 0, .Count = 1, .Sampler = SamplerType::Linear },  // Camera VP data
            { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = 3, .Count = 1, .Sampler = SamplerType::Linear },  // Light buffer
            { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = 4, .Count = 1, .Sampler = SamplerType::Linear }
        };
    
        ShaderStageMask visibleStages = ShaderStageMask(0);
        visibleStages.SetFragment(true);  // Lighting calculations need camera position
    
        lightingDesc.ResourceLayout = {
            .Bindings = bindings,
            .VisibleStages = visibleStages
        };
        
        // 11. Attachment operations - load G-buffer, output final color
        lightingDesc.ColorLoadOps = {AttachmentLoadOp::Clear};
        lightingDesc.ColorStoreOps = {AttachmentStoreOp::Store};
        lightingDesc.DepthLoadOp = AttachmentLoadOp::DontCare;
        lightingDesc.DepthStoreOp = AttachmentStoreOp::DontCare;
        
        lightingDesc.IsPresented = true;
        lightingDesc.IsQuad = true;

        return lightingDesc;
    }

    static PipelineDesc SkinnedVSMVariant()
    {
        PipelineDesc skinnedVariant = {};
        
        skinnedVariant.PipelineID = 0;
        skinnedVariant.IsVariant = true;
        skinnedVariant.VertexShader = ImportShader("vs_vsm_skinned", "main");
        skinnedVariant.ViewportSize = { 1024, 1024 };
        skinnedVariant.ViewMask = 0xF; 
        skinnedVariant.VertexBindings = {
            VertexBinding{
                .Binding = 0,
                .Stride = sizeof(SkinnedVertex),
                .Instanced = false
            }
        };
        
        skinnedVariant.VertexAttributes = {
            VertexAttribute{ .Binding = 0, .Location = 0, .Format = Format::R32G32B32_FLOAT,    .Offset = 0,   .SemanticName = SemanticName::Position,     .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 1, .Format = Format::R32G32B32_FLOAT,    .Offset = 12,  .SemanticName = SemanticName::Normal,       .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 2, .Format = Format::R32G32B32_FLOAT,    .Offset = 24,  .SemanticName = SemanticName::Tangent,      .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 3, .Format = Format::R32G32B32_FLOAT,    .Offset = 36,  .SemanticName = SemanticName::Binormal,     .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 4, .Format = Format::R32G32_FLOAT,       .Offset = 48,  .SemanticName = SemanticName::TexCoord,     .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 5, .Format = Format::R32G32B32A32_FLOAT, .Offset = 56,  .SemanticName = SemanticName::BlendWeight,  .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 6, .Format = Format::R32G32B32A32_UINT,  .Offset = 72,  .SemanticName = SemanticName::BlendIndices, .SemanticIndex = 0 }
        };
        
        ShaderStageMask boneStages = ShaderStageMask(0);
        boneStages.SetVertex(true);
        
        skinnedVariant.VariantResourceLayout.Bindings = {
            { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Count = 1 }
        };
        skinnedVariant.VariantResourceLayout.VisibleStages = boneStages;
        
        ShaderStageMask constantVisibleStages = ShaderStageMask(0);
        constantVisibleStages.SetVertex(true);
        skinnedVariant.Constants = {
            {
                .Size = sizeof(ModelData),
                .VisibleStages = constantVisibleStages
            }
        };
        
        return skinnedVariant;
    }
    
    static PipelineDesc ShadowVSMPipeline()
    {
        PipelineDesc shadowDesc;
        
        shadowDesc.PipelineID = 0;
        shadowDesc.ViewportSize = { 1024, 1024 };
        shadowDesc.PipelineVariants.push_back(SkinnedVSMVariant());
        
        shadowDesc.CreateOwnAttachments = true;
        shadowDesc.OutputDescriptorSetIndex = 1;
        shadowDesc.AttachmentWidth = 1024;
        shadowDesc.AttachmentHeight = 1024;
        shadowDesc.AttachmentArrayLayers = 4;
        shadowDesc.ViewMask = 0xF; 
        
        shadowDesc.VertexShader = ImportShader("vs_vsm", "main");
        shadowDesc.FragmentShader = ImportShader("ps_vsm", "main");
        
        if (!shadowDesc.VertexShader.ByteCode || shadowDesc.VertexShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load vertex shader!");
        if (!shadowDesc.FragmentShader.ByteCode || shadowDesc.FragmentShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load fragment shader!");
        
        shadowDesc.VertexBindings = {
            VertexBinding{
                .Binding   = 0,
                .Stride    = sizeof(Vertex),
                .Instanced = false
            }
        };
        
        shadowDesc.VertexAttributes = {
            VertexAttribute{ .Binding = 0, .Location = 0, .Format = Format::R32G32B32_FLOAT, .Offset = 0,   .SemanticName = SemanticName::Position,  .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 1, .Format = Format::R32G32B32_FLOAT, .Offset = 12,  .SemanticName = SemanticName::Normal,    .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 2, .Format = Format::R32G32B32_FLOAT, .Offset = 24,  .SemanticName = SemanticName::Tangent,   .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 3, .Format = Format::R32G32B32_FLOAT, .Offset = 36,  .SemanticName = SemanticName::Binormal,  .SemanticIndex = 0 },
            VertexAttribute{ .Binding = 0, .Location = 4, .Format = Format::R32G32_FLOAT,    .Offset = 48,  .SemanticName = SemanticName::TexCoord,  .SemanticIndex = 0 }
        };
        
        shadowDesc.PrimitiveTopology = PrimitiveTopology::TriangleList;
        
        shadowDesc.RasterizerState = {
            FillMode::Solid,                        // Solid fill
            CullMode::Back,                         // Don't cull any faces
            false,                                  // Front face clockwise
            0.0f,                                   // No depth bias
            0.0f,                                   // No slope depth bias
            0.0f,                                   // No depth bias clamp
            true                                    // Enable depth clipping
        };
        
        shadowDesc.DepthStencilState = {
            true,
            true,
            CompareOp::LessEqual,                   // Comparison op
            false,                                  // No depth bounds test
            0.0f,                                   // Min depth
            1.0f,                                   // Max depth
            false,                                  // Stencil test disabled
            0xFF,                                   // Stencil read mask
            0xFF,                                   // Stencil write mask
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep},  // Front
            {CompareOp::Always, StencilOp::Keep, StencilOp::Keep, StencilOp::Keep}   // Back
        };
        
        shadowDesc.BlendAttachmentStates = { DisabledBlendAttachmentState };
        shadowDesc.RenderTargetFormats = { Format::R32G32_FLOAT };
        shadowDesc.AttachmentSamplers = { SamplerType::Linear };
        shadowDesc.AttachmentClearValues = { {1,1,0,0} };
        shadowDesc.CreateDepthImage = true;
        shadowDesc.DepthStencilFormat = Format::D32_FLOAT;
        shadowDesc.DepthClearValue = 1.0f;
        
        shadowDesc.MultisampleState = {
            1,
            false
        };
        
        std::vector<DescriptorBinding> bindings {
            { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = 0, .Count = 1, .Sampler = SamplerType::Linear },
            { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = 1, .Count = 1, .Sampler = SamplerType::Linear }
        };
        
        ShaderStageMask visibleStages = ShaderStageMask(0);
        visibleStages.SetVertex(true);
        
        shadowDesc.ResourceLayout = {
            .Bindings = bindings,
            .VisibleStages = visibleStages
        };
        
        shadowDesc.ColorLoadOps = {AttachmentLoadOp::Clear, AttachmentLoadOp::Clear, AttachmentLoadOp::Clear, AttachmentLoadOp::Clear};
        shadowDesc.ColorStoreOps = {AttachmentStoreOp::Store, AttachmentStoreOp::Store, AttachmentStoreOp::Store, AttachmentStoreOp::Store};
        shadowDesc.DepthLoadOp = AttachmentLoadOp::Clear;
        shadowDesc.DepthStoreOp = AttachmentStoreOp::DontCare;
        
        ShaderStageMask constantVisibleStages = ShaderStageMask(0);
        constantVisibleStages.SetVertex(true);
        std::vector<PipelineConstant> constants {
                    {
                        .Size = sizeof(ModelData),
                        .VisibleStages = constantVisibleStages
                    }
        };
        shadowDesc.Constants = constants;
        
        shadowDesc.IsPresented = false;
        shadowDesc.IsQuad = false;
        
        return shadowDesc;
    }
    
}

