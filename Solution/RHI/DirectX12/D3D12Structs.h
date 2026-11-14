#pragma once
#include "D3DCore.h"
using Microsoft::WRL::ComPtr;
namespace D3D12Structs
{
    struct DX12ImageData
    {
        ComPtr<ID3D12Resource> Image;
        D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView = {};
        D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView = {};
        uint32_t RenderTargetSlot = UINT32_MAX;     // UINT32_MAX when the view is not owned by the allocator
        uint32_t DepthStencilSlot = UINT32_MAX;
    };

    struct DX12BufferData
    {
        ComPtr<ID3D12Resource> Buffer;  // Changed from ID3D12Resource*
        D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = 0;
    };
    

}
