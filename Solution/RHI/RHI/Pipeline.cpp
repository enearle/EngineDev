#include "Pipeline.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include "BufferAllocator.h"
#include "RHIConstants.h"
#include "../Renderer.h"
#include "../GraphicsSettings.h"
#include "../Window.h"
#include "../DirectX12/D3D12Structs.h"
#include "../Vulkan/VulkanStructs.h"
#include "../DirectX12/D3DCore.h"
#include "../DirectX12/D3DRootSignatureBuilder.h"
#include "../Vulkan/VulkanCore.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../Vulkan/VulkanPipelineLayoutBuilder.h"

using namespace Win32ErrorHandler;
using namespace VulkanStructs;
using namespace D3D12Structs;

Pipeline* Pipeline::Create(const PipelineDesc& desc, std::vector<IOResource*>* inputIOResources)
{
    Pipeline* mainPipeline = nullptr;

    // Create the main pipeline
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        mainPipeline = new VulkanPipeline(desc, inputIOResources);
    else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        mainPipeline = new D3DPipeline(desc, inputIOResources);
    else
        throw std::runtime_error("Invalid graphics API selected.");
    
    // Create variants if specified
    if (!desc.PipelineVariants.empty())
    {
        mainPipeline->PipelineVariants.reserve(desc.PipelineVariants.size());

        for (size_t i = 0; i < desc.PipelineVariants.size(); i++)
        {
            PipelineDesc variantDesc = desc.PipelineVariants[i];
            
            variantDesc.CreateOwnAttachments = false;
            variantDesc.CreateDepthImage = false;
            
            variantDesc.AttachmentsAreViewportDims = desc.AttachmentsAreViewportDims;
            variantDesc.AttachmentWidth = desc.AttachmentWidth;
            variantDesc.AttachmentHeight = desc.AttachmentHeight;
            
            if (variantDesc.RenderTargetFormats.empty())
                variantDesc.RenderTargetFormats = desc.RenderTargetFormats;
            if (variantDesc.DepthStencilFormat == Format::Unknown)
                variantDesc.DepthStencilFormat = desc.DepthStencilFormat;
            
            if (variantDesc.BlendAttachmentStates.empty())
                variantDesc.BlendAttachmentStates = desc.BlendAttachmentStates;
            
            if (variantDesc.MultisampleState.SampleCount == 0)
                variantDesc.MultisampleState = desc.MultisampleState;
            
            if (variantDesc.Constants.empty())
                variantDesc.Constants = desc.Constants;
            
            if (variantDesc.DepthStencilState.DepthCompareOp == CompareOp::Never)
                variantDesc.DepthStencilState = desc.DepthStencilState;
            
            variantDesc.RasterizerState = desc.RasterizerState;
            
            if (variantDesc.ResourceLayout.Bindings.empty())
                variantDesc.ResourceLayout = desc.ResourceLayout;
            
            if (variantDesc.PrimitiveTopology == PrimitiveTopology::TriangleList && 
                desc.PrimitiveTopology != PrimitiveTopology::TriangleList)
                variantDesc.PrimitiveTopology = desc.PrimitiveTopology;
            
            if (variantDesc.FragmentShader.ByteCode == nullptr)
                variantDesc.FragmentShader = desc.FragmentShader;
            
            if (variantDesc.AttachmentSamplers.empty())
                variantDesc.AttachmentSamplers = desc.AttachmentSamplers;
            
            if (variantDesc.AttachmentClearValues.empty())
                variantDesc.AttachmentClearValues = desc.AttachmentClearValues;
            
            
            Pipeline* variant = nullptr;

            if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
                variant = new VulkanPipeline(variantDesc, inputIOResources);
            else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
                variant = new D3DPipeline(variantDesc, inputIOResources);

            mainPipeline->PipelineVariants.push_back(variant);
        }
    }
    
    return mainPipeline;
}

