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

class D3DPipeline : public Pipeline
{
public:
    D3DPipeline(const PipelineDesc& desc);
    
    ID3D12PipelineState* GetPipelineState() const { return PipelineState.Get(); }
    ID3D12RootSignature* GetRootSignature() const { return RootSignature.Get(); }
    
private:
    ComPtr<ID3D12RootSignature> RootSignature;
    ComPtr<ID3D12PipelineState> PipelineState;
};

class VulkanPipeline : public Pipeline
{
public:
    VulkanPipeline(const PipelineDesc& desc);
    ~VulkanPipeline();
    
    VkPipeline GetVulkanPipeline() const { return Pipeline; }
    VkPipelineLayout GetPipelineLayout() const { return PipelineLayout; }
    VkRenderPass GetRenderPass() const { return RenderPass; }  // <-- Baked in!
    
private:
    void CreateRenderPass(const PipelineDesc& desc);
    
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkPipelineCache PipelineCache = VK_NULL_HANDLE;
    VkRenderPass RenderPass = VK_NULL_HANDLE;  // <-- Stored here
};

