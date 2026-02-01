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
    static ComPtr<ID3D12RootSignature> BuildRootSignature(ID3D12Device* device, const ResourceLayout& layout);

private:
    struct RootParameter
    {
        D3D12_ROOT_PARAMETER parameter;
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges; 
    };
    
    static void CreateRootParameters(const ResourceLayout& layout, std::vector<RootParameter>& outRootParameters, std::vector<D3D12_DESCRIPTOR_RANGE>
                                     & outSRVRanges, std::vector<D3D12_DESCRIPTOR_RANGE>& outUAVRanges);
};
