#include "Pipeline.h"

#include <stdexcept>
#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../Windows/Win32ErrorHandler.h"

using namespace Win32ErrorHandler;

Pipeline::Pipeline(const PipelineDesc& desc)
{
    if (desc.Layout)
        Layout = desc.Layout;
    else
    {
        Layout = PipelineLayout::Create(desc.ResourceLayout, desc.Constants);
        OwnsLayout = true;
    }
}

Pipeline::Pipeline(PipelineLayout* layout) : Layout(layout), IsComputePipeline(true)
{
    if (!layout)
        throw std::runtime_error("A compute pipeline needs a layout.");
}

Pipeline* Pipeline::CreateCompute(PipelineLayout* layout, const ShaderStage& computeShader)
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        return new VulkanPipeline(layout, computeShader);
    if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        return new D3DPipeline(layout, computeShader);
    throw std::runtime_error("Invalid graphics API selected.");
}

Pipeline::~Pipeline()
{
    if (OwnsLayout)
        delete Layout;
}

Pipeline* Pipeline::Create(const PipelineDesc& desc)
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        return new VulkanPipeline(desc);
    if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        return new D3DPipeline(desc);
    throw std::runtime_error("Invalid graphics API selected.");
}

//================================================//
// DirectX 12                                     //
//================================================//

D3DPipeline::D3DPipeline(const PipelineDesc& desc) : Pipeline(desc)
{
    Topology = DXPrimitiveTopology(desc.PrimitiveTopology);

    if (desc.BlendAttachmentStates.size() > 8 || desc.RenderTargetFormats.size() > 8)
        throw std::runtime_error("Too many render targets (max 8 for D3D12)");

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.AlphaToCoverageEnable = desc.MultisampleState.SampleCount > 1 && desc.MultisampleState.AlphaToCoverageEnable;
    blendDesc.IndependentBlendEnable = desc.BlendAttachmentStates.size() > 1;
    for (int i = 0; i < 8; i++)
    {
        D3D12_RENDER_TARGET_BLEND_DESC& target = blendDesc.RenderTarget[i];
        target.SrcBlend = D3D12_BLEND_ONE;
        target.DestBlend = D3D12_BLEND_ZERO;
        target.BlendOp = D3D12_BLEND_OP_ADD;
        target.SrcBlendAlpha = D3D12_BLEND_ONE;
        target.DestBlendAlpha = D3D12_BLEND_ZERO;
        target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    }
    for (size_t i = 0; i < desc.BlendAttachmentStates.size(); i++)
    {
        const BlendAttachmentState& state = desc.BlendAttachmentStates[i];
        D3D12_RENDER_TARGET_BLEND_DESC& target = blendDesc.RenderTarget[i];
        target.BlendEnable = state.BlendEnable;
        target.SrcBlend = DXBlendFactor(state.SrcColorBlendFactor);
        target.DestBlend = DXBlendFactor(state.DestColorBlendFactor);
        target.BlendOp = DXBlendOp(state.ColorBlendOp);
        target.SrcBlendAlpha = DXBlendFactor(state.SrcAlphaBlendFactor);
        target.DestBlendAlpha = DXBlendFactor(state.DestAlphaBlendFactor);
        target.BlendOpAlpha = DXBlendOp(state.AlphaBlendOp);
    }

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = DXFillMode(desc.RasterizerState.FillMode);
    rasterizerDesc.CullMode = DXCullMode(desc.RasterizerState.CullMode);
    rasterizerDesc.FrontCounterClockwise = desc.RasterizerState.FrontCounterClockwise;
    rasterizerDesc.DepthBias = static_cast<INT>(desc.RasterizerState.DepthBias);
    rasterizerDesc.SlopeScaledDepthBias = desc.RasterizerState.SlopeScaledDepthBias;
    rasterizerDesc.DepthBiasClamp = desc.RasterizerState.DepthBiasClamp;
    rasterizerDesc.DepthClipEnable = desc.RasterizerState.DepthClipEnable;
    rasterizerDesc.MultisampleEnable = desc.MultisampleState.SampleCount > 1;

    const DepthStencilState& depth = desc.DepthStencilState;
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = depth.DepthTestEnable;
    depthStencilDesc.DepthWriteMask = depth.DepthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
    depthStencilDesc.DepthFunc = DXCompareOp(depth.DepthCompareOp);
    depthStencilDesc.StencilEnable = depth.StencilTestEnable;
    depthStencilDesc.StencilReadMask = static_cast<UINT8>(depth.StencilReadMask);
    depthStencilDesc.StencilWriteMask = static_cast<UINT8>(depth.StencilWriteMask);
    depthStencilDesc.FrontFace = { DXStencilOp(depth.FrontStencil.FailOp), DXStencilOp(depth.FrontStencil.DepthFailOp),
                                   DXStencilOp(depth.FrontStencil.PassOp), DXCompareOp(depth.FrontStencil.CompareOp) };
    depthStencilDesc.BackFace = { DXStencilOp(depth.BackStencil.FailOp), DXStencilOp(depth.BackStencil.DepthFailOp),
                                  DXStencilOp(depth.BackStencil.PassOp), DXCompareOp(depth.BackStencil.CompareOp) };

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;
    for (const VertexAttribute& attribute : desc.VertexAttributes)
    {
        D3D12_INPUT_ELEMENT_DESC element = {};
        const char* semanticName = SemanticNameString(attribute.SemanticName);
        element.SemanticName = semanticName ? semanticName : "TEXCOORD";
        element.SemanticIndex = attribute.SemanticIndex;
        element.Format = DXFormat(attribute.Format);
        element.InputSlot = attribute.Binding;
        element.AlignedByteOffset = attribute.Offset;
        element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        for (const VertexBinding& binding : desc.VertexBindings)
            if (binding.Binding == attribute.Binding && binding.Instanced)
            {
                element.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                element.InstanceDataStepRate = 1;
            }
        inputElements.push_back(element);
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc = {};
    pipelineStateDesc.pRootSignature = static_cast<D3DPipelineLayout*>(Layout)->GetRootSignature();
    pipelineStateDesc.VS = DXShaderBytecode(desc.VertexShader);
    pipelineStateDesc.PS = DXShaderBytecode(desc.FragmentShader);
    pipelineStateDesc.DS = DXShaderBytecode(desc.DomainShader);
    pipelineStateDesc.HS = DXShaderBytecode(desc.HullShader);
    pipelineStateDesc.GS = DXShaderBytecode(desc.GeometryShader);
    pipelineStateDesc.BlendState = blendDesc;
    pipelineStateDesc.SampleMask = UINT_MAX;
    pipelineStateDesc.RasterizerState = rasterizerDesc;
    pipelineStateDesc.DepthStencilState = depthStencilDesc;
    pipelineStateDesc.InputLayout = { inputElements.empty() ? nullptr : inputElements.data(), static_cast<UINT>(inputElements.size()) };
    pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    pipelineStateDesc.PrimitiveTopologyType = DXPrimitiveTopologyType(desc.PrimitiveTopology);
    pipelineStateDesc.NumRenderTargets = static_cast<UINT>(desc.RenderTargetFormats.size());
    for (size_t i = 0; i < desc.RenderTargetFormats.size(); i++)
        pipelineStateDesc.RTVFormats[i] = DXFormat(desc.RenderTargetFormats[i]);
    pipelineStateDesc.DSVFormat = DXFormat(desc.DepthStencilFormat);
    pipelineStateDesc.SampleDesc.Count = (std::max)(desc.MultisampleState.SampleCount, 1u);
    if (desc.CachedPipelineData && desc.CachedPipelineDataSize > 0)
        pipelineStateDesc.CachedPSO = { desc.CachedPipelineData, desc.CachedPipelineDataSize };

    D3DCore::Instance().GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, IID_PPV_ARGS(&PipelineState)) >> ERROR_HANDLER;
}