D3DPipeline::D3DPipeline(const PipelineDesc& desc, std::vector<IOResource*>* inputIOResources)
{
    ClearColors = desc.AttachmentClearValues;
    DepthClearValue = desc.DepthClearValue;
    PushConstantCount = static_cast<uint32_t>(desc.Constants.size());
    IsVariant = desc.IsVariant;
    ViewMask = desc.ViewMask;
    ArrayLayerCount = desc.AttachmentArrayLayers;
    
    // For recreating attachments
    RenderTargetFormats = desc.RenderTargetFormats;
    AttachmentSamplers = desc.AttachmentSamplers;
    DepthStencilFormat = desc.DepthStencilFormat;
    OutputDescriptorSetIndex = desc.OutputDescriptorSetIndex;
    CreateOwnAttachments = desc.CreateOwnAttachments;
    CreateDepthImage = desc.CreateDepthImage;
    CreateDepthAttachment = desc.CreateDepthAttachment;
    MultisampleStateInfo = desc.MultisampleState;
    AttachmentsAreViewportDims = desc.AttachmentsAreViewportDims;
    
    Window* window = Renderer::GetWindow();
    uint32_t width = desc.AttachmentsAreViewportDims ? window->GetWidth() : desc.AttachmentWidth;
    uint32_t height = desc.AttachmentsAreViewportDims ? window->GetHeight() : desc.AttachmentHeight;
    ViewportSize = {width, height};
    
    ComPtr<ID3D12Device> device = D3DCore::Instance().GetDevice();
    Topology = DXPrimitiveTopology(desc.PrimitiveTopology);
    
    std::vector<ResourceLayout> resourceLayouts;
    
    if (desc.UseOwnResourceLayout)
        resourceLayouts.push_back(desc.ResourceLayout);
    
    if (inputIOResources && !desc.IsVariant)
        for (IOResource* ioResource : *inputIOResources)
            resourceLayouts.push_back(ioResource->Layout);

    for (uint32_t i = desc.UseOwnResourceLayout ? 1 : 0; i < resourceLayouts.size(); i++)
        for (auto& binding : resourceLayouts[i].Bindings)
            if (binding.Set < RHIConstants::VARIANT_DESCRIPTOR_SET_BASE)
                binding.Set = desc.UseOwnResourceLayout ? i + 1 : i;

    std::vector<ResourceLayout> splitLayouts;
    for (const auto& layout : resourceLayouts)
    {
        std::map<uint32_t, std::vector<DescriptorBinding>> bindingsBySet;
        for (const auto& binding : layout.Bindings)
            bindingsBySet[binding.Set].push_back(binding);
    
        for (const auto& [setIdx, bindings] : bindingsBySet)
        {
            ResourceLayout splitLayout = layout;
            splitLayout.Bindings = bindings;
            splitLayouts.push_back(splitLayout);
        }
    }
    resourceLayouts = splitLayouts;
    
    std::sort(resourceLayouts.begin(), resourceLayouts.end(), 
    [](const ResourceLayout& a, const ResourceLayout& b) {
        uint32_t setA = a.Bindings.empty() ? 0 : a.Bindings[0].Set;
        uint32_t setB = b.Bindings.empty() ? 0 : b.Bindings[0].Set;
        return setA < setB;
    });
    
    if (desc.IsVariant)
    {
        ResourceLayout variantLayout = desc.VariantResourceLayout;
        
        for (auto& binding : variantLayout.Bindings)
            binding.Set = RHIConstants::VARIANT_DESCRIPTOR_SET_BASE;
    
        resourceLayouts.push_back(variantLayout);
    }
    
    RootSignature = D3DRootSignatureBuilder::BuildRootSignature(desc.PipelineID, resourceLayouts, desc.Constants, SetIndexToBuilderIndex);
    
    if (inputIOResources && !desc.IsVariant)
    {
        for (uint32_t i = 0; i < inputIOResources->size(); i++)
        {
            IOResource* ioResource = inputIOResources->at(i);
            uint32_t setIndex = ioResource->Layout.Bindings.empty() ? 0 : ioResource->Layout.Bindings[0].Set;

            uint64_t descriptorSetID = BufferAllocator::GetInstance()->AllocateDescriptorSet(
                desc.PipelineID, setIndex, ioResource->Bindings);
            PipelineInputDescriptorSetIDs.push_back(descriptorSetID);
            InputIOResources.push_back(ioResource);
        }
    }

    // This is a DirectX-specific means to store 3D vertex data in the pipeline for later use.
    // A more modern (and API agnostic) approach is to handle additional 3D transformations (outside VS/GS)
    // in compute shaders and only use graphics pipelines for a purely rasterized process.
    D3D12_STREAM_OUTPUT_DESC streamOutputDesc = {};
    streamOutputDesc.NumEntries = 0;
    
    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = desc.MultisampleState.SampleCount > 1 ? desc.MultisampleState.AlphaToCoverageEnable : FALSE;
    blendDesc.IndependentBlendEnable = desc.BlendAttachmentStates.size() > 1;
    if (desc.BlendAttachmentStates.size() > 8)
        throw std::runtime_error("Too many render targets (max 8 for D3D12)");
    
    for (int i = 0; i < 8; i++)
    {
        blendDesc.RenderTarget[i] = {};
        blendDesc.RenderTarget[i].BlendEnable = FALSE;
        blendDesc.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }

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
    
    D3D12_DEPTH_STENCILOP_DESC frontFaceStencil = {};
    frontFaceStencil.StencilFailOp = DXStencilOp(desc.DepthStencilState.FrontStencil.FailOp);
    frontFaceStencil.StencilDepthFailOp = DXStencilOp(desc.DepthStencilState.FrontStencil.DepthFailOp);
    frontFaceStencil.StencilPassOp = DXStencilOp(desc.DepthStencilState.FrontStencil.PassOp);
    frontFaceStencil.StencilFunc = DXCompareOp(desc.DepthStencilState.FrontStencil.CompareOp);

    D3D12_DEPTH_STENCILOP_DESC backFaceStencil = {};
    backFaceStencil.StencilFailOp = DXStencilOp(desc.DepthStencilState.BackStencil.FailOp);
    backFaceStencil.StencilDepthFailOp = DXStencilOp(desc.DepthStencilState.BackStencil.DepthFailOp);
    backFaceStencil.StencilPassOp = DXStencilOp(desc.DepthStencilState.BackStencil.PassOp);
    backFaceStencil.StencilFunc = DXCompareOp(desc.DepthStencilState.BackStencil.CompareOp);
    
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = desc.DepthStencilState.DepthTestEnable;
    depthStencilDesc.DepthWriteMask = desc.DepthStencilState.DepthWriteEnable
        ? D3D12_DEPTH_WRITE_MASK_ALL
        : D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = DXCompareOp(desc.DepthStencilState.DepthCompareOp);
    depthStencilDesc.StencilEnable = desc.DepthStencilState.StencilTestEnable;
    depthStencilDesc.StencilReadMask = desc.DepthStencilState.StencilReadMask;
    depthStencilDesc.StencilWriteMask = desc.DepthStencilState.StencilWriteMask;
    depthStencilDesc.FrontFace = frontFaceStencil;
    depthStencilDesc.BackFace = backFaceStencil;
    
    D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
    for (const auto& attr : desc.VertexAttributes)
    {
        D3D12_INPUT_ELEMENT_DESC element = {};
        const char* semanticName = SemanticNameString(attr.SemanticName);
        element.SemanticName = !semanticName ? "TEXCOORD" : semanticName;
        element.SemanticIndex = attr.SemanticIndex;
        element.Format = DXFormat(attr.Format);
        element.InputSlot = attr.Binding;
        element.AlignedByteOffset = attr.Offset;
        element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    
        // Check if this binding is instanced
        for (const auto& binding : desc.VertexBindings)
        {
            if (binding.Binding == attr.Binding && binding.Instanced)
            {
                element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                element.InstanceDataStepRate = 1;
                break;
            }
        }
    
        inputElements.push_back(element);
    }
    
    if (!inputElements.empty())
    {
        inputLayoutDesc.pInputElementDescs = inputElements.data();
        inputLayoutDesc.NumElements = static_cast<UINT>(inputElements.size());
    }
    else
    {
        inputLayoutDesc.pInputElementDescs = nullptr;
        inputLayoutDesc.NumElements = 0;
    }
    
    DXGI_SAMPLE_DESC sampleDesc = {};
    if (desc.MultisampleState.SampleCount > 1)
    {
        sampleDesc.Count = desc.MultisampleState.SampleCount;
        sampleDesc.Quality = D3DCore::Instance().GetMSAAQualityLevel(
            DXFormat(desc.RenderTargetFormats[0]),
            desc.MultisampleState.SampleCount
        );
    }
    else
    {
        sampleDesc.Count = 1;
        sampleDesc.Quality = 0;
    }
    
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    pipelineStateDesc.pRootSignature = RootSignature.Get();
    pipelineStateDesc.VS = DXShaderBytecode(desc.VertexShader);
    pipelineStateDesc.PS = DXShaderBytecode(desc.FragmentShader);
    pipelineStateDesc.DS = DXShaderBytecode(desc.DomainShader);
    pipelineStateDesc.HS = DXShaderBytecode(desc.HullShader);
    pipelineStateDesc.GS = DXShaderBytecode(desc.GeometryShader);
    pipelineStateDesc.StreamOutput = streamOutputDesc;
    pipelineStateDesc.BlendState = blendDesc;
    pipelineStateDesc.SampleMask = UINT_MAX; // Mask to toggle active msaa samples (useful for TAA and other optimizations)
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.InputLayout = inputLayoutDesc;
    pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED; // Splits up geometry in a single draw call (rarely useful and can be done by other explicit means)
    pipelineStateDesc.PrimitiveTopologyType = DXPrimitiveTopologyType(desc.PrimitiveTopology);
    pipelineStateDesc.NumRenderTargets = static_cast<UINT>(desc.RenderTargetFormats.size());
    
    // Initialize RTVFormats array to UNKNOWN first
    for (int i = 0; i < 8; i++)
        pipelineStateDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN;

    // Then fill in the actual formats
    for (size_t i = 0; i < desc.RenderTargetFormats.size() && i < 8; i++)
        pipelineStateDesc.RTVFormats[i] = DXFormat(desc.RenderTargetFormats[i]);
    

    pipelineStateDesc.DSVFormat = DXFormat(desc.DepthStencilFormat);
    pipelineStateDesc.SampleDesc = sampleDesc;
    pipelineStateDesc.NodeMask = 0; // Which (or both) gpu(s) to use in multi-gpu setup (NVidia SLI or AMD Crossfire)
    // Cached PSO if there is one
    if (desc.CachedPipelineData && desc.CachedPipelineDataSize > 0)
    {
        pipelineStateDesc.CachedPSO.pCachedBlob = desc.CachedPipelineData;
        pipelineStateDesc.CachedPSO.CachedBlobSizeInBytes = desc.CachedPipelineDataSize;
    }
    else
        pipelineStateDesc.CachedPSO = {};
    
    D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
    pipelineStateDesc.Flags = flags;

    device->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&PipelineState)) >> ERROR_HANDLER;
    
    BufferAllocator* alloc = BufferAllocator::GetInstance();
    DirectX12BufferAllocator* dxAlloc = static_cast<DirectX12BufferAllocator*>(alloc);
    
    // Create depth image
    DescriptorSetBinding depthBindingData{};
    if (desc.CreateDepthImage)
    {
        D3D12_RESOURCE_DESC depthResourceDesc = {};
        depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthResourceDesc.Alignment = 0;
        depthResourceDesc.Width = ViewportSize.x;
        depthResourceDesc.Height = ViewportSize.y;
        depthResourceDesc.DepthOrArraySize = desc.AttachmentArrayLayers;
        depthResourceDesc.MipLevels = 1;
        depthResourceDesc.Format = DXFormat(desc.DepthStencilFormat);
        depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depthResourceDesc.SampleDesc.Count = 1;
        depthResourceDesc.SampleDesc.Quality = 0;
    
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthResourceDesc, 
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&OwnedDepthResource)) >> ERROR_HANDLER;
    
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXFormat(desc.DepthStencilFormat);
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.Texture2D.MipSlice = 0;
          
        OwnedDSV = dxAlloc->AllocateDescriptor(DirectX12BufferAllocator::DescriptorType::DSV);
        device->CreateDepthStencilView(OwnedDepthResource.Get(), &dsvDesc, OwnedDSV);
    
        // Cache depth image for next pipeline
        DX12ImageData* depthImageData = new DX12ImageData();
        depthImageData->Image = OwnedDepthResource;
    
        ImageAllocation depthAllocation;
        depthAllocation.Image = depthImageData;
        depthAllocation.Desc.Format = desc.DepthStencilFormat;
        depthAllocation.Desc.Width = ViewportSize.x;
        depthAllocation.Desc.Height = ViewportSize.y;
    
        depthBindingData.Binding = static_cast<uint32_t>(desc.RenderTargetFormats.size());
        depthBindingData.ResourceID = alloc->CacheImage(depthAllocation);
    }
    
    if (!desc.CreateOwnAttachments) return;
    PipelineOutputResource = new IOResource();
    
    OwnedRTVs.resize(desc.RenderTargetFormats.size());
    OwnedColorResources.resize(desc.RenderTargetFormats.size());
    
    PipelineOutputResource->Layout.VisibleStages.SetFragment(true);
    
    for (size_t i = 0; i < desc.RenderTargetFormats.size(); i++)
    {
        DXGI_SAMPLE_DESC sampleDesc = {};
        if (desc.MultisampleState.SampleCount > 1)
        {
            sampleDesc.Count = desc.MultisampleState.SampleCount;
            sampleDesc.Quality = D3DCore::Instance().GetMSAAQualityLevel(
                DXFormat(desc.RenderTargetFormats[i]),
                desc.MultisampleState.SampleCount
            );
        }
        else
        {
            sampleDesc.Count = 1;
            sampleDesc.Quality = 0;
        }
        
        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Alignment = 0;
        resourceDesc.Width = ViewportSize.x;
        resourceDesc.Height = ViewportSize.y;
        resourceDesc.DepthOrArraySize = desc.AttachmentArrayLayers;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXFormat(desc.RenderTargetFormats[i]);
        resourceDesc.SampleDesc = sampleDesc;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, 
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&OwnedColorResources[i])) >> ERROR_HANDLER;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXFormat(desc.RenderTargetFormats[i]);
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        
        OwnedRTVs[i] = dxAlloc->AllocateDescriptor(DirectX12BufferAllocator::DescriptorType::RTV);
        device->CreateRenderTargetView(OwnedColorResources[i].Get(), &rtvDesc, OwnedRTVs[i]);
    
        DescriptorBinding binding{};
        binding.Type = DescriptorType::SampledImage;
        binding.Count = 1;
        binding.Set = desc.OutputDescriptorSetIndex;
        binding.Slot = static_cast<uint32_t>(i);
        PipelineOutputResource->Layout.Bindings.push_back(binding);
        
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXFormat(desc.RenderTargetFormats[i]);
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (desc.AttachmentArrayLayers > 1)
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
            srvDesc.Texture2DArray.MipLevels = 1;
            srvDesc.Texture2DArray.MostDetailedMip = 0;
            srvDesc.Texture2DArray.FirstArraySlice = 0;
            srvDesc.Texture2DArray.ArraySize = desc.AttachmentArrayLayers;
        }
        else
        {
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srvDesc.Texture2D.MipLevels = 1;
            srvDesc.Texture2D.MostDetailedMip = 0;
        }

        DX12ImageData* imageData = new DX12ImageData();
        imageData->Image = OwnedColorResources[i];
        if (!imageData->Image.Get())
            throw std::runtime_error("Render target resource is NULL!");
        ImageAllocation imageAllocation;
        imageAllocation.Image = imageData;
        imageAllocation.Desc.Format = desc.RenderTargetFormats[i];
        imageAllocation.Desc.Width = ViewportSize.x;
        imageAllocation.Desc.Height = ViewportSize.y;
    
        DescriptorSetBinding bindingData{};
        bindingData.Binding = binding.Slot;
        bindingData.ResourceID = alloc->CacheImage(imageAllocation);
        PipelineOutputResource->Bindings.push_back(bindingData);
    }
    
    if (desc.CreateDepthAttachment && desc.CreateDepthImage)
    {
        PipelineOutputResource->Bindings.push_back(depthBindingData);
    }
}

