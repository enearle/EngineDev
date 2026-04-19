#include "../../Common/RHI/BufferAllocator.h"
#include "D3DRootSignatureBuilder.h"

#include <algorithm>

#include "../Windows/Win32ErrorHandler.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include <iostream>

#include "D3DCore.h"

using namespace Win32ErrorHandler;

ComPtr<ID3D12RootSignature> D3DRootSignatureBuilder::BuildRootSignature(uint32_t pipelineID, const std::vector<ResourceLayout>& layouts, const std::vector<PipelineConstant>& constants)
{
    ComPtr<ID3D12Device> device = D3DCore::Instance().GetDevice();
    if (!device)
        throw std::runtime_error("D3D12 device is null");
    
    std::vector<RootParameter> rootParameters;
    CreateRootParameters(pipelineID, layouts, constants, rootParameters);
    
    std::vector<D3D12_ROOT_PARAMETER> parameters;
    parameters.reserve(rootParameters.size());
    for (auto& rp : rootParameters)
    {
        parameters.push_back(rp.parameter);
    }
    
    D3D12_STATIC_SAMPLER_DESC samplers[2] = {};
    samplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplers[0].MipLODBias = 0.0f;
    samplers[0].MaxAnisotropy = 1;
    samplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplers[0].MinLOD = 0.0f;
    samplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    samplers[0].ShaderRegister = 0;
    samplers[0].RegisterSpace = 0;
    samplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    
    samplers[1] = samplers[0];
    samplers[1].Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    samplers[1].ShaderRegister = 1;
    
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = static_cast<UINT>(parameters.size());
    rootSigDesc.pParameters = parameters.data();
    rootSigDesc.NumStaticSamplers = 2;
    rootSigDesc.pStaticSamplers = samplers;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(),
        errorBlob.GetAddressOf()
    ) >> ERROR_HANDLER;
    
    ComPtr<ID3D12RootSignature> rootSignature;
    device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(rootSignature.GetAddressOf())
    ) >> ERROR_HANDLER;

    return rootSignature;
}

void D3DRootSignatureBuilder::CreateRootParameters(uint32_t pipelineID, const std::vector<ResourceLayout>& layouts,
    const std::vector<PipelineConstant>& constants, std::vector<RootParameter>& outRootParameters)
{
    // Push constants always use space0 if present
    uint32_t spaceOffset = 0;
    
    std::vector<D3D12_ROOT_PARAMETER> constantRanges;
    uint32_t constantSlot = 0;
    for (const PipelineConstant& constant : constants)
    {
        D3D12_ROOT_PARAMETER parameter = {};
        parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        parameter.Constants.ShaderRegister = constantSlot++;
        parameter.Constants.RegisterSpace = 0;
        parameter.Constants.Num32BitValues = constant.Size / 4;
        parameter.ShaderVisibility = DXShaderStageFlags(constant.VisibleStages);
        constantRanges.push_back(parameter);
    }
    outRootParameters.insert(outRootParameters.end(), constantRanges.begin(), constantRanges.end());
    
    // If we have push constants, offset descriptor sets by 1
    if (!constants.empty())
        spaceOffset = 1;
    
    for (uint32_t i = 0; i < layouts.size(); i++)
    {
        const ResourceLayout& layout = layouts[i];
        
        BufferAllocator::GetInstance()->RegisterDescriptorSetLayout(pipelineID, layout);
                
        std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE> uavRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE> cbvRanges;
        
        D3D12_SHADER_VISIBILITY visibility = DXShaderStageFlags(layout.VisibleStages);
        
        for (const DescriptorBinding& binding : layout.Bindings)
        {
            if (binding.Type == DescriptorType::UniformBuffer)
            {
                RootParameter parameter = {};
                parameter.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                parameter.parameter.Descriptor.ShaderRegister = binding.Slot;
                if (binding.Set >= 16)
                    parameter.parameter.Descriptor.RegisterSpace = binding.Set;
                else
                    parameter.parameter.Descriptor.RegisterSpace = binding.Set + spaceOffset;
                parameter.parameter.ShaderVisibility = visibility;
                
                outRootParameters.push_back(parameter);
            }
            else if (binding.Type == DescriptorType::DynamicUniformBuffer)
            {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                range.NumDescriptors = binding.Count;
                range.BaseShaderRegister = binding.Slot;
                range.RegisterSpace = binding.Set + spaceOffset;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                
                cbvRanges.push_back(range);
            }
            else if (binding.Type == DescriptorType::StorageBuffer || 
                     binding.Type == DescriptorType::SampledImage)
            {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.NumDescriptors = binding.Count;
                range.BaseShaderRegister = binding.Slot;
                range.RegisterSpace = binding.Set + spaceOffset;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                
                srvRanges.push_back(range);
            }
            else if (binding.Type == DescriptorType::StorageImage)
            {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                range.NumDescriptors = binding.Count;
                range.BaseShaderRegister = binding.Slot;
                range.RegisterSpace = binding.Set + spaceOffset;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                
                uavRanges.push_back(range);
            }
        }
        
        if (!srvRanges.empty())
        {
            std::sort(srvRanges.begin(), srvRanges.end(), 
                [](const D3D12_DESCRIPTOR_RANGE& a, const D3D12_DESCRIPTOR_RANGE& b) {
                    return a.BaseShaderRegister < b.BaseShaderRegister;
                });
            
            std::vector<D3D12_DESCRIPTOR_RANGE> consolidatedRanges;
            for (size_t i = 0; i < srvRanges.size(); ) {
                D3D12_DESCRIPTOR_RANGE range = srvRanges[i];
                size_t j = i + 1;
                
                while (j < srvRanges.size() && 
                       srvRanges[j].BaseShaderRegister == range.BaseShaderRegister + range.NumDescriptors) {
                    range.NumDescriptors += srvRanges[j].NumDescriptors;
                    j++;
                       }
                
                range.OffsetInDescriptorsFromTableStart = 
                    consolidatedRanges.empty() ? 0 : D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
        
                consolidatedRanges.push_back(range);
                i = j;
            }
    
            srvRanges = consolidatedRanges;
        }
        
        if (!cbvRanges.empty())
        {
            RootParameter parameter = {};
            parameter.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.parameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(cbvRanges.size());
            parameter.parameter.ShaderVisibility = visibility;
            parameter.ranges = cbvRanges;
            outRootParameters.push_back(parameter);
        }
        
        if (!srvRanges.empty())
        {
            RootParameter parameter = {};
            parameter.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.parameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(srvRanges.size());
            parameter.parameter.ShaderVisibility = visibility;
            parameter.ranges = srvRanges;
            outRootParameters.push_back(parameter);
        }
        
        if (!uavRanges.empty())
        {
            RootParameter parameter = {};
            parameter.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            parameter.parameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(uavRanges.size());
            parameter.parameter.ShaderVisibility = visibility;
            parameter.ranges = uavRanges;
            outRootParameters.push_back(parameter);
        }
    }

    // Fix up pointers after all parameters are created
    // Solves issue with std::vector reallocation
    for (RootParameter& rootParameter : outRootParameters)
    {
        if (rootParameter.parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            rootParameter.parameter.DescriptorTable.pDescriptorRanges = rootParameter.ranges.data();
        }
    }
}

