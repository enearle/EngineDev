#pragma once
#include "../Windows/WindowsHeaders.h"
#include <dxgi1_6.h>
#include <d3d12.h>

#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

class Window;

class D3DCore
{
    D3DCore() = default;
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
    
    void InitDirect3D(Window* window);
    void WaitForGPU();
    void Reset();

    ComPtr<ID3D12Device> GetDevice() const { return Device; }
    ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return CommandQueue; }
    ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return CommandList; }
    ComPtr<ID3D12DescriptorHeap> GetRenderTargetDescriptorHeap() const { return RenderTargetDescriptorHeap; }
    ComPtr<ID3D12DescriptorHeap> GetDepthStencilDescriptorHeap() const { return DepthStencilDescriptorHeap; }

private:

    void InitDebugLayer();
    void CreateFactory();
    void CreateDevice();
    void CreateFence();
    void CreateCommandObjects();
    void CreateSwapChain();
    void CreateSwapChainDescriptorHeaps();
    
};