D3DPipeline::D3DPipeline(PipelineLayout* layout, const ShaderStage& computeShader) : Pipeline(layout)
{
    Topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
    desc.pRootSignature = static_cast<D3DPipelineLayout*>(Layout)->GetRootSignature();
    desc.CS = DXShaderBytecode(computeShader);
    D3DCore::Instance().GetDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(&PipelineState)) >> ERROR_HANDLER;
}

//================================================//
// Vulkan                                         //
//================================================//

VulkanPipeline::VulkanPipeline(PipelineLayout* layout, const ShaderStage& computeShader) : Pipeline(layout)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = VulkanShaderModule(computeShader);
    pipelineInfo.stage.pName = computeShader.EntryPoint ? computeShader.EntryPoint : "main";
    pipelineInfo.layout = static_cast<VulkanPipelineLayout*>(Layout)->GetLayout();

    VkResult result = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &PipelineHandle);
    vkDestroyShaderModule(device, pipelineInfo.stage.module, nullptr);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan compute pipeline!");
}

VulkanPipeline::VulkanPipeline(const PipelineDesc& desc) : Pipeline(desc)
{
    VkDevice device = VulkanCore::Instance().GetDevice();

    struct StageSource
    {
        const ShaderStage* Stage;
        VkShaderStageFlagBits Flag;
    };
    StageSource sources[] = {
        { &desc.VertexShader, VK_SHADER_STAGE_VERTEX_BIT },
        { &desc.FragmentShader, VK_SHADER_STAGE_FRAGMENT_BIT },
        { &desc.GeometryShader, VK_SHADER_STAGE_GEOMETRY_BIT },
        { &desc.HullShader, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT },
        { &desc.DomainShader, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT },
    };

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    for (const StageSource& source : sources)
    {
        if (!source.Stage->ByteCode)
            continue;
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = source.Flag;
        stageInfo.module = VulkanShaderModule(*source.Stage);
        stageInfo.pName = source.Stage->EntryPoint ? source.Stage->EntryPoint : "main";
        shaderStages.push_back(stageInfo);
    }

    std::vector<VkVertexInputBindingDescription> bindingDescriptions;
    for (const VertexBinding& binding : desc.VertexBindings)
        bindingDescriptions.push_back({ binding.Binding, binding.Stride,
            binding.Instanced ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX });

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    for (const VertexAttribute& attribute : desc.VertexAttributes)
        attributeDescriptions.push_back({ attribute.Location, attribute.Binding, VulkanFormat(attribute.Format), attribute.Offset });

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VulkanPrimitiveTopology(desc.PrimitiveTopology);

    VkPipelineTessellationStateCreateInfo tessellation{};
    tessellation.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    tessellation.patchControlPoints = GetPatchControlPoints(desc.PrimitiveTopology);
    bool isTessellated = desc.HullShader.ByteCode && desc.DomainShader.ByteCode;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // The viewport has a negative height to match D3D12's orientation, which also flips the winding.
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = !desc.RasterizerState.DepthClipEnable;
    rasterizer.cullMode = VulkanCullMode(desc.RasterizerState.CullMode);
    rasterizer.frontFace = desc.RasterizerState.FrontCounterClockwise ? VK_FRONT_FACE_COUNTER_CLOCKWISE : VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = desc.RasterizerState.DepthBias != 0.0f;
    rasterizer.depthBiasConstantFactor = desc.RasterizerState.DepthBias;
    rasterizer.depthBiasSlopeFactor = desc.RasterizerState.SlopeScaledDepthBias;
    rasterizer.depthBiasClamp = desc.RasterizerState.DepthBiasClamp;
    rasterizer.polygonMode = VulkanFillMode(desc.RasterizerState.FillMode);
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = static_cast<VkSampleCountFlagBits>((std::max)(desc.MultisampleState.SampleCount, 1u));
    multisampling.alphaToCoverageEnable = desc.MultisampleState.SampleCount > 1 && desc.MultisampleState.AlphaToCoverageEnable;
    multisampling.minSampleShading = 1.0f;

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachments;
    for (const BlendAttachmentState& state : desc.BlendAttachmentStates)
    {
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.blendEnable = state.BlendEnable;
        attachment.colorBlendOp = VulkanBlendOp(state.ColorBlendOp);
        attachment.alphaBlendOp = VulkanBlendOp(state.AlphaBlendOp);
        attachment.srcColorBlendFactor = VulkanBlendFactor(state.SrcColorBlendFactor);
        attachment.dstColorBlendFactor = VulkanBlendFactor(state.DestColorBlendFactor);
        attachment.srcAlphaBlendFactor = VulkanBlendFactor(state.SrcAlphaBlendFactor);
        attachment.dstAlphaBlendFactor = VulkanBlendFactor(state.DestAlphaBlendFactor);
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blendAttachments.push_back(attachment);
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
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

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    std::vector<VkFormat> colorFormats;
    for (Format format : desc.RenderTargetFormats)
        colorFormats.push_back(VulkanFormat(format));

    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorFormats.size());
    renderingInfo.pColorAttachmentFormats = colorFormats.empty() ? nullptr : colorFormats.data();
    renderingInfo.depthAttachmentFormat = VulkanFormat(desc.DepthStencilFormat);

    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    if (desc.CachedPipelineData && desc.CachedPipelineDataSize > 0)
    {
        cacheInfo.initialDataSize = desc.CachedPipelineDataSize;
        cacheInfo.pInitialData = desc.CachedPipelineData;
    }
    if (vkCreatePipelineCache(device, &cacheInfo, nullptr, &PipelineCache) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline cache!");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pTessellationState = isTessellated ? &tessellation : nullptr;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencilState;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = static_cast<VulkanPipelineLayout*>(Layout)->GetLayout();

    VkResult result = vkCreateGraphicsPipelines(device, PipelineCache, 1, &pipelineInfo, nullptr, &PipelineHandle);

    for (const VkPipelineShaderStageCreateInfo& stage : shaderStages)
        vkDestroyShaderModule(device, stage.module, nullptr);

    if (result != VK_SUCCESS)
        throw std::runtime_error("Failed to create Vulkan graphics pipeline!");
}

VulkanPipeline::~VulkanPipeline()
{
    VkDevice device = VulkanCore::Instance().GetDevice();
    vkDestroyPipeline(device, PipelineHandle, nullptr);
    vkDestroyPipelineCache(device, PipelineCache, nullptr);
}
