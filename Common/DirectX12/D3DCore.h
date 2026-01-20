#pragma once
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "../Windows/Win32ErrorHandler.h"

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class D3DCore
{
    static D3DCore* Instance;

    bool Initialized = false;
    
    ComPtr<IDXGIFactory6> Factory;
    ComPtr<ID3D12Device> Device;
    
    ComPtr<IDXGISwapChain> SwapChain;
    ComPtr<ID3D12Fence> Fence;
    UINT64 CurrentFence = 0;
	
    ComPtr<ID3D12CommandQueue> CommandQueue;
    ComPtr<ID3D12CommandAllocator> DirectCmdListAlloc;
    ComPtr<ID3D12GraphicsCommandList> CommandList;

    static const int SwapChainBufferCount = 2;
    int CurrBackBuffer = 0;
    ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];
    ComPtr<ID3D12Resource> DepthStencilBuffer;

    ComPtr<ID3D12DescriptorHeap> RtvHeap;
    ComPtr<ID3D12DescriptorHeap> DsvHeap;
    
public:

    static D3DCore& GetInstance();
    
    bool InitDirect3D();

private:

    void InitDebugLayer();
    void CreateFactory();
    void CreateDevice();
    void CreateSwapChain();
    void CreateCommandObjects();
    void CreateFence();
    
    
    
};
