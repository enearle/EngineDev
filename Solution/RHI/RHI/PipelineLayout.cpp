#include "PipelineLayout.h"

#include <algorithm>
#include <stdexcept>
#include <d3dcompiler.h>
#include "BufferAllocator.h"
#include "../GraphicsSettings.h"
#include "../DirectX12/D3DCore.h"
#include "../Vulkan/VulkanCore.h"
#include "../Windows/Win32ErrorHandler.h"

using namespace RHIStructures;
using namespace Win32ErrorHandler;

uint32_t PipelineLayout::NextID = 1;

PipelineLayout::PipelineLayout() : ID(NextID++)
{
}

PipelineLayout::~PipelineLayout()
{
    BufferAllocator::GetInstance()->RetireDescriptorSetLayouts(ID);
}

PipelineLayout* PipelineLayout::Create(const ResourceLayout& resources, const std::vector<PipelineConstant>& constants)
{
    if (GRAPHICS_SETTINGS.APIToUse == Vulkan)
        return new VulkanPipelineLayout(resources, constants);
    if (GRAPHICS_SETTINGS.APIToUse == DirectX12)
        return new D3DPipelineLayout(resources, constants);
    throw std::runtime_error("Invalid graphics API selected.");
}

static uint32_t MaxSetIndex(const ResourceLayout& resources)
{
    uint32_t maxSet = 0;
    for (const DescriptorBinding& binding : resources.Bindings)
        maxSet = (std::max)(maxSet, binding.Set);
    return maxSet;
}

//================================================//
// Vulkan                                         //
//================================================//

