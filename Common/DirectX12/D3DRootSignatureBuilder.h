#pragma once
#include <d3d12.h>
#include <vector>
#include <wrl/client.h>
#include "../RHI/RHIStructures.h"

using Microsoft::WRL::ComPtr;
using namespace RHIStructures;

class D3DRootSignatureBuilder
{
public:
    static ComPtr<ID3D12RootSignature> BuildRootSignature(
        ID3D12Device* device,
        const ResourceLayout& layout
    );

private:
    struct RootParameter
    {
        D3D12_ROOT_PARAMETER parameter;
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges; 
    };
    
    static RootParameter CreateRootParameter(
        const DescriptorBinding& binding,
        UINT& rootParameterIndex
    );
    
    static D3D12_SHADER_VISIBILITY GetShaderVisibility(const ShaderStageMask& stages);
    static D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeType(DescriptorType type);
};
