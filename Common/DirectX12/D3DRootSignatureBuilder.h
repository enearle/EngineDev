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
    static ComPtr<ID3D12RootSignature> BuildRootSignature(uint32_t pipelineID, const std::vector<ResourceLayout>& layouts);

private:
    struct RootParameter
    {
        D3D12_ROOT_PARAMETER parameter;
        std::vector<D3D12_DESCRIPTOR_RANGE> ranges; 
    };
    
    static void CreateRootParameters(uint32_t pipelineID, const std::vector<ResourceLayout>& layouts, std::vector<RootParameter>& outRootParameters);
};
