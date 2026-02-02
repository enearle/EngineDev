#pragma once
#include "D3DCore.h"

namespace D3D12Structs
{
    struct DX12ImageData
    {
        ID3D12Resource* Image = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE Descriptor = {};
    };

    struct DX12BufferData
    {
        ID3D12Resource* Buffer = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;
    };
}
