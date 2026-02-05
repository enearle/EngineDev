#pragma once
#include "D3DCore.h"
using Microsoft::WRL::ComPtr;
namespace D3D12Structs
{
    struct DX12ImageData
    {
        ComPtr<ID3D12Resource> Image;
        D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = {};
    };

    struct DX12BufferData
    {
        ID3D12Resource* Buffer = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;
    };
    

}
