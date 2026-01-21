#include "D3DCore.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../Window.h"
#include <exception>
#include <stdexcept>
#include "../GraphicsSettings.h"
using namespace Win32ErrorHandler;

D3DCore& D3DCore::GetInstance()
{
    static D3DCore instance;
    return instance;
}

bool D3DCore::InitDirect3D(Window* window)
{
    try
    {
        if (window == nullptr) throw std::runtime_error("Window is null.");
        RendererWindow = window;
        if (Initialized == true) throw std::runtime_error("Direct3D already initialized.");
        Initialized = true;
        InitDebugLayer();
        CreateFactory();
        CreateDevice();
        CreateFence();
        CreateCommandObjects();
        CreateSwapChain();
        CreateSwapChainDescriptorHeaps();
    }
    catch (const std::exception& exception)
    {
        ErrorMessage(exception.what());
    }
}

void D3DCore::WaitForGPU()
{
    CommandQueue->Signal(Fence.Get(), ++CurrentFence) >> ERROR_HANDLER;
    if(Fence->GetCompletedValue() < CurrentFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        Fence->SetEventOnCompletion(CurrentFence, eventHandle) >> ERROR_HANDLER;
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void D3DCore::Reset()
{
    WaitForGPU();
    
    CommandList.Reset();
    CommandAllocator.Reset();
    CommandQueue.Reset();
    SwapChain.Reset();
    Device.Reset();
    Factory.Reset();
}

void D3DCore::InitDebugLayer()
{
#if defined(DEBUG) || defined(_DEBUG) 
    {
        ComPtr<ID3D12Debug> debugController;
        D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)) >> ERROR_HANDLER;
        debugController->EnableDebugLayer();
    }
#endif
}

void D3DCore::CreateFactory()
{
    UINT flags = 0;
#if defined(DEBUG) || defined(_DEBUG)
    flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    CreateDXGIFactory2(flags, IID_PPV_ARGS(&Factory)) >> ERROR_HANDLER;
}

void D3DCore::CreateDevice()
{
    IDXGIAdapter* adapter = nullptr;
    Factory->EnumAdapterByGpuPreference(
        0,
        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
        IID_PPV_ARGS(&adapter)
        ) >> ERROR_HANDLER;
    D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&Device)) >> ERROR_HANDLER;
    adapter->Release();
}

void D3DCore::CreateFence()
{
    Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence)) >> ERROR_HANDLER;
}

void D3DCore::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    Device->CreateCommandQueue(
        &queueDesc,
        IID_PPV_ARGS(&CommandQueue)
        ) >> ERROR_HANDLER;
    
    Device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(CommandAllocator.GetAddressOf())
        ) >> ERROR_HANDLER;
    
    Device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        CommandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(CommandList.GetAddressOf())
        ) >> ERROR_HANDLER;

    CommandList->Close();
}

void D3DCore::CreateSwapChain()
{
    D3D12_FEATURE_DATA_FORMAT_SUPPORT hdrFormatSupport = {};
    hdrFormatSupport.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;

    Device->CheckFeatureSupport(
        D3D12_FEATURE_FORMAT_SUPPORT,
        &hdrFormatSupport,
        sizeof(hdrFormatSupport)
        ) >> ERROR_HANDLER;

    bool renderTargetSupport = hdrFormatSupport.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET;

    RenderTargetFormat = renderTargetSupport && GRAPHICS_SETTINGS.HDR
        ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
    
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels;
    msaaQualityLevels.Format = RenderTargetFormat;
    msaaQualityLevels.SampleCount = 4;
    msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    msaaQualityLevels.NumQualityLevels = 0;
    Device->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &msaaQualityLevels,
        sizeof(msaaQualityLevels)
        ) >> ERROR_HANDLER;

    bool msaaSupported = msaaQualityLevels.NumQualityLevels > 0;

    DXGI_SAMPLE_DESC msaaSampleDesc;
    msaaSampleDesc.Count = GRAPHICS_SETTINGS.MSAA && msaaSupported ? 4 : 1;
    msaaSampleDesc.Quality = GRAPHICS_SETTINGS.MSAA && msaaSupported ? msaaQualityLevels.NumQualityLevels - 1 : 0;
    
    SwapChain.Reset();
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = RendererWindow->GetWidth();
    swapChainDesc.Height = RendererWindow->GetHeight();
    swapChainDesc.Format = RenderTargetFormat;
    swapChainDesc.Stereo = false;
    swapChainDesc.SampleDesc = msaaSampleDesc;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SwapChainBufferCount;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreenDesc = {};
    fullscreenDesc.RefreshRate.Numerator = 0;
    fullscreenDesc.RefreshRate.Denominator = 1;
    fullscreenDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    fullscreenDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    fullscreenDesc.Windowed = TRUE; 

    Factory->CreateSwapChainForHwnd(
        CommandQueue.Get(),
        RendererWindow->GetWindowHandle(),
        &swapChainDesc,
        &fullscreenDesc,
        nullptr,
        SwapChain.GetAddressOf()
        ) >> ERROR_HANDLER;
}

void D3DCore::CreateSwapChainDescriptorHeaps()
{
    RenderTargetDescriptorOffset = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    DepthStencilDescriptorOffset = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    ShaderResourceDescriptorOffset = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_DESCRIPTOR_HEAP_DESC RenderTargetHeapDescription;
    RenderTargetHeapDescription.NumDescriptors = SwapChainBufferCount;
    RenderTargetHeapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    RenderTargetHeapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    RenderTargetHeapDescription.NodeMask = 0;
    Device->CreateDescriptorHeap(
        &RenderTargetHeapDescription, IID_PPV_ARGS(RenderTargetDescriptorHeap.GetAddressOf())) >> ERROR_HANDLER;


    D3D12_DESCRIPTOR_HEAP_DESC DephtStencilHeapDesc;
    DephtStencilHeapDesc.NumDescriptors = 1;
    DephtStencilHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    DephtStencilHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DephtStencilHeapDesc.NodeMask = 0;
    Device->CreateDescriptorHeap(
        &DephtStencilHeapDesc, IID_PPV_ARGS(DepthStencilDescriptorHeap.GetAddressOf())) >> ERROR_HANDLER;
}

