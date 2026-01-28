#include "D3DRootSignatureBuilder.h"
#include "../Windows/Win32ErrorHandler.h"
#include <stdexcept>
#include <d3dcompiler.h>

using namespace Win32ErrorHandler;

ComPtr<ID3D12RootSignature> D3DRootSignatureBuilder::BuildRootSignature(
    ID3D12Device* device,
    const ResourceLayout& layout)
{
    if (!device)
        throw std::runtime_error("D3D12 device is null");

    std::vector<RootParameter> rootParameters;
    rootParameters.reserve(layout.Bindings.size());

    UINT rootParameterIndex = 0;

    // Create root parameters from bindings
    for (const auto& binding : layout.Bindings)
    {
        rootParameters.push_back(CreateRootParameter(binding, rootParameterIndex));
        rootParameterIndex++;
    }
    
    for (auto& rp : rootParameters)
    {
        if (rp.parameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
        {
            rp.parameter.DescriptorTable.pDescriptorRanges = rp.ranges.data();
        }
    }

    // Extract the actual parameter descriptors
    std::vector<D3D12_ROOT_PARAMETER> parameters;
    parameters.reserve(rootParameters.size());
    for (auto& rp : rootParameters)
    {
        parameters.push_back(rp.parameter);
    }

    // Create root signature description
    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = static_cast<UINT>(parameters.size());
    rootSigDesc.pParameters = parameters.data();
    rootSigDesc.NumStaticSamplers = 0;
    rootSigDesc.pStaticSamplers = nullptr;
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


D3DRootSignatureBuilder::RootParameter D3DRootSignatureBuilder::CreateRootParameter(
    const DescriptorBinding& binding,
    UINT& rootParameterIndex)
{
    RootParameter result = {};

    switch (binding.Type)
    {
        case DescriptorType::UniformBuffer:
        {
            result.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            result.parameter.Descriptor.ShaderRegister = binding.Slot;
            result.parameter.Descriptor.RegisterSpace = binding.Set;
            result.parameter.ShaderVisibility = GetShaderVisibility(binding.VisibleStages);
            break;
        }

        case DescriptorType::StorageBuffer:
        {
            result.ranges.resize(1);
            result.ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            result.ranges[0].NumDescriptors = binding.Count;
            result.ranges[0].BaseShaderRegister = binding.Slot;
            result.ranges[0].RegisterSpace = binding.Set;
            result.ranges[0].OffsetInDescriptorsFromTableStart = 0;

            result.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            result.parameter.DescriptorTable.NumDescriptorRanges = 1;
            result.parameter.DescriptorTable.pDescriptorRanges = result.ranges.data();
            result.parameter.ShaderVisibility = GetShaderVisibility(binding.VisibleStages);
            break;
        }

        case DescriptorType::SampledImage:
        {
            result.ranges.resize(1);
            result.ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            result.ranges[0].NumDescriptors = binding.Count;
            result.ranges[0].BaseShaderRegister = binding.Slot;
            result.ranges[0].RegisterSpace = binding.Set;
            result.ranges[0].OffsetInDescriptorsFromTableStart = 0;

            result.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            result.parameter.DescriptorTable.NumDescriptorRanges = 1;
            result.parameter.DescriptorTable.pDescriptorRanges = result.ranges.data();
            result.parameter.ShaderVisibility = GetShaderVisibility(binding.VisibleStages);
            break;
        }

        case DescriptorType::StorageImage:
        {
            result.ranges.resize(1);
            result.ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            result.ranges[0].NumDescriptors = binding.Count;
            result.ranges[0].BaseShaderRegister = binding.Slot;
            result.ranges[0].RegisterSpace = binding.Set;
            result.ranges[0].OffsetInDescriptorsFromTableStart = 0;

            result.parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            result.parameter.DescriptorTable.NumDescriptorRanges = 1;
            result.parameter.DescriptorTable.pDescriptorRanges = result.ranges.data();
            result.parameter.ShaderVisibility = GetShaderVisibility(binding.VisibleStages);
            break;
        }

        default:
            throw std::runtime_error("Unknown descriptor type");
    }

    return result;
}

D3D12_SHADER_VISIBILITY D3DRootSignatureBuilder::GetShaderVisibility(
    const ShaderStageMask& stages)
{
    // DirectX 12 does not support fine-grained shader visibility.
    // Parameters are either visible to 1 stage or all stages...
    int stageCount = 0;
    if (stages.Vertex) stageCount++;
    if (stages.Fragment) stageCount++;
    if (stages.Geometry) stageCount++;
    if (stages.TessControl) stageCount++;
    if (stages.TessEval) stageCount++;
    if (stages.Compute) stageCount++;
    
    if (stageCount > 1 || stages.Compute)
        return D3D12_SHADER_VISIBILITY_ALL;
    
    if (stages.Vertex)
        return D3D12_SHADER_VISIBILITY_VERTEX;
    if (stages.Fragment)
        return D3D12_SHADER_VISIBILITY_PIXEL;
    if (stages.Geometry)
        return D3D12_SHADER_VISIBILITY_GEOMETRY;
    if (stages.TessControl)
        return D3D12_SHADER_VISIBILITY_HULL;
    if (stages.TessEval)
        return D3D12_SHADER_VISIBILITY_DOMAIN;
    
    return D3D12_SHADER_VISIBILITY_ALL;
}

D3D12_DESCRIPTOR_RANGE_TYPE D3DRootSignatureBuilder::GetDescriptorRangeType(
    DescriptorType type)
{
    switch (type)
    {
        case DescriptorType::UniformBuffer:
            return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
        case DescriptorType::StorageBuffer:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case DescriptorType::SampledImage:
            return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
        case DescriptorType::StorageImage:
            return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
        default:
            throw std::runtime_error("Unknown descriptor type");
    }
}