void D3DPipeline::RecreateAttachments(uint32_t width, uint32_t height)
{
    if (!AttachmentsAreViewportDims || !CreateOwnAttachments)
        return;

    ViewportSize = {width, height};

    ComPtr<ID3D12Device> device = D3DCore::Instance().GetDevice();
    BufferAllocator* alloc = BufferAllocator::GetInstance();
    DirectX12BufferAllocator* dxAlloc = static_cast<DirectX12BufferAllocator*>(alloc);

    // Recreate depth image
    if (CreateDepthImage && OwnedDepthResource)
    {
        OwnedDepthResource.Reset();

        D3D12_RESOURCE_DESC depthResourceDesc = {};
        depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthResourceDesc.Width = width;
        depthResourceDesc.Height = height;
        depthResourceDesc.DepthOrArraySize = static_cast<UINT16>(ArrayLayerCount);
        depthResourceDesc.MipLevels = 1;
        depthResourceDesc.Format = DXFormat(DepthStencilFormat);
        depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depthResourceDesc.SampleDesc.Count = 1;
        depthResourceDesc.SampleDesc.Quality = 0;

        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthResourceDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&OwnedDepthResource)) >> ERROR_HANDLER;

        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXFormat(DepthStencilFormat);
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        device->CreateDepthStencilView(OwnedDepthResource.Get(), &dsvDesc, OwnedDSV);
    }

    // Recreate color attachments
    for (size_t i = 0; i < RenderTargetFormats.size(); i++)
    {
        OwnedColorResources[i].Reset();

        D3D12_RESOURCE_DESC resourceDesc = {};
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        resourceDesc.Width = width;
        resourceDesc.Height = height;
        resourceDesc.DepthOrArraySize = static_cast<UINT16>(ArrayLayerCount);
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXFormat(RenderTargetFormats[i]);
        resourceDesc.SampleDesc.Count = MultisampleStateInfo.SampleCount > 1 ? MultisampleStateInfo.SampleCount : 1;
        resourceDesc.SampleDesc.Quality = MultisampleStateInfo.SampleCount > 1
            ? D3DCore::Instance().GetMSAAQualityLevel(DXFormat(RenderTargetFormats[i]), MultisampleStateInfo.SampleCount)
            : 0;
        resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
            D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&OwnedColorResources[i])) >> ERROR_HANDLER;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXFormat(RenderTargetFormats[i]);
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        device->CreateRenderTargetView(OwnedColorResources[i].Get(), &rtvDesc, OwnedRTVs[i]);
    }
}

//================================================//
// Vulkan                                         //
//================================================//

VulkanPipeline::VulkanPipeline(const PipelineDesc& desc, std::vector<IOResource*>* inputIOResources)
{
    ClearColors = desc.AttachmentClearValues;
    DepthClearValue = desc.DepthClearValue;
    PushConstantCount = static_cast<uint32_t>(desc.Constants.size());
    IsVariant = desc.IsVariant;
    ViewMask = desc.ViewMask;
    ArrayLayerCount = desc.AttachmentArrayLayers;
    
    // For recreating attachments
    RenderTargetFormats = desc.RenderTargetFormats;
    AttachmentSamplers = desc.AttachmentSamplers;
    DepthStencilFormat = desc.DepthStencilFormat;
    OutputDescriptorSetIndex = desc.OutputDescriptorSetIndex;
    CreateOwnAttachments = desc.CreateOwnAttachments;
    CreateDepthImage = desc.CreateDepthImage;
    CreateDepthAttachment = desc.CreateDepthAttachment;
    MultisampleStateInfo = desc.MultisampleState;
    AttachmentsAreViewportDims = desc.AttachmentsAreViewportDims;
    
    Window* window = Renderer::GetWindow();
    uint32_t width = desc.AttachmentsAreViewportDims ? window->GetWidth() : desc.AttachmentWidth;
    uint32_t height = desc.AttachmentsAreViewportDims ? window->GetHeight() : desc.AttachmentHeight;
    ViewportSize = {width, height};
    
    // Cache shader modules for cleanup
    // All shaders will allways be loaded. This is meh, but for my engine probably fine.
    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    if (desc.VertexShader.ByteCode)
    {
        VkShaderModule vertModule = VulkanShaderModule(desc.VertexShader);
        VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
        vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexShaderStageInfo.module = vertModule;
        vertexShaderStageInfo.pName = desc.VertexShader.EntryPoint ? desc.VertexShader.EntryPoint : "main";
        shaderStages.push_back(vertexShaderStageInfo);
        ShaderModules.push_back(vertModule);
    }
    if (desc.FragmentShader.ByteCode)
    {
        VkShaderModule fragModule = VulkanShaderModule(desc.FragmentShader);
        VkPipelineShaderStageCreateInfo fragmentShaderStageInfo{};
        fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragmentShaderStageInfo.module = fragModule;
        fragmentShaderStageInfo.pName = desc.FragmentShader.EntryPoint ? desc.FragmentShader.EntryPoint : "main";
        shaderStages.push_back(fragmentShaderStageInfo);
        ShaderModules.push_back(fragModule);
    }
    if (desc.GeometryShader.ByteCode)
    {
        VkShaderModule geomModule = VulkanShaderModule(desc.GeometryShader);
        VkPipelineShaderStageCreateInfo geometryShaderStageInfo{};
        geometryShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        geometryShaderStageInfo.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
        geometryShaderStageInfo.module = geomModule;
        geometryShaderStageInfo.pName = desc.GeometryShader.EntryPoint ? desc.GeometryShader.EntryPoint : "main";
        shaderStages.push_back(geometryShaderStageInfo);
        ShaderModules.push_back(geomModule);
    }
    if (desc.DomainShader.ByteCode)
    {
        VkShaderModule domainModule = VulkanShaderModule(desc.DomainShader);
        VkPipelineShaderStageCreateInfo domainShaderStageInfo{};
        domainShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        domainShaderStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        domainShaderStageInfo.module = domainModule;
        domainShaderStageInfo.pName = desc.DomainShader.EntryPoint ? desc.DomainShader.EntryPoint : "main";
        shaderStages.push_back(domainShaderStageInfo);
        ShaderModules.push_back(domainModule);
    }
    if (desc.HullShader.ByteCode)
    {
        VkShaderModule hullModule = VulkanShaderModule(desc.HullShader);
        VkPipelineShaderStageCreateInfo hullShaderStageInfo{};
        hullShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        hullShaderStageInfo.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        hullShaderStageInfo.module = hullModule;
        hullShaderStageInfo.pName = desc.HullShader.EntryPoint ? desc.HullShader.EntryPoint : "main";
        shaderStages.push_back(hullShaderStageInfo);
        ShaderModules.push_back(hullModule);
    }

    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    for (const VertexBinding& binding : desc.VertexBindings)
    {
        VkVertexInputBindingDescription bindingDesc{};
        bindingDesc.binding = binding.Binding;
        bindingDesc.stride = binding.Stride;
        bindingDesc.inputRate = binding.Instanced 
            ? VK_VERTEX_INPUT_RATE_INSTANCE 
            : VK_VERTEX_INPUT_RATE_VERTEX;
    
        bindingDescriptions.push_back(bindingDesc);
    }

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    for (const VertexAttribute& attr : desc.VertexAttributes)
    {
        VkVertexInputAttributeDescription attributeDesc{};
        attributeDesc.binding = attr.Binding;
        attributeDesc.location = attr.Location;
        attributeDesc.format = VulkanFormat(attr.Format);
        attributeDesc.offset = attr.Offset;
    
        attributeDescriptions.push_back(attributeDesc);
    }

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VulkanPrimitiveTopology(desc.PrimitiveTopology);
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineTessellationStateCreateInfo tessellation{};
    tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation.patchControlPoints = GetPatchControlPoints(desc.PrimitiveTopology);
    bool tessellationEnabled = desc.HullShader.ByteCode && desc.DomainShader.ByteCode;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = !desc.RasterizerState.DepthClipEnable;  // Inverted. Clip and Clamp are opposite behaviours.
    rasterizer.depthBiasClamp = desc.RasterizerState.DepthBiasClamp;
    rasterizer.cullMode = VulkanCullMode(desc.RasterizerState.CullMode);
    rasterizer.frontFace = desc.RasterizerState.FrontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE; // Inverting because of negative viewport height
    rasterizer.depthBiasEnable = desc.RasterizerState.DepthBias != 0.0f;
    rasterizer.depthBiasConstantFactor = desc.RasterizerState.DepthBias;
    rasterizer.depthBiasSlopeFactor = desc.RasterizerState.SlopeScaledDepthBias;
    rasterizer.polygonMode = VulkanFillMode(desc.RasterizerState.FillMode);
    rasterizer.lineWidth = 1.0f;
    rasterizer.rasterizerDiscardEnable = VK_FALSE; // Vulkan only. Skips rasterizer and subsequent stages. Like Stream Output in DX12, this is made irrelevant by compute pipelines.

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.alphaToCoverageEnable = desc.MultisampleState.SampleCount > 1 ? desc.MultisampleState.AlphaToCoverageEnable : VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;
    multisampling.rasterizationSamples = static_cast<VkSampleCountFlagBits>(desc.MultisampleState.SampleCount);
    multisampling.sampleShadingEnable = desc.MultisampleState.SampleCount > 1 ? VK_TRUE : VK_FALSE;
    multisampling.minSampleShading = desc.MultisampleState.SampleCount > 1 ? 0.5f : 1.0f;  // Sample shading affects the colour quality of MSAA samples.
    multisampling.pSampleMask = nullptr;

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    for (const BlendAttachmentState& attachmentState : desc.BlendAttachmentStates)
    {
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.alphaBlendOp = VulkanBlendOp(attachmentState.AlphaBlendOp);
        blendAttachment.blendEnable = attachmentState.BlendEnable;
        blendAttachment.colorBlendOp = VulkanBlendOp(attachmentState.ColorBlendOp);
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | 
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachment.srcColorBlendFactor = VulkanBlendFactor(attachmentState.SrcColorBlendFactor);
        blendAttachment.dstColorBlendFactor = VulkanBlendFactor(attachmentState.DestColorBlendFactor);
        blendAttachment.srcAlphaBlendFactor = VulkanBlendFactor(attachmentState.SrcAlphaBlendFactor);
        blendAttachment.dstAlphaBlendFactor = VulkanBlendFactor(attachmentState.DestAlphaBlendFactor);
        blendAttachments.push_back(blendAttachment);
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachments.size());
    colorBlendState.pAttachments = blendAttachments.data();

    VkPipelineDepthStencilStateCreateInfo depthStencilState{};
    depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilState.depthTestEnable = desc.DepthStencilState.DepthTestEnable;
    depthStencilState.depthWriteEnable = desc.DepthStencilState.DepthWriteEnable;
    depthStencilState.depthCompareOp = VulkanCompareOp(desc.DepthStencilState.DepthCompareOp);
    depthStencilState.depthBoundsTestEnable = desc.DepthStencilState.DepthBoundsTestEnable;
    depthStencilState.stencilTestEnable = desc.DepthStencilState.StencilTestEnable;
    depthStencilState.front = VulkanStencilOpState(desc.DepthStencilState.FrontStencil);
    depthStencilState.back = VulkanStencilOpState(desc.DepthStencilState.BackStencil);

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;
    
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (desc.CachedPipelineData && desc.CachedPipelineDataSize > 0)
    {
        cacheInfo.initialDataSize = desc.CachedPipelineDataSize;
        cacheInfo.pInitialData = desc.CachedPipelineData;
    }
    
    std::vector<ResourceLayout> resourceLayouts;
    
    if (desc.UseOwnResourceLayout)
        resourceLayouts.push_back(desc.ResourceLayout);
    
    if (inputIOResources && !desc.IsVariant)
        for (IOResource* ioResource : *inputIOResources)
            resourceLayouts.push_back(ioResource->Layout);

    for (uint32_t i = desc.UseOwnResourceLayout ? 1 : 0; i < resourceLayouts.size(); i++)
        for (auto& binding : resourceLayouts[i].Bindings)
            if (binding.Set < RHIConstants::VARIANT_DESCRIPTOR_SET_BASE)
                binding.Set = desc.UseOwnResourceLayout ? i + 1 : i;

    if (desc.IsVariant)
    {
        ResourceLayout variantLayout = desc.VariantResourceLayout;
        for (auto& binding : variantLayout.Bindings)
            binding.Set = RHIConstants::VARIANT_DESCRIPTOR_SET_BASE;
        resourceLayouts.push_back(variantLayout);
    }

    PipelineLayout = VulkanPipelineLayoutBuilder::BuildPipelineLayout(desc.PipelineID, resourceLayouts, SetLayouts, desc.Constants, SetIndexToBuilderIndex);
    
    if (inputIOResources && !desc.IsVariant)
    {
        for (uint32_t i = 0; i < inputIOResources->size(); i++)
        {
            IOResource* ioResource = inputIOResources->at(i);
            uint32_t setIndex = ioResource->Layout.Bindings.empty() ? 0 : ioResource->Layout.Bindings[0].Set;

            uint64_t descriptorSetID = BufferAllocator::GetInstance()->AllocateDescriptorSet(
                desc.PipelineID, setIndex, ioResource->Bindings);
            PipelineInputDescriptorSetIDs.push_back(descriptorSetID);
            InputIOResources.push_back(ioResource);
        }
    }
    
    std::vector<VkFormat> colorFormats;
    for (const auto& format : desc.RenderTargetFormats)
        colorFormats.push_back(VulkanFormat(format));
    
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingCreateInfo.viewMask = desc.ViewMask;
    pipelineRenderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(desc.RenderTargetFormats.size());
    pipelineRenderingCreateInfo.pColorAttachmentFormats = colorFormats.empty() ? nullptr : colorFormats.data();
    pipelineRenderingCreateInfo.depthAttachmentFormat = VulkanFormat(desc.DepthStencilFormat);
    pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED; 
    pipelineRenderingCreateInfo.pNext = nullptr;
    
    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineCreateInfo.pStages = shaderStages.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pTessellationState = tessellationEnabled ? &tessellation : nullptr;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisampling;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.flags = desc.UseDescriptorBuffer ? VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT : 0;
    pipelineCreateInfo.renderPass = VK_NULL_HANDLE;
    pipelineCreateInfo.layout = PipelineLayout;
    pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
    
    VkPipelineCacheCreateInfo cacheCreateInfo{};
    cacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    cacheCreateInfo.initialDataSize = 0;
    cacheCreateInfo.pInitialData = nullptr;
    cacheCreateInfo.flags = 0;
    cacheCreateInfo.pNext = nullptr;
    
    if (desc.CachedPipelineData && desc.CachedPipelineDataSize > 0)
    {
        cacheCreateInfo.initialDataSize = desc.CachedPipelineDataSize;
        cacheCreateInfo.pInitialData = desc.CachedPipelineData;
    }

    VkResult cacheResult = vkCreatePipelineCache(
        VulkanCore::Instance().GetDevice(),
        &cacheCreateInfo,
        nullptr,
        &PipelineCache
    );
    
    if (cacheResult != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline cache!");

    VkResult result = vkCreateGraphicsPipelines(VulkanCore::Instance().GetDevice(), PipelineCache, 1, &pipelineCreateInfo, nullptr, &Pipeline);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan graphics pipeline!");
    
    // Create attachment descriptions for dynamic rendering
    for (size_t i = 0; i < desc.RenderTargetFormats.size(); ++i)
    {
        VkAttachmentDescription attachment{};
        attachment.format = VulkanFormat(desc.RenderTargetFormats[i]);
        attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        attachment.loadOp = (i < desc.ColorLoadOps.size() && desc.ColorLoadOps[i] == AttachmentLoadOp::Clear) 
                           ? VK_ATTACHMENT_LOAD_OP_CLEAR 
                           : (i < desc.ColorLoadOps.size() && desc.ColorLoadOps[i] == AttachmentLoadOp::Load)
                           ? VK_ATTACHMENT_LOAD_OP_LOAD
                           : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.storeOp = (i < desc.ColorStoreOps.size() && desc.ColorStoreOps[i] == AttachmentStoreOp::Store)
                            ? VK_ATTACHMENT_STORE_OP_STORE
                            : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        AttachmentDescriptions.push_back(attachment);
    }
    
    // Depth attachment (if present)
    DepthAttachmentDescription = {};
    DepthAttachmentDescription.format = VK_FORMAT_UNDEFINED;
    DepthAttachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    DepthAttachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    if (desc.DepthStencilFormat != Format::Unknown)
    {
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = VulkanFormat(desc.DepthStencilFormat);
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = desc.DepthLoadOp == AttachmentLoadOp::Clear 
                                ? VK_ATTACHMENT_LOAD_OP_CLEAR 
                                : desc.DepthLoadOp == AttachmentLoadOp::Load
                                ? VK_ATTACHMENT_LOAD_OP_LOAD
                                : VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.storeOp = desc.DepthStoreOp == AttachmentStoreOp::Store
                                 ? VK_ATTACHMENT_STORE_OP_STORE
                                 : VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        DepthAttachmentDescription = depthAttachment;
    }
    
    // Depth buffer (if needed)
    DescriptorSetBinding depthBindingData{};
    if (desc.CreateDepthImage)
    {
        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = VulkanFormat(desc.DepthStencilFormat);
        depthImageInfo.extent.width = ViewportSize.x;
        depthImageInfo.extent.height = ViewportSize.y;
        depthImageInfo.extent.depth = 1;
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = desc.AttachmentArrayLayers;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        result = vkCreateImage(VulkanCore::Instance().GetDevice(), &depthImageInfo, nullptr, &OwnedDepthImage);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image for pipeline depth buffer!");
        
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(VulkanCore::Instance().GetDevice(), OwnedDepthImage, &memReqs);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = VulkanBufferAllocator::FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        result = vkAllocateMemory(VulkanCore::Instance().GetDevice(), &allocInfo, nullptr, &OwnedDepthImageMemory);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate Vulkan image memory for pipeline depth buffer!");
        
        result = vkBindImageMemory(VulkanCore::Instance().GetDevice(), OwnedDepthImage, OwnedDepthImageMemory, 0);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to bind Vulkan image memory for pipeline depth buffer!");
        
        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = OwnedDepthImage;
        imageViewInfo.format = VulkanFormat(desc.DepthStencilFormat);
        imageViewInfo.viewType = (desc.AttachmentArrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (desc.DepthStencilFormat == Format::D24_UNORM_S8_UINT || desc.DepthStencilFormat == Format::D32_FLOAT_S8X24_UINT)
            imageViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = desc.AttachmentArrayLayers;

        result = vkCreateImageView(VulkanCore::Instance().GetDevice(), &imageViewInfo, nullptr, &OwnedDepthImageView);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image view for pipeline depth buffer!");
        
        VulkanImageData* vulkanImageData = new VulkanImageData();
        vulkanImageData->ImageView = OwnedDepthImageView;
        vulkanImageData->ImageHandle = OwnedDepthImage;
        vulkanImageData->Memory = OwnedDepthImageMemory;
        
        DescriptorBinding binding{};
        binding.Type = DescriptorType::SampledImage;
        binding.Count = 1;
        binding.Set = desc.OutputDescriptorSetIndex;
        binding.Slot = static_cast<uint32_t>(OwnedImageViews.size());
        binding.Sampler = SamplerType::Linear;
        
        ImageAllocation allocation;
        allocation.Image = vulkanImageData;
        depthBindingData.Binding = binding.Slot;
        depthBindingData.ResourceID = BufferAllocator::GetInstance()->CacheImage(allocation);
        OwnedDepthImageResourceID = depthBindingData.ResourceID;
    }

    // Create pipeline local attachment images
    // For multipass rendering
    if (!desc.CreateOwnAttachments) return;
    
    PipelineOutputResource = new IOResource();
    
    OwnedImages.resize(desc.RenderTargetFormats.size());
    OwnedImageViews.resize(desc.RenderTargetFormats.size());
    OwnedImageMemory.resize(desc.RenderTargetFormats.size());
    
    PipelineOutputResource->Layout.VisibleStages.SetFragment(true);
    
    for (size_t i = 0; i < desc.RenderTargetFormats.size(); ++i)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VulkanFormat(desc.RenderTargetFormats[i]);
        imageInfo.extent.width = ViewportSize.x;
        imageInfo.extent.height = ViewportSize.y;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = desc.AttachmentArrayLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // Can be sampled by next pass
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        result = vkCreateImage(VulkanCore::Instance().GetDevice(), &imageInfo, nullptr, &OwnedImages[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image for pipeline render target!");
        
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(VulkanCore::Instance().GetDevice(), OwnedImages[i], &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = VulkanBufferAllocator::FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        result = vkAllocateMemory(VulkanCore::Instance().GetDevice(), &allocInfo, nullptr, &OwnedImageMemory[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate Vulkan image memory for pipeline render target!");
        
        result = vkBindImageMemory(VulkanCore::Instance().GetDevice(), OwnedImages[i], OwnedImageMemory[i], 0);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to bind Vulkan image memory for pipeline render target!");
        
        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = OwnedImages[i];
        imageViewInfo.format = VulkanFormat(desc.RenderTargetFormats[i]);
        imageViewInfo.viewType = (desc.AttachmentArrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = desc.AttachmentArrayLayers;

        result = vkCreateImageView(VulkanCore::Instance().GetDevice(), &imageViewInfo, nullptr, &OwnedImageViews[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image view for pipeline render target!");
        
        VulkanImageData* vulkanImageData = new VulkanImageData();
        vulkanImageData->ImageView = OwnedImageViews[i];
        vulkanImageData->ImageHandle = OwnedImages[i];
        vulkanImageData->Memory = OwnedImageMemory[i];
        
        DescriptorBinding binding{};
        binding.Type = DescriptorType::SampledImage;
        binding.Count = 1;
        binding.Set = desc.OutputDescriptorSetIndex;
        binding.Slot = static_cast<uint32_t>(i);
        binding.Sampler = desc.AttachmentSamplers[i];
        PipelineOutputResource->Layout.Bindings.push_back(binding);
        
        ImageAllocation allocation;
        allocation.Image = vulkanImageData;
        
        DescriptorSetBinding bindingData{};
        bindingData.Binding = binding.Slot;
        bindingData.ResourceID = BufferAllocator::GetInstance()->CacheImage(allocation);
        OwnedColorResourceIDs.push_back(bindingData.ResourceID);
        PipelineOutputResource->Bindings.push_back(bindingData);
    }

    if (desc.CreateDepthAttachment && desc.CreateDepthImage)
    {
        PipelineOutputResource->Bindings.push_back(depthBindingData);
    }
}

void VulkanPipeline::DestroyDepthImage()
{
    if (OwnedDepthImageResourceID != UINT64_MAX)
    {
        BufferAllocator::GetInstance()->EvictImage(OwnedDepthImageResourceID);
        OwnedDepthImageResourceID = UINT64_MAX;
    }

    VkDevice device = VulkanCore::Instance().GetDevice();

    if (OwnedDepthImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, OwnedDepthImageView, nullptr);
        OwnedDepthImageView = VK_NULL_HANDLE;
    }
    if (OwnedDepthImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, OwnedDepthImage, nullptr);
        OwnedDepthImage = VK_NULL_HANDLE;
    }
    if (OwnedDepthImageMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, OwnedDepthImageMemory, nullptr);
        OwnedDepthImageMemory = VK_NULL_HANDLE;
    }
}

void VulkanPipeline::ReallocDepthImage()
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    VkImageCreateInfo depthImageInfo{};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.format = VulkanFormat(DepthStencilFormat);
    depthImageInfo.extent = {ViewportSize.x, ViewportSize.y, 1};
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = ArrayLayerCount;
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkResult result = vkCreateImage(device, &depthImageInfo, nullptr, &OwnedDepthImage);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image on resize.");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device, OwnedDepthImage, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanBufferAllocator::FindMemoryType(
        memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = vkAllocateMemory(device, &allocInfo, nullptr, &OwnedDepthImageMemory);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate depth image memory on resize.");

    result = vkBindImageMemory(device, OwnedDepthImage, OwnedDepthImageMemory, 0);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to bind depth image memory on resize.");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = OwnedDepthImage;
    viewInfo.format = VulkanFormat(DepthStencilFormat);
    viewInfo.viewType = (ArrayLayerCount > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (DepthStencilFormat == Format::D24_UNORM_S8_UINT || DepthStencilFormat == Format::D32_FLOAT_S8X24_UINT)
        viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = ArrayLayerCount;

    result = vkCreateImageView(device, &viewInfo, nullptr, &OwnedDepthImageView);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image view on resize.");

    VulkanImageData* vulkanImageData = new VulkanImageData();
    vulkanImageData->ImageView = OwnedDepthImageView;
    vulkanImageData->ImageHandle = OwnedDepthImage;
    vulkanImageData->Memory = OwnedDepthImageMemory;

    ImageAllocation allocation;
    allocation.Image = vulkanImageData;
    OwnedDepthImageResourceID = BufferAllocator::GetInstance()->CacheImage(allocation);
}

void VulkanPipeline::RefreshInputDescriptorSets()
{
    if (InputIOResources.size() != PipelineInputDescriptorSetIDs.size())
        return;

    VulkanBufferAllocator* allocator = static_cast<VulkanBufferAllocator*>(BufferAllocator::GetInstance());
    for (uint32_t i = 0; i < PipelineInputDescriptorSetIDs.size(); i++)
        allocator->UpdateDescriptorSet(PipelineInputDescriptorSetIDs[i], InputIOResources[i]->Bindings);
}

VulkanPipeline::~VulkanPipeline()
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    DestroyColorAttachments();
    DestroyDepthImage();

    for (int i = 0; i < PipelineVariants.size(); i++)
        delete PipelineVariants[i];

    if (Pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, Pipeline, nullptr);
        
    if (PipelineCache != VK_NULL_HANDLE)
        vkDestroyPipelineCache(device, PipelineCache, nullptr);
        
    if (PipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, PipelineLayout, nullptr);

    for (VkShaderModule shaderModule : ShaderModules)
        vkDestroyShaderModule(device, shaderModule, nullptr);
}

void VulkanPipeline::RecreateAttachments(uint32_t width, uint32_t height)
{
    if (!AttachmentsAreViewportDims)
        return;

    ViewportSize = {width, height};

    if (!CreateOwnAttachments)
        return;

    DestroyColorAttachments();
    CreateColorAttachments(ArrayLayerCount);

    if (CreateDepthImage)
    {
        DestroyDepthImage();
        ReallocDepthImage();
    }
}

void VulkanPipeline::DestroyColorAttachments()
{
    VulkanBufferAllocator* allocator = static_cast<VulkanBufferAllocator*>(BufferAllocator::GetInstance());
    for (uint64_t id : OwnedColorResourceIDs)
        allocator->EvictImage(id);
    OwnedColorResourceIDs.clear();

    VkDevice device = VulkanCore::Instance().GetDevice();

    // Destroy old image views
    for (VkImageView view : OwnedImageViews)
    {
        if (view != VK_NULL_HANDLE)
            vkDestroyImageView(device, view, nullptr);
    }
    
    // Destroy old images
    for (VkImage image : OwnedImages)
    {
        if (image != VK_NULL_HANDLE)
            vkDestroyImage(device, image, nullptr);
    }
    
    // Free old memory
    for (VkDeviceMemory memory : OwnedImageMemory)
    {
        if (memory != VK_NULL_HANDLE)
            vkFreeMemory(device, memory, nullptr);
    }
    
    // Clear the vectors
    OwnedImageViews.clear();
    OwnedImages.clear();
    OwnedImageMemory.clear();
    
    // Clear bindings from output resource
    if (PipelineOutputResource)
    {
        PipelineOutputResource->Bindings.clear();
        PipelineOutputResource->Layout.Bindings.clear();
    }
}

void VulkanPipeline::CreateColorAttachments(uint32_t arrayLayers)
{
    VkResult result;
    
    if (!PipelineOutputResource)
        PipelineOutputResource = new IOResource();
    
    OwnedImages.resize(RenderTargetFormats.size());
    OwnedImageViews.resize(RenderTargetFormats.size());
    OwnedImageMemory.resize(RenderTargetFormats.size());
    
    PipelineOutputResource->Layout.VisibleStages.SetFragment(true);
    
    for (size_t i = 0; i < RenderTargetFormats.size(); ++i)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VulkanFormat(RenderTargetFormats[i]);
        imageInfo.extent.width = ViewportSize.x;
        imageInfo.extent.height = ViewportSize.y;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = arrayLayers;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        result = vkCreateImage(VulkanCore::Instance().GetDevice(), &imageInfo, nullptr, &OwnedImages[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image for pipeline render target!");
        
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(VulkanCore::Instance().GetDevice(), OwnedImages[i], &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = VulkanBufferAllocator::FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        result = vkAllocateMemory(VulkanCore::Instance().GetDevice(), &allocInfo, nullptr, &OwnedImageMemory[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate Vulkan image memory for pipeline render target!");
        
        result = vkBindImageMemory(VulkanCore::Instance().GetDevice(), OwnedImages[i], OwnedImageMemory[i], 0);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to bind Vulkan image memory for pipeline render target!");
        
        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = OwnedImages[i];
        imageViewInfo.format = VulkanFormat(RenderTargetFormats[i]);
        imageViewInfo.viewType = (arrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = arrayLayers;

        result = vkCreateImageView(VulkanCore::Instance().GetDevice(), &imageViewInfo, nullptr, &OwnedImageViews[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image view for pipeline render target!");
        
        VulkanImageData* vulkanImageData = new VulkanImageData();
        vulkanImageData->ImageView = OwnedImageViews[i];
        vulkanImageData->ImageHandle = OwnedImages[i];
        vulkanImageData->Memory = OwnedImageMemory[i];
        
        DescriptorBinding binding{};
        binding.Type = DescriptorType::SampledImage;
        binding.Count = 1;
        binding.Set = OutputDescriptorSetIndex;
        binding.Slot = static_cast<uint32_t>(i);
        binding.Sampler = AttachmentSamplers[i];
        PipelineOutputResource->Layout.Bindings.push_back(binding);
        
        ImageAllocation allocation;
        allocation.Image = vulkanImageData;
        
        DescriptorSetBinding bindingData{};
        bindingData.Binding = binding.Slot;
        bindingData.ResourceID = BufferAllocator::GetInstance()->CacheImage(allocation);
        OwnedColorResourceIDs.push_back(bindingData.ResourceID);
        PipelineOutputResource->Bindings.push_back(bindingData);
    }
}

