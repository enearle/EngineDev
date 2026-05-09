#pragma once
#include <array>
#include "../Windows/WindowsHeaders.h"
#include <dxgi1_6.h>
#include <d3d12.h>
#include <vector>
#include "../RHI/RHIStructures.h"
#include "d3dx12.h"

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
    
    ComPtr<ID3D12GraphicsCommandList> TransferCommandList;
    ComPtr<ID3D12CommandAllocator> TransferCommandAllocator;
    ComPtr<ID3D12CommandQueue> TransferCommandQueue;
    ComPtr<ID3D12Fence> TransferFence;
    
    ComPtr<ID3D12Resource> SwapChainBuffer[SwapChainBufferCount];
    ComPtr<ID3D12Resource> NoesisDepthStencilBuffers[SwapChainBufferCount];
    ComPtr<ID3D12Resource> DepthStencilBuffer;
    ComPtr<ID3D12Resource> NoesisDepthStencilBuffer;
    
    ComPtr<ID3D12DescriptorHeap> RenderTargetDescriptorHeap;
    ComPtr<ID3D12DescriptorHeap> DepthStencilDescriptorHeap;
    ComPtr<ID3D12DescriptorHeap> NoesisDepthStencilDescriptorHeap;

    UINT RenderTargetDescriptorOffset = 0;
    UINT DepthStencilDescriptorOffset = 0;

    DXGI_FORMAT RenderTargetFormat;

    bool SwapChainMSAA = false;
    UINT SwapChainMSAASamples = 1;
    UINT SwapChainMSAAQuality = 0;
    
public:
    ~D3DCore() = default;
    static D3DCore& Instance();
    
    void InitDirect3D(Window* window, RHIStructures::CoreInitData data);
    void WaitForGpu();
    void Reset();
    void BeginFrame();
    void EndFrame();
    void ResetWindow();

    void DeferUploadBufferRelease(ComPtr<ID3D12Resource> resource);
    ComPtr<ID3D12Device> GetDevice() const { return Device; }
    ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return CommandQueue; }
    ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return CommandLists[CurrentFrameIndex]; }
    ComPtr<ID3D12GraphicsCommandList> GetTransferCommandList() const { return TransferCommandList; }
    ComPtr<ID3D12CommandAllocator> GetTransferCommandAllocator() const { return TransferCommandAllocator; }
    ComPtr<ID3D12Fence> GetFrameFence() const { return Fence; }
    ComPtr<ID3D12Fence> GetTransferFence() const { return TransferFence; }
    uint32_t GetCurrentFrameIndex() const { return CurrentFrameIndex; }
    uint32_t GetFramesInFlight() const { return SwapChainBufferCount; }
    ComPtr<ID3D12DescriptorHeap> GetRenderTargetDescriptorHeap() const { return RenderTargetDescriptorHeap; }
    ComPtr<ID3D12DescriptorHeap> GetNoesisDepthStencilDescriptorHeap() const { return DepthStencilDescriptorHeap; }
    UINT GetMSAAQualityLevel(DXGI_FORMAT format, UINT sampleCount);
    D3D12_CPU_DESCRIPTOR_HANDLE GetRenderTargetDescriptor();
    D3D12_CPU_DESCRIPTOR_HANDLE GetNoesisDepthStencilDescriptor();
    ID3D12Resource* GetCurrentBackBuffer() const { return SwapChainBuffer[CurrentFrameIndex].Get(); }
    DXGI_FORMAT GetRenderTargetFormat() const { return RenderTargetFormat; }
    DXGI_SAMPLE_DESC GetSampleDesc() const 
    { 
        DXGI_SAMPLE_DESC desc;
        desc.Count = SwapChainMSAA ? SwapChainMSAASamples : 1;
        desc.Quality = SwapChainMSAAQuality;
        return desc;
    }

private:
     
    void InitDebugLayer();
    void CreateFactory();
    void CreateDevice();
    void CreateFence();
    void CreateCommandObjects();
    void CreateSwapChain();
    void CreateSwapChainDescriptorHeaps();
    void CreateNoesisDepthStencilBuffers();
    void WaitForFrame(uint32_t frameIndex);

    std::vector<ComPtr<ID3D12Resource>> DeferredUploadReleases[SwapChainBufferCount]; 
    
};
