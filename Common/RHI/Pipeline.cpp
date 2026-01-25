#include "Pipeline.h"

#include <stdexcept>

#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"

Pipeline* Pipeline::Create(const PipelineDesc& desc)
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
    {
        return new VulkanPipeline(desc);
    }
    else if (GRAPHICS_SETTINGS.APIToUse == Direct3D12)
    {
        return new D3DPipeline(desc);
    }
    else
    {
        throw std::runtime_error("Invalid graphics API selected.");
    }
}

D3DPipeline::D3DPipeline(const PipelineDesc& desc)
{

    // This is a DirectX-specific means to store 3D vertex data in the pipeline for later use.
    // A more modern (and API agnostic) approach is to handle additional 3D transformations (outside VS/GS)
    // in compute shaders and only use graphics pipelines for a purely rasterized process.
    D3D12_STREAM_OUTPUT_DESC streamOutputDesc = {};
    streamOutputDesc.NumEntries = 0;
    
    D3D12_BLEND_DESC blendDesc = {};
    // AlphaToCoverage factors out MSAA samples per fragment based on the fragment's alpha (or combined sampled alpha).
    // Lower fragment alpha == less active MSAA samples.
    blendDesc.AlphaToCoverageEnable = desc.MultisampleState.SampleCount > 1 ? desc.MultisampleState.AlphaToCoverageEnable : FALSE;
    // Default allow different blend modes per render target.
    blendDesc.IndependentBlendEnable = desc.BlendAttachmentStates.size() > 1;
    if (desc.BlendAttachmentStates.size() > 8)
        throw std::runtime_error("Too many render targets (max 8 for D3D12)");
    for (size_t i = 0; i < desc.BlendAttachmentStates.size() && i < 8; i++)
    {
        const BlendAttachmentState& attachmentState = desc.BlendAttachmentStates[i];
        D3D12_RENDER_TARGET_BLEND_DESC& renderTargetBlendDesc = blendDesc.RenderTarget[i];
    
        renderTargetBlendDesc.BlendEnable = attachmentState.BlendEnable;
        renderTargetBlendDesc.SrcBlend = DXBlendFactor(attachmentState.SrcColorBlendFactor);
        renderTargetBlendDesc.DestBlend = DXBlendFactor(attachmentState.DestColorBlendFactor);
        renderTargetBlendDesc.BlendOp = DXBlendOp(attachmentState.ColorBlendOp);
        renderTargetBlendDesc.SrcBlendAlpha = DXBlendFactor(attachmentState.SrcAlphaBlendFactor);
        renderTargetBlendDesc.DestBlendAlpha = DXBlendFactor(attachmentState.DestAlphaBlendFactor);
        renderTargetBlendDesc.BlendOpAlpha = DXBlendOp(attachmentState.AlphaBlendOp);
        renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    
    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = DXFillMode(desc.RasterizerState.FillMode);
    rasterizerDesc.CullMode = DXCullMode(desc.RasterizerState.CullMode);
    rasterizerDesc.FrontCounterClockwise = desc.RasterizerState.FrontCounterClockwise;
    rasterizerDesc.DepthBias = desc.RasterizerState.DepthBias;
    rasterizerDesc.SlopeScaledDepthBias = desc.RasterizerState.SlopeScaledDepthBias;
    rasterizerDesc.DepthClipEnable = desc.RasterizerState.DepthClipEnable;
    rasterizerDesc.DepthBiasClamp = desc.RasterizerState.DepthBiasClamp;
    rasterizerDesc.MultisampleEnable = desc.MultisampleState.SampleCount > 1 ? TRUE : FALSE;
    
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    
    
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
    DXGI_SAMPLE_DESC sampleDesc = {};

    
    
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    
    //D3DCore::GetInstance().GetDevice()->CreateGraphicsPipelineState();
    
}

VulkanPipeline::VulkanPipeline(const PipelineDesc& desc)
{
    
}