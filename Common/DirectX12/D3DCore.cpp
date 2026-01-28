#include "D3DCore.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../Window.h"
#include <exception>
#include <iostream>
#include <stdexcept>
#include "../GraphicsSettings.h"
#include "../RHI/Renderer.h"
using namespace Win32ErrorHandler;

D3DCore& D3DCore::GetInstance()
{
    static D3DCore instance;
    return instance;
}

void D3DCore::InitDirect3D(Window* window, CoreInitData data)
{
    try
    {
        if (window == nullptr) throw std::runtime_error("Window is null.");
        RendererWindow = window;
        if (Initialized == true) throw std::runtime_error("Direct3D already initialized.");
        Initialized = true;
        SwapChainMSAA = data.SwapchainMSAA;
        SwapChainMSAASamples = data.SwapchainMSAASamples;
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
        Initialized = false;
        throw;
    }
}

void D3DCore::WaitForGPU()
{
    for (int i = 0; i < SwapChainBufferCount; ++i)
    {
        WaitForFrame(i);
    }

}

void D3DCore::Reset()
{
    WaitForGPU();

    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        CommandLists[i].Reset();
        CommandAllocators[i].Reset();
    }
    CommandQueue.Reset();
    SwapChain.Reset();
    Device.Reset();
    Factory.Reset();
}

void D3DCore::BeginFrame()
{
    WaitForFrame(CurrentFrameIndex);
    CommandAllocators[CurrentFrameIndex]->Reset();
    CommandLists[CurrentFrameIndex]->Reset(CommandAllocators[CurrentFrameIndex].Get(), nullptr);
}

void D3DCore::EndFrame()
{
    std::cerr << "EndFrame: Closing command list..." << std::endl;
    HRESULT hrClose = CommandLists[CurrentFrameIndex]->Close();
    if (FAILED(hrClose))
    {
        throw std::runtime_error("Failed to close command list. HRESULT: 0x" + 
            std::to_string(static_cast<unsigned int>(hrClose)));
    }
    
    std::cerr << "EndFrame: Command list closed successfully" << std::endl;
    std::cerr << "EndFrame: Executing command lists..." << std::endl;
    
    ID3D12CommandList* ppCommandLists[] = { CommandLists[CurrentFrameIndex].Get() };
    
    // This will crash if the command list or queue is in a bad state
    CommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
    
    std::cerr << "EndFrame: Command lists executed successfully" << std::endl;
    
    CurrentFence++;
    FrameFences[CurrentFrameIndex] = CurrentFence;

    CurrentFence++;
    FrameFences[CurrentFrameIndex] = CurrentFence;
    CommandQueue->Signal(Fence.Get(), CurrentFence) >> ERROR_HANDLER;
    SwapChain->Present(1, 0) >> ERROR_HANDLER;
    CurrentFrameIndex = (CurrentFrameIndex + 1) % SwapChainBufferCount;
}

void D3DCore::InitDebugLayer()
{
#if defined(DEBUG) || defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    ComPtr<ID3D12Debug1> debugController1;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        debugController->EnableDebugLayer();
        if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1))))
        {
            debugController1->SetEnableGPUBasedValidation(true);
        }
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
    
    for (int i = 0; i < SwapChainBufferCount; ++i)
    {
        Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(CommandAllocators[i].GetAddressOf())) >> ERROR_HANDLER;

        Device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            CommandAllocators[i].Get(),
            nullptr,
            IID_PPV_ARGS(CommandLists[i].GetAddressOf())) >> ERROR_HANDLER;
        
        CommandLists[i]->Close();
        
        FrameFences[i] = 0;
    }

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
    
    UINT msaaQualityLevels = GetMSAAQualityLevel(RenderTargetFormat, SwapChainMSAASamples);
    bool msaaSupported = msaaQualityLevels > 0;

    DXGI_SAMPLE_DESC msaaSampleDesc;
    msaaSampleDesc.Count = SwapChainMSAA && msaaSupported ? SwapChainMSAASamples : 1;
    msaaSampleDesc.Quality = SwapChainMSAA && msaaSupported ? msaaQualityLevels : 0;
    
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

    for (UINT i = 0; i < SwapChainBufferCount; ++i)
    {
        ComPtr<ID3D12Resource> backBuffer;
        SwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));

        SwapChainBuffer[i] = backBuffer;
        
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = 
            RenderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += i * RenderTargetDescriptorOffset;
        
        Device->CreateRenderTargetView(backBuffer.Get(), nullptr, rtvHandle);
    }
}

void D3DCore::WaitForFrame(uint32_t frameIndex)
{
    UINT64 frameFence = FrameFences[frameIndex];
    if (Fence->GetCompletedValue() < frameFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        Fence->SetEventOnCompletion(frameFence, eventHandle) >> ERROR_HANDLER;
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

UINT D3DCore::GetMSAAQualityLevel(DXGI_FORMAT format, UINT sampleCount)
{
    if (sampleCount <= 1) return 0;
    
    D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels = {};
    msaaQualityLevels.Format = format;
    msaaQualityLevels.SampleCount = sampleCount;
    msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
    
    Device->CheckFeatureSupport(
        D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
        &msaaQualityLevels,
        sizeof(msaaQualityLevels)
    ) >> ERROR_HANDLER;

    return msaaQualityLevels.NumQualityLevels > 0 
        ? msaaQualityLevels.NumQualityLevels - 1 
        : 0;
}

D3D12_CPU_DESCRIPTOR_HANDLE D3DCore::GetRenderTargetDescriptor()
{
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = 
        RenderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += CurrentFrameIndex * RenderTargetDescriptorOffset;
    return rtvHandle;
}
