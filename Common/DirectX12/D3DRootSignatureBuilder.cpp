#include "D3DRootSignatureBuilder.h"
#include "../Windows/Win32ErrorHandler.h"
#include <stdexcept>
#include <d3dcompiler.h>

using namespace Win32ErrorHandler;

ComPtr<ID3D12RootSignature> D3DRootSignatureBuilder::BuildRootSignature(ID3D12Device* device, const ResourceLayout& layout)
{
    if (!device)
        throw std::runtime_error("D3D12 device is null");
    
    std::vector<RootParameter> rootParameters;
    std::vector<D3D12_DESCRIPTOR_RANGE> srvRanges;
    std::vector<D3D12_DESCRIPTOR_RANGE> uavRanges;
    CreateRootParameters(layout, rootParameters, srvRanges, uavRanges);

    // Extract the actual parameter descriptors
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

    // Create root signature description
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = static_cast<UINT>(parameters.size());
    rootSigDesc.pParameters = parameters.data();
    rootSigDesc.NumStaticSamplers = 1;
    rootSigDesc.pStaticSamplers = &samplerDesc;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // Serialize root signature
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(
        &rootSigDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        std::string errorMsg = "Failed to serialize root signature";
        if (errorBlob)
        {
            errorMsg += ": ";
            errorMsg += static_cast<const char*>(errorBlob->GetBufferPointer());
        }
        throw std::runtime_error(errorMsg);
    }
    
    ComPtr<ID3D12RootSignature> rootSignature;
    hr = device->CreateRootSignature(
        0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(rootSignature.GetAddressOf())
    );

    if (FAILED(hr))
        throw std::runtime_error("Failed to create root signature");

    return rootSignature;
}


void D3DRootSignatureBuilder::CreateRootParameters(const ResourceLayout& layout, std::vector<RootParameter>& outRootParameters,
            std::vector<D3D12_DESCRIPTOR_RANGE>& outSRVRanges, std::vector<D3D12_DESCRIPTOR_RANGE>& outUAVRanges)
{
    
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
            
            outSRVRanges.push_back(range);
        }
        else if (binding.Type == DescriptorType::StorageImage)
        {
            D3D12_DESCRIPTOR_RANGE range{};
            range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            range.NumDescriptors = binding.Count;
            range.BaseShaderRegister = binding.Slot;
            range.RegisterSpace = binding.Set;
            range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
            
            outUAVRanges.push_back(range);
        }
    }

    if (!outSRVRanges.empty())
    {
        RootParameter parameter = {};
        parameter.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.parameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(outSRVRanges.size());
        parameter.parameter.DescriptorTable.pDescriptorRanges = outSRVRanges.data();
        parameter.parameter.ShaderVisibility = visibility;
        parameter.ranges = outSRVRanges;
        outRootParameters.push_back(parameter);
    }

    if (!outUAVRanges.empty())
    {
        RootParameter parameter = {};
        parameter.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        parameter.parameter.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(outUAVRanges.size());
        parameter.parameter.DescriptorTable.pDescriptorRanges = outUAVRanges.data();
        parameter.parameter.ShaderVisibility = visibility;
        parameter.ranges = outUAVRanges;
        outRootParameters.push_back(parameter);
    }

    for (RootParameter& rootParameter : outRootParameters)
    {
        if (rootParameter.parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            rootParameter.parameter.DescriptorTable.pDescriptorRanges = rootParameter.ranges.data();
        }
    }
}

