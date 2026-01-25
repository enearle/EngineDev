#pragma once
#include "RHIStructures.h"

using namespace RHIStructures;

using Microsoft::WRL::ComPtr;

class Pipeline
{
public:
    static Pipeline* Create(const PipelineDesc& desc);
    virtual ~Pipeline() = default;
};

// Platform-specific implementations
class D3DPipeline : public Pipeline
{
public:
    D3DPipeline(const PipelineDesc& desc);
    
private:
    ComPtr<ID3D12PipelineState> PipelineState;
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveTopology;
};

class VulkanPipeline : public Pipeline
{
public:
    VulkanPipeline(const PipelineDesc& desc);
    
private:
    VkPipeline Pipeline;
    VkPipelineLayout PipelineLayout;
};
