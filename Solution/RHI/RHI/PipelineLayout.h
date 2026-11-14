#pragma once
#include <map>
#include "RHIStructures.h"
#include "../RHI_API_Macro.h"

using Microsoft::WRL::ComPtr;

// The binding interface of one or more pipelines: descriptor sets and push constants. A root
// signature on D3D12, a VkPipelineLayout on Vulkan. Set 0 is reserved for the two global samplers
// (s0 linear, s1 point). Pipelines that share a layout share descriptor sets as well.
class RHI_API PipelineLayout
{
protected:
    uint32_t ID = 0;
    uint32_t PushConstantSize = 0;
    static uint32_t NextID;

    PipelineLayout();

public:
    static PipelineLayout* Create(const RHIStructures::ResourceLayout& resources, const std::vector<RHIStructures::PipelineConstant>& constants);
    virtual ~PipelineLayout();

    uint32_t GetID() const { return ID; }
    uint32_t GetPushConstantSize() const { return PushConstantSize; }
};

class VulkanPipelineLayout : public PipelineLayout
{
    VkPipelineLayout Layout = VK_NULL_HANDLE;
    VkShaderStageFlags PushConstantStages = 0;

public:
    VulkanPipelineLayout(const RHIStructures::ResourceLayout& resources, const std::vector<RHIStructures::PipelineConstant>& constants);
    ~VulkanPipelineLayout() override;

    VkPipelineLayout GetLayout() const { return Layout; }
    VkShaderStageFlags GetPushConstantStages() const { return PushConstantStages; }
};

class D3DPipelineLayout : public PipelineLayout
{
public:
    // Root parameters of one descriptor set: at most one table, plus one root CBV per dynamic
    // uniform buffer in slot order.
    struct SetParameters
    {
        int32_t TableParameter = -1;
        std::vector<uint32_t> DynamicParameters;
    };

private:
    ComPtr<ID3D12RootSignature> RootSignature;
    std::map<uint32_t, SetParameters> Sets;
    int32_t ConstantsParameter = -1;

public:
    D3DPipelineLayout(const RHIStructures::ResourceLayout& resources, const std::vector<RHIStructures::PipelineConstant>& constants);

    ID3D12RootSignature* GetRootSignature() const { return RootSignature.Get(); }
    const SetParameters* FindSet(uint32_t setIndex) const;
    int32_t GetConstantsParameter() const { return ConstantsParameter; }
};
