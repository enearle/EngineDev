#pragma once
#include "Pipeline.h"
#include "RHIStructures.h"

using namespace RHIStructures;

namespace RHIConstants
{
    static Pipeline* CreateRainbowTrianglePipeline(PipelineDesc& rainbowTrianglePipeline)
    {
        PipelineDesc rainbowTrianglePipeline;

        // 1. Shader stages
    
        rainbowTrianglePipeline.VertexShader = 
        rainbowTrianglePipeline.FragmentShader = {
            fragmentShaderBytecode,    // Your compiled PS bytecode
            fragmentShaderSize,        // Size of bytecode
            "main"                     // Entry point
        };

        // 2. Vertex input layout
        rainbowTrianglePipeline.VertexAttributes = {
            {
                0,                                    // Binding
                0,                                    // Location (POSITION)
                Format::R32G32B32_FLOAT,             // 3D position
                0,                                    // Offset
                SemanticName::Position
            },
            {
                0,                                    // Binding
                1,                                    // Location (COLOR)
                Format::R32G32B32A32_FLOAT,          // RGBA color
                12,                                   // Offset (after position)
                SemanticName::Color
            }
        };

        rainbowTrianglePipeline.VertexBindings = {
            {
                0,                                    // Binding index
                28,                                   // Stride (3 floats + 4 floats = 28 bytes)
                false                                 // Not instanced
            }
        };

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
                true                                // Blending enabled
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


    
};
