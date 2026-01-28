#pragma once
#include <array>

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


    static const int SwapChainBufferCount = 3;
    int CurrentBackBuffer = 0;
    uint32_t CurrentFrameIndex = 0;
    
    ComPtr<IDXGISwapChain1> SwapChain;
    ComPtr<ID3D12Fence> Fence;
    UINT64 CurrentFence = 0;
	
    ComPtr<ID3D12CommandQueue> CommandQueue;
    std::array<ComPtr<ID3D12CommandAllocator>, SwapChainBufferCount> CommandAllocators;
    std::array<ComPtr<ID3D12GraphicsCommandList>, SwapChainBufferCount> CommandLists;
    std::array<UINT64, SwapChainBufferCount> FrameFences;
    
    ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];
    ComPtr<ID3D12Resource> DepthStencilBuffer;

    ComPtr<ID3D12DescriptorHeap> RenderTargetDescriptorHeap;
    ComPtr<ID3D12DescriptorHeap> DepthStencilDescriptorHeap;

    UINT RenderTargetDescriptorOffset = 0;
    UINT DepthStencilDescriptorOffset = 0;
    UINT ShaderResourceDescriptorOffset = 0;

    DXGI_FORMAT RenderTargetFormat;

    bool SwapChainMSAA = false;
    UINT SwapChainMSAASamples = 1;
    
public:

    static D3DCore& GetInstance();
    
    void InitDirect3D(Window* window, struct CoreInitData data);
    void WaitForGPU();
    void Reset();
    void BeginFrame();
    void EndFrame();

    ComPtr<ID3D12Device> GetDevice() const { return Device; }
    ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return CommandQueue; }
    ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return CommandLists[CurrentFrameIndex]; }
    uint32_t GetCurrentFrameIndex() const { return CurrentFrameIndex; }
    ComPtr<ID3D12DescriptorHeap> GetRenderTargetDescriptorHeap() const { return RenderTargetDescriptorHeap; }
    ComPtr<ID3D12DescriptorHeap> GetDepthStencilDescriptorHeap() const { return DepthStencilDescriptorHeap; }
    UINT GetMSAAQualityLevel(DXGI_FORMAT format, UINT sampleCount);
    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetDescriptor();
    ID3D12Resource* GetBackBuffer() const { return SwapChainBuffer[CurrentFrameIndex].Get(); }

private:
     
    void InitDebugLayer();
    void CreateFactory();
    void CreateDevice();
    void CreateFence();
    void CreateCommandObjects();
    void CreateSwapChain();
    void CreateSwapChainDescriptorHeaps();
    void WaitForFrame(uint32_t frameIndex);
    
};
