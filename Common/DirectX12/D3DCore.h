#pragma once
#include <wrl.h>
#include <dxgi1_6.h>
#include <d3d12.h>

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class Window;

class D3DCore
{
    static D3DCore* Instance;

    bool Initialized = false;

    Window* RendererWindow;
    
    ComPtr<IDXGIFactory6> Factory;
    ComPtr<ID3D12Device> Device;
    
    ComPtr<IDXGISwapChain1> SwapChain;
    ComPtr<ID3D12Fence> Fence;
    UINT64 CurrentFence = 0;
	
    ComPtr<ID3D12CommandQueue> CommandQueue;
    ComPtr<ID3D12CommandAllocator> CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> CommandList;

    static const int SwapChainBufferCount = 3;
    int CurrBackBuffer = 0;
    ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];
    ComPtr<ID3D12Resource> DepthStencilBuffer;

    ComPtr<ID3D12DescriptorHeap> RenderTargetDescriptorHeap;
    ComPtr<ID3D12DescriptorHeap> DepthStencilDescriptorHeap;

    UINT RenderTargetDescriptorOffset = 0;
    UINT DepthStencilDescriptorOffset = 0;
    UINT ShaderResourceDescriptorOffset = 0;

    DXGI_FORMAT RenderTargetFormat;
    
public:

    static D3DCore& GetInstance();
    
    bool InitDirect3D(Window* window);

private:

    void InitDebugLayer();
    void CreateFactory();
    void CreateDevice();
    void CreateFence();
    void CreateCommandObjects();
    void CreateSwapChain();
    
    
    
    
    
};
