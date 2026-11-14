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
    uint32_t ViewMask = 0;

    explicit Pipeline(const PipelineDesc& desc);

public:
    static Pipeline* Create(const PipelineDesc& desc);
    virtual ~Pipeline();

    PipelineLayout* GetLayout() const { return Layout; }
    uint32_t GetViewMask() const { return ViewMask; }
};

class D3DPipeline : public Pipeline
{
    D3D12_PRIMITIVE_TOPOLOGY Topology;
    ComPtr<ID3D12PipelineState> PipelineState;

public:
    explicit D3DPipeline(const PipelineDesc& desc);

    ID3D12PipelineState* GetPipelineState() const { return PipelineState.Get(); }
    D3D12_PRIMITIVE_TOPOLOGY GetTopology() const { return Topology; }
};

class VulkanPipeline : public Pipeline
{
    VkPipeline PipelineHandle = VK_NULL_HANDLE;
    VkPipelineCache PipelineCache = VK_NULL_HANDLE;

public:
    explicit VulkanPipeline(const PipelineDesc& desc);
    ~VulkanPipeline() override;

    VkPipeline GetVulkanPipeline() const { return PipelineHandle; }
};