VulkanPipelineLayout::VulkanPipelineLayout(const ResourceLayout& resources, const std::vector<PipelineConstant>& constants)
{
    VulkanBufferAllocator* allocator = static_cast<VulkanBufferAllocator*>(BufferAllocator::GetInstance());
    uint32_t maxSet = MaxSetIndex(resources);
    allocator->RegisterDescriptorSetLayout(ID, resources, maxSet);

    std::vector<VkDescriptorSetLayout> setLayouts;
    setLayouts.push_back(VulkanCore::Instance().GetGlobalSamplerSetLayout());
    for (uint32_t setIndex = 1; setIndex <= maxSet; setIndex++)
        setLayouts.push_back(allocator->GetRegisteredDescriptorSetLayout(ID, setIndex));

    std::vector<VkPushConstantRange> ranges;
    uint32_t offset = 0;
    for (const PipelineConstant& constant : constants)
    {
        VkPushConstantRange range{};
        range.stageFlags = VulkanShaderStageFlags(constant.VisibleStages);
        range.offset = offset;
        range.size = static_cast<uint32_t>(constant.Size);
        ranges.push_back(range);
        PushConstantStages |= range.stageFlags;
        offset += range.size;
    }
    PushConstantSize = offset;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = static_cast<uint32_t>(setLayouts.size());
    layoutInfo.pSetLayouts = setLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(ranges.size());
    layoutInfo.pPushConstantRanges = ranges.empty() ? nullptr : ranges.data();

    if (vkCreatePipelineLayout(VulkanCore::Instance().GetDevice(), &layoutInfo, nullptr, &Layout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create pipeline layout");
}

VulkanPipelineLayout::~VulkanPipelineLayout()
{
    vkDestroyPipelineLayout(VulkanCore::Instance().GetDevice(), Layout, nullptr);
}

//================================================//
// DirectX 12                                     //
//================================================//

D3DPipelineLayout::D3DPipelineLayout(const ResourceLayout& resources, const std::vector<PipelineConstant>& constants)
{
    BufferAllocator::GetInstance()->RegisterDescriptorSetLayout(ID, resources, MaxSetIndex(resources));

    D3D12_SHADER_VISIBILITY visibility = DXShaderStageFlags(resources.VisibleStages);
    std::vector<D3D12_ROOT_PARAMETER> parameters;
    std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> tableRanges;

    uint32_t constantRegister = 0;
    for (const PipelineConstant& constant : constants)
    {
        D3D12_ROOT_PARAMETER parameter = {};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameter.Constants.ShaderRegister = constantRegister++;
        parameter.Constants.RegisterSpace = 999;
        parameter.Constants.Num32BitValues = static_cast<UINT>(constant.Size / 4);
        parameter.ShaderVisibility = DXShaderStageFlags(constant.VisibleStages);
        if (ConstantsParameter < 0)
            ConstantsParameter = static_cast<int32_t>(parameters.size());
        parameters.push_back(parameter);
        PushConstantSize += static_cast<uint32_t>(constant.Size);
    }

    std::map<uint32_t, std::vector<DescriptorBinding>> bindingsBySet;
    for (const DescriptorBinding& binding : resources.Bindings)
        bindingsBySet[binding.Set].push_back(binding);

    for (const auto& [setIndex, bindings] : bindingsBySet)
    {
        DirectX12BufferAllocator::SetLayoutInfo info = DirectX12BufferAllocator::BuildSetLayoutInfo(bindings);
        SetParameters& set = Sets[setIndex];

        std::vector<D3D12_DESCRIPTOR_RANGE> ranges;
        for (size_t i = 0; i < info.Bindings.size(); i++)
        {
            const DescriptorBinding& binding = info.Bindings[i];
            if (binding.Type == DescriptorType::DynamicUniformBuffer)
            {
                D3D12_ROOT_PARAMETER parameter = {};
                parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                parameter.Descriptor.ShaderRegister = binding.Slot;
                parameter.Descriptor.RegisterSpace = setIndex;
                parameter.ShaderVisibility = visibility;
                set.DynamicParameters.push_back(static_cast<uint32_t>(parameters.size()));
                parameters.push_back(parameter);
                continue;
            }

            D3D12_DESCRIPTOR_RANGE range{};
            switch (binding.Type)
            {
            case DescriptorType::UniformBuffer: range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
            case DescriptorType::StorageImage:  range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
            default:                            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
            }
            range.NumDescriptors = (std::max)(binding.Count, 1u);
            range.BaseShaderRegister = binding.Slot;
            range.RegisterSpace = setIndex;
            range.OffsetInDescriptorsFromTableStart = info.TableOffsets[i];
            ranges.push_back(range);
        }

        if (!ranges.empty())
        {
            D3D12_ROOT_PARAMETER parameter = {};
            parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
            parameter.ShaderVisibility = visibility;
            set.TableParameter = static_cast<int32_t>(parameters.size());
            parameters.push_back(parameter);
            tableRanges.push_back(std::move(ranges));
        }
    }

    size_t tableIndex = 0;
    for (D3D12_ROOT_PARAMETER& parameter : parameters)
        if (parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
            parameter.DescriptorTable.pDescriptorRanges = tableRanges[tableIndex++].data();

    D3D12_STATIC_SAMPLER_DESC samplers[GlobalSamplerCount] = {};
    for (uint32_t i = 0; i < GlobalSamplerCount; i++)
    {
        const SamplerDesc& desc = GlobalSamplers[i];
        D3D12_TEXTURE_ADDRESS_MODE address = desc.Address == SamplerAddress::Repeat
            ? D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        if (desc.MaxAnisotropy > 1)
            samplers[i].Filter = D3D12_FILTER_ANISOTROPIC;
        else
            samplers[i].Filter = desc.Filter == SamplerFilter::Linear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
        samplers[i].AddressU = address;
        samplers[i].AddressV = address;
        samplers[i].AddressW = address;
        samplers[i].MaxAnisotropy = desc.MaxAnisotropy;
        samplers[i].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        samplers[i].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
        samplers[i].MaxLOD = D3D12_FLOAT32_MAX;
        samplers[i].ShaderRegister = i;
        samplers[i].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
    rootSignatureDesc.NumParameters = static_cast<UINT>(parameters.size());
    rootSignatureDesc.pParameters = parameters.empty() ? nullptr : parameters.data();
    rootSignatureDesc.NumStaticSamplers = GlobalSamplerCount;
    rootSignatureDesc.pStaticSamplers = samplers;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> serialized;
    ComPtr<ID3DBlob> errors;
    HRESULT result = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &errors);
    if (FAILED(result))
        throw std::runtime_error(errors ? static_cast<const char*>(errors->GetBufferPointer()) : "Failed to serialize root signature");

    D3DCore::Instance().GetDevice()->CreateRootSignature(0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(RootSignature.GetAddressOf())) >> ERROR_HANDLER;
}

const D3DPipelineLayout::SetParameters* D3DPipelineLayout::FindSet(uint32_t setIndex) const
{
    auto found = Sets.find(setIndex);
    return found == Sets.end() ? nullptr : &found->second;
}
