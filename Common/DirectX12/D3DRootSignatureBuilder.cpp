#include "../../Common/RHI/BufferAllocator.h"
#include "D3DRootSignatureBuilder.h"
#include "../Windows/Win32ErrorHandler.h"
#include <stdexcept>
#include <d3dcompiler.h>
#include "D3DCore.h"

using namespace Win32ErrorHandler;

ComPtr<ID3D12RootSignature> D3DRootSignatureBuilder::BuildRootSignature(uint32_t pipelineID, const std::vector<ResourceLayout>& layouts)
{
    ComPtr<ID3D12Device> device = D3DCore::GetInstance().GetDevice();
    if (!device)
        throw std::runtime_error("D3D12 device is null");
    
    std::vector<RootParameter> rootParameters;
    CreateRootParameters(pipelineID, layouts, rootParameters);
    
    std::vector<D3D12_ROOT_PARAMETER> parameters;
    parameters.reserve(rootParameters.size());
    for (auto& rp : rootParameters)
    {
        parameters.push_back(rp.parameter);
    }
    
    D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
    samplerDesc.MinLOD = 0.0f;
    
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = static_cast<UINT>(parameters.size());
    rootSigDesc.pParameters = parameters.data();
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &samplerDesc;
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
    std::vector<RootParameter>& outRootParameters)
{
    
    for (uint32_t i = 0; i < layouts.size(); i++)
    {
        const ResourceLayout& layout = layouts[i];
        
        BufferAllocator::GetInstance()->RegisterDescriptorSetLayout(pipelineID, layout);
                
        std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;
        std::vector<D3D12_DESCRIPTOR_RANGE> uavRanges;
        
        D3D12_SHADER_VISIBILITY visibility = DXShaderStageFlags(layout.VisibleStages);
        
        for (const DescriptorBinding& binding : layout.Bindings)
        {
            if (binding.Type == DescriptorType::UniformBuffer)
            {
                RootParameter parameter = {};
                parameter.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
                parameter.parameter.Descriptor.ShaderRegister = binding.Slot;
                parameter.parameter.Descriptor.RegisterSpace = binding.Set;
                parameter.parameter.ShaderVisibility = visibility;
                outRootParameters.push_back(parameter);
            }
            else if (binding.Type == DescriptorType::StorageBuffer || 
                     binding.Type == DescriptorType::SampledImage)
            {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                range.NumDescriptors = binding.Count;
                range.BaseShaderRegister = binding.Slot;
                range.RegisterSpace = binding.Set;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                
                srvRanges.push_back(range);
            }
            else if (binding.Type == DescriptorType::StorageImage)
            {
                D3D12_DESCRIPTOR_RANGE range{};
                range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                range.NumDescriptors = binding.Count;
                range.BaseShaderRegister = binding.Slot;
                range.RegisterSpace = binding.Set;
                range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                
                uavRanges.push_back(range);
            }
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

