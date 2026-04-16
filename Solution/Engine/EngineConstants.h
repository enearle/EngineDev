#pragma once
#include "Common/RHI/RHIStructures.h"
#include "Common/RHI/RHIConstants.h"


namespace EngineConstants
{    
    using namespace RHIStructures;
    using namespace RHIConstants;
    
    struct ModelData 
    {
        DirectX::XMFLOAT4X4 ModelMatrix;
        DirectX::XMFLOAT4X4 NormalMatrix;
    };
    
    static PipelineDesc CreateRainbowTrianglePipeline()
    {
        PipelineDesc rainbowTrianglePipeline = {};

        // 1. Shader stages
        rainbowTrianglePipeline.VertexShader = ImportShader("vs_rainbow", "main");
        rainbowTrianglePipeline.FragmentShader = ImportShader("ps_rainbow", "main");

        if (!rainbowTrianglePipeline.VertexShader.ByteCode || rainbowTrianglePipeline.VertexShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load vertex shader!");
        if (!rainbowTrianglePipeline.FragmentShader.ByteCode || rainbowTrianglePipeline.FragmentShader.ByteCodeSize == 0)
            throw std::runtime_error("Failed to load fragment shader!");
        
        // 2. Vertex input layout
        rainbowTrianglePipeline.VertexAttributes = {};  // Empty - shader uses SV_VertexID
        rainbowTrianglePipeline.VertexBindings = {};    // Empty - no bindings needed

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
            Format::R8G8B8A8_UNORM                  // Standard RGBA color format
        };

        // 8. No depth stencil
        rainbowTrianglePipeline.DepthStencilFormat = Format::Unknown;

        // 9. Multisampling
        rainbowTrianglePipeline.MultisampleState = {
            1,                                      // Sample count (no MSAA)
            false                                   // No alpha to coverage
        };

        // 10. No resource bindings needed for simple triangle
        ResourceLayout resourceLayout = {};
        rainbowTrianglePipeline.ResourceLayout = resourceLayout;
        
        ShaderStageMask visibleStages = ShaderStageMask(0);

        // 11. Attachment load/store operations
        rainbowTrianglePipeline.ColorLoadOps = {AttachmentLoadOp::Clear};
        rainbowTrianglePipeline.ColorStoreOps = {AttachmentStoreOp::Store};
        rainbowTrianglePipeline.DepthLoadOp = AttachmentLoadOp::Load;
        rainbowTrianglePipeline.DepthStoreOp = AttachmentStoreOp::Store;

        return rainbowTrianglePipeline;
    }
    
    static PipelineDesc TexturedQuadPipeline()
    {
        PipelineDesc TexturedQuadDesc = {};

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
            Format::R8G8B8A8_UNORM                  // Standard RGBA color format
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
        
        ShaderStageMask descriptorVisibleStages = ShaderStageMask(0);
        descriptorVisibleStages.SetFragment(true);
        
        TexturedQuadDesc.ResourceLayout = {
            .Bindings = bindings,
            .VisibleStages = descriptorVisibleStages
        };

        // 11. Attachment load/store operations
        TexturedQuadDesc.ColorLoadOps = {AttachmentLoadOp::Clear};
        TexturedQuadDesc.ColorStoreOps = {AttachmentStoreOp::Store};
        TexturedQuadDesc.DepthLoadOp = AttachmentLoadOp::Load;
        TexturedQuadDesc.DepthStoreOp = AttachmentStoreOp::Store;

        return TexturedQuadDesc;
    }
    
    static PipelineDesc PBRGeometryPipeline()
    {
        PipelineDesc PBRDescGeometry = {};
        
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
        { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = 0, .Count = 1, .Sampler = SamplerType::Linear },
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

        lightingDesc.UseOwnResourceLayout = true;
        
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

        // 10. Resource layout - VP buffer for camera data (NEW)
        std::vector<DescriptorBinding> bindings {
            { .Type = DescriptorType::DynamicUniformBuffer, .Slot = 0, .Set = 0, .Count = 1, .Sampler = SamplerType::Linear }, // Camera VP data
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
    }}