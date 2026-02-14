#include "Pipeline.h"
#include <stdexcept>
#include "BufferAllocator.h"
#include "../GraphicsSettings.h"
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

Pipeline* Pipeline::Create(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>*inputIOResources)
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        return new VulkanPipeline(pipelineID, desc, inputIOResources);
    else if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        return new D3DPipeline(pipelineID, desc, inputIOResources);
    else
        throw std::runtime_error("Invalid graphics API selected.");
    
}

D3DPipeline::D3DPipeline(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources)
{
    ComPtr<ID3D12Device> device = D3DCore::GetInstance().GetDevice();
    Topology = DXPrimitiveTopology(desc.PrimitiveTopology);
    
    std::vector<ResourceLayout> resourceLayouts;
    resourceLayouts.push_back(desc.ResourceLayout);
    if (inputIOResources)
        for (const IOResource& ioResource : *inputIOResources)
            resourceLayouts.push_back(ioResource.Layout);
    
    RootSignature = D3DRootSignatureBuilder::BuildRootSignature(pipelineID, resourceLayouts);
    
    if (resourceLayouts.size() > 1)
        for (uint32_t i = 1; i < resourceLayouts.size(); i++)
        {
            BufferAllocator::GetInstance()->AllocateDescriptorSet(pipelineID, i, inputIOResources->at(i - 1).Bindings);
            PipelineInputDescriptorSetIDs.push_back(BufferAllocator::GetInstance()->MakeKey(pipelineID, i));
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
        element.SemanticIndex = attr.Location;
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
        sampleDesc.Quality = D3DCore::GetInstance().GetMSAAQualityLevel(
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
    
    if (!desc.CreateOwnAttachments) return;
    PipelineOutputResource = new IOResource();
    
    BufferAllocator* alloc = BufferAllocator::GetInstance();
    
    OwnedRTVs.resize(desc.RenderTargetFormats.size());
    OwnedColorResources.resize(desc.RenderTargetFormats.size());
    
    for (size_t i = 0; i < desc.RenderTargetFormats.size(); i++)
    {
        DXGI_SAMPLE_DESC sampleDesc = {};
        if (desc.MultisampleState.SampleCount > 1)
        {
            sampleDesc.Count = desc.MultisampleState.SampleCount;
            sampleDesc.Quality = D3DCore::GetInstance().GetMSAAQualityLevel(
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
        resourceDesc.Width = desc.AttachmentWidth;
        resourceDesc.Height = desc.AttachmentHeight;
        resourceDesc.DepthOrArraySize = 1;
        resourceDesc.MipLevels = 1;
        resourceDesc.Format = DXFormat(desc.RenderTargetFormats[i]);
        resourceDesc.SampleDesc = sampleDesc;
        
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, 
            D3D12_RESOURCE_STATE_RENDER_TARGET, nullptr, IID_PPV_ARGS(&OwnedColorResources[i])) >> ERROR_HANDLER;

        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = DXFormat(desc.RenderTargetFormats[i]);
        rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;
        device->CreateRenderTargetView(OwnedColorResources[i].Get(), &rtvDesc, OwnedRTVs[i]);
    
        DescriptorBinding binding{};
        binding.Type = DescriptorType::SampledImage;
        binding.Count = 1;
        binding.Set = desc.OutputDescriptorSetIndex;
        binding.Slot = static_cast<uint32_t>(i);
        PipelineOutputResource->Layout.Bindings.push_back(binding);
    
        // Create shader resource view descriptor for sampling this render target
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXFormat(desc.RenderTargetFormats[i]);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        DX12ImageData* imageData = new DX12ImageData();
        imageData->Image = OwnedColorResources[i].Get();
        // If your DX12ImageData has an SRV field, you'd create/store the descriptor here
    
        ImageAllocation imageAllocation;
        imageAllocation.Image = imageData;
    
        DescriptorSetBinding bindingData{};
        bindingData.Binding = binding.Slot;
        bindingData.ResourceID = alloc->CacheImage(imageAllocation);
        PipelineOutputResource->Bindings.push_back(bindingData);
    }
    
    if (desc.CreateDepthAttachment)
    {
        D3D12_RESOURCE_DESC depthResourceDesc = {};
        depthResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthResourceDesc.Alignment = 0;
        depthResourceDesc.Width = desc.AttachmentWidth;
        depthResourceDesc.Height = desc.AttachmentHeight;
        depthResourceDesc.DepthOrArraySize = 1;
        depthResourceDesc.MipLevels = 1;
        depthResourceDesc.Format = DXFormat(desc.DepthStencilFormat);
        depthResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depthResourceDesc.SampleDesc.Count = 1;
        depthResourceDesc.SampleDesc.Quality = 0;
        
        D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &depthResourceDesc, 
            D3D12_RESOURCE_STATE_DEPTH_WRITE, nullptr, IID_PPV_ARGS(&OwnedDepthResource)) >> ERROR_HANDLER;
        
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXFormat(desc.DepthStencilFormat);
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.Texture2D.MipSlice = 0;
        device->CreateDepthStencilView(OwnedDepthResource.Get(), &dsvDesc, OwnedDSV);
    
        DescriptorBinding binding{};
        binding.Type = DescriptorType::SampledImage;
        binding.Count = 1;
        binding.Set = desc.OutputDescriptorSetIndex;
        binding.Slot = static_cast<uint32_t>(desc.RenderTargetFormats.size());
        PipelineOutputResource->Layout.Bindings.push_back(binding);

        // Create shader resource view descriptor for sampling the depth buffer
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXFormat(desc.DepthStencilFormat);
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        DX12ImageData* depthImageData = new DX12ImageData();
        depthImageData->Image = OwnedDepthResource.Get();

        ImageAllocation depthAllocation;
        depthAllocation.Image = depthImageData;

        DescriptorSetBinding bindingData{};
        bindingData.Binding = binding.Slot;
        bindingData.ResourceID = alloc->CacheImage(depthAllocation);
        PipelineOutputResource->Bindings.push_back(bindingData);
    }
}

VulkanPipeline::VulkanPipeline(uint32_t pipelineID, const PipelineDesc& desc, std::vector<IOResource>* inputIOResources)
{
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
    rasterizer.frontFace = !desc.RasterizerState.FrontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE; // Inverting because of negative viewport height
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
    multisampling.minSampleShading = 0.5f;  // Sample shading affects the colour quality of MSAA samples.
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
    resourceLayouts.push_back(desc.ResourceLayout);
    if (inputIOResources)
        for (const IOResource& ioResource : *inputIOResources)
            resourceLayouts.push_back(ioResource.Layout);

    PipelineLayout = VulkanPipelineLayoutBuilder::BuildPipelineLayout(pipelineID, resourceLayouts, SetLayouts);
    
    if (resourceLayouts.size() > 1)
        for (uint32_t i = 1; i < resourceLayouts.size(); i++)
        {
            BufferAllocator::GetInstance()->AllocateDescriptorSet(pipelineID, i, inputIOResources->at(i - 1).Bindings);
            PipelineInputDescriptorSetIDs.push_back(BufferAllocator::GetInstance()->MakeKey(pipelineID, i));
        }
    
    std::vector<VkFormat> colorFormats;
    for (const auto& format : desc.RenderTargetFormats)
        colorFormats.push_back(VulkanFormat(format));
    
    VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo{};
    pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingCreateInfo.viewMask = 0;
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
    pipelineCreateInfo.flags = VK_PIPELINE_CREATE_DESCRIPTOR_BUFFER_BIT_EXT;
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
        VulkanCore::GetInstance().GetDevice(),
        &cacheCreateInfo,
        nullptr,
        &PipelineCache
    );
    
    if (cacheResult != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline cache!");

    VkResult result = vkCreateGraphicsPipelines(VulkanCore::GetInstance().GetDevice(), PipelineCache, 1, &pipelineCreateInfo, nullptr, &Pipeline);
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
    if (desc.DepthStencilFormat != Format::Unknown && desc.DepthStoreOp == AttachmentStoreOp::Store)
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
        
        AttachmentDescriptions.push_back(depthAttachment);
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
        imageInfo.extent.width = desc.AttachmentWidth;
        imageInfo.extent.height = desc.AttachmentHeight;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT; // Can be sampled by next pass
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        result = vkCreateImage(VulkanCore::GetInstance().GetDevice(), &imageInfo, nullptr, &OwnedImages[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image for pipeline render target!");
        
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(VulkanCore::GetInstance().GetDevice(), OwnedImages[i], &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = VulkanBufferAllocator::FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        result = vkAllocateMemory(VulkanCore::GetInstance().GetDevice(), &allocInfo, nullptr, &OwnedImageMemory[i]);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate Vulkan image memory for pipeline render target!");
        
        result = vkBindImageMemory(VulkanCore::GetInstance().GetDevice(), OwnedImages[i], OwnedImageMemory[i], 0);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to bind Vulkan image memory for pipeline render target!");
        
        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = OwnedImages[i];
        imageViewInfo.format = VulkanFormat(desc.RenderTargetFormats[i]);
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;

        result = vkCreateImageView(VulkanCore::GetInstance().GetDevice(), &imageViewInfo, nullptr, &OwnedImageViews[i]);
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
        PipelineOutputResource->Layout.Bindings.push_back(binding);
        
        ImageAllocation allocation;
        allocation.Image = vulkanImageData;
        
        DescriptorSetBinding bindingData{};
        bindingData.Binding = binding.Slot;
        bindingData.ResourceID = BufferAllocator::GetInstance()->CacheImage(allocation);
        PipelineOutputResource->Bindings.push_back(bindingData);
    }

    // Depth buffer (if needed)
    if (desc.CreateDepthAttachment)
    {
        VkImageCreateInfo depthImageInfo{};
        depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
        depthImageInfo.format = VulkanFormat(desc.DepthStencilFormat);
        depthImageInfo.extent.width = desc.AttachmentWidth;
        depthImageInfo.extent.height = desc.AttachmentHeight;
        depthImageInfo.extent.depth = 1;
        depthImageInfo.mipLevels = 1;
        depthImageInfo.arrayLayers = 1;
        depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        result = vkCreateImage(VulkanCore::GetInstance().GetDevice(), &depthImageInfo, nullptr, &OwnedDepthImage);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to create Vulkan image for pipeline depth buffer!");
        
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(VulkanCore::GetInstance().GetDevice(), OwnedDepthImage, &memReqs);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = VulkanBufferAllocator::FindMemoryType(
            memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        result = vkAllocateMemory(VulkanCore::GetInstance().GetDevice(), &allocInfo, nullptr, &OwnedDepthImageMemory);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate Vulkan image memory for pipeline depth buffer!");
        
        result = vkBindImageMemory(VulkanCore::GetInstance().GetDevice(), OwnedDepthImage, OwnedDepthImageMemory, 0);
        if (result != VK_SUCCESS)
            throw std::runtime_error("Failed to bind Vulkan image memory for pipeline depth buffer!");
        
        VkImageViewCreateInfo imageViewInfo{};
        imageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        imageViewInfo.image = OwnedDepthImage;
        imageViewInfo.format = VulkanFormat(desc.DepthStencilFormat);
        imageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (desc.DepthStencilFormat == Format::D24_UNORM_S8_UINT || desc.DepthStencilFormat == Format::D32_FLOAT_S8X24_UINT)
            imageViewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        imageViewInfo.subresourceRange.baseMipLevel = 0;
        imageViewInfo.subresourceRange.levelCount = 1;
        imageViewInfo.subresourceRange.baseArrayLayer = 0;
        imageViewInfo.subresourceRange.layerCount = 1;

        result = vkCreateImageView(VulkanCore::GetInstance().GetDevice(), &imageViewInfo, nullptr, &OwnedDepthImageView);
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
        PipelineOutputResource->Layout.Bindings.push_back(binding);
        
        ImageAllocation allocation;
        allocation.Image = vulkanImageData;
        
        DescriptorSetBinding bindingData{};
        bindingData.Binding = binding.Slot;
        bindingData.ResourceID = BufferAllocator::GetInstance()->CacheImage(allocation);
        PipelineOutputResource->Bindings.push_back(bindingData);
    }
}

VulkanPipeline::~VulkanPipeline()
{
    VkDevice device = VulkanCore::GetInstance().GetDevice();
    
    if (OwnedDepthImage) vkDestroyImage(device, OwnedDepthImage, nullptr);
    if (OwnedDepthImageMemory) vkFreeMemory(device, OwnedDepthImageMemory, nullptr);
    if (OwnedDepthImageView) vkDestroyImageView(device, OwnedDepthImageView, nullptr);

    for (auto& image : OwnedImages) 
        vkDestroyImage(device, OwnedImages[0], nullptr);
    for (auto& memory : OwnedImageMemory) 
        vkFreeMemory(device, memory, nullptr);
    for (auto& imageView : OwnedImageViews) 
        vkDestroyImageView(device, imageView, nullptr);
    
    for (VkDescriptorSetLayout setLayout : SetLayouts)
        vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
    
    if (Pipeline != VK_NULL_HANDLE)
        vkDestroyPipeline(device, Pipeline, nullptr);
        
    if (PipelineCache != VK_NULL_HANDLE)
        vkDestroyPipelineCache(device, PipelineCache, nullptr);
        
    if (RenderPass != VK_NULL_HANDLE)
        vkDestroyRenderPass(device, RenderPass, nullptr);
        
    if (PipelineLayout != VK_NULL_HANDLE)
        vkDestroyPipelineLayout(device, PipelineLayout, nullptr);

    for (VkShaderModule shaderModule : ShaderModules)
        vkDestroyShaderModule(device, shaderModule, nullptr);
}

