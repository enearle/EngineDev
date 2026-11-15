#pragma once
#include "RHIStructures.h"
#include "PipelineLayout.h"
#include "../RHI_API_Macro.h"

using namespace RHIStructures;
using Microsoft::WRL::ComPtr;

// Pipeline state only. Attachments belong to whoever renders (the pass graph); the pipeline keeps
// just the formats its state was built for.
class RHI_API Pipeline
{
protected:
    PipelineLayout* Layout = nullptr;
    bool OwnsLayout = false;
    bool IsComputePipeline = false;

    explicit Pipeline(const PipelineDesc& desc);
    explicit Pipeline(PipelineLayout* layout);

public:
    static Pipeline* Create(const PipelineDesc& desc);
    // The layout is shared, not owned, and must include the compute stage in its visible stages.
    static Pipeline* CreateCompute(PipelineLayout* layout, const ShaderStage& computeShader);
    virtual ~Pipeline();

    PipelineLayout* GetLayout() const { return Layout; }
    bool IsCompute() const { return IsComputePipeline; }
};

class D3DPipeline : public Pipeline
{
    D3D12_PRIMITIVE_TOPOLOGY Topology;
    ComPtr<ID3D12PipelineState> PipelineState;

public:
    explicit D3DPipeline(const PipelineDesc& desc);
    D3DPipeline(PipelineLayout* layout, const ShaderStage& computeShader);

    ID3D12PipelineState* GetPipelineState() const { return PipelineState.Get(); }
    D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }
};

class VulkanPipeline : public Pipeline
{
    VkPipeline PipelineHandle = VK_NULL_HANDLE;
    VkPipelineCache PipelineCache = VK_NULL_HANDLE;

public:
    explicit VulkanPipeline(const PipelineDesc& desc);
    VulkanPipeline(PipelineLayout* layout, const ShaderStage& computeShader);
    ~VulkanPipeline() override;

    VkPipeline GetVulkanPipeline() const { return PipelineHandle; }
};
