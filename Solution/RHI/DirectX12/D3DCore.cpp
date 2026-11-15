#include "D3DCore.h"
#include "../Windows/Win32ErrorHandler.h"
#include "../Window.h"
#include <exception>
#include <iostream>
#include <stdexcept>
#include "../GraphicsSettings.h"
#include "../RHI/BufferAllocator.h"


using namespace Win32ErrorHandler;
using namespace RHIStructures;

D3DCore& D3DCore::Instance()
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
        SubscribeDebugMessages();
        CreateFence();
        CreateCommandObjects();
        CreateSwapChain();
        CreateSwapChainDescriptorHeaps();
        CreateOverlayDepthStencilBuffers();
    }
    catch (const std::exception& exception)
    {
        ErrorMessage(exception.what());
        Initialized = false;
        throw;
    }
}

void D3DCore::WaitForGpu()
{
    for (int i = 0; i < SwapChainBufferCount; ++i)
    {
        WaitForFrame(i);
    }
}

void D3DCore::Reset()
{
    if (CommandQueue)
    {
        CurrentFence++;
        CommandQueue->Signal(Fence.Get(), CurrentFence) >> ERROR_HANDLER;
        WaitForGpu();
    }

    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        CommandLists[i].Reset();
        CommandAllocators[i].Reset();
        SwapChainBuffer[i].Reset();
        OverlayDepthStencilBuffers[i].Reset();
    }
    DepthStencilBuffer.Reset();
    RenderTargetDescriptorHeap.Reset();
    DepthStencilDescriptorHeap.Reset();
    OverlayDepthStencilDescriptorHeap.Reset();
    Fence.Reset();
    CommandQueue.Reset();
    SwapChain.Reset();

#if defined(DEBUG) || defined(_DEBUG)
    ComPtr<ID3D12InfoQueue1> infoQueue;
    if (MessageCallbackCookie != 0 && SUCCEEDED(Device.As(&infoQueue)))
        infoQueue->UnregisterMessageCallback(MessageCallbackCookie);
    MessageCallbackCookie = 0;
#endif

    Device.Reset();
    Factory.Reset();
    Initialized = false;
}

void D3DCore::BeginFrame()
{
    WaitForFrame(CurrentFrameIndex);
    FrameNumber++;
    CommandAllocators[CurrentFrameIndex]->Reset();
    CommandLists[CurrentFrameIndex]->Reset(CommandAllocators[CurrentFrameIndex].Get(), nullptr);
}

void D3DCore::EndFrame()
{
    CommandLists[CurrentFrameIndex]->Close() >> ERROR_HANDLER;

    ID3D12CommandList* commandLists[] = { CommandLists[CurrentFrameIndex].Get() };
    CommandQueue->ExecuteCommandLists(1, commandLists);

    CurrentFence++;
    FrameFences[CurrentFrameIndex] = CurrentFence;
    SlotFrameNumbers[CurrentFrameIndex] = FrameNumber;
    CommandQueue->Signal(Fence.Get(), CurrentFence) >> ERROR_HANDLER;

    bool isTearing = !GRAPHICS_SETTINGS.VSync && IsTearingSupported;
    SwapChain->Present(GRAPHICS_SETTINGS.VSync ? 1 : 0, isTearing ? DXGI_PRESENT_ALLOW_TEARING : 0) >> ERROR_HANDLER;
    CurrentFrameIndex = (CurrentFrameIndex + 1) % SwapChainBufferCount;
}

void D3DCore::ResetWindow()
{
    // Drain DXGI's present-queued flip work before releasing swapchain buffers.
    // Skip at startup (FrameFences all zero) since Present() hasn't been called yet.
    bool hasRenderedFrames = false;
    for (int i = 0; i < SwapChainBufferCount; ++i)
        if (FrameFences[i] > 0) { hasRenderedFrames = true; break; }

    if (hasRenderedFrames)
    {
        CurrentFence++;
        CommandQueue->Signal(Fence.Get(), CurrentFence) >> ERROR_HANDLER;
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
        Fence->SetEventOnCompletion(CurrentFence, eventHandle) >> ERROR_HANDLER;
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    for (auto& buffer : SwapChainBuffer)
        buffer.Reset();

    uint32_t width = RendererWindow->GetWidth();
    uint32_t height = RendererWindow->GetHeight();

    SwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, SwapChainFlags()) >> ERROR_HANDLER;

    for (UINT i = 0; i < SwapChainBufferCount; ++i)
    {
        SwapChain->GetBuffer(i, IID_PPV_ARGS(&SwapChainBuffer[i])) >> ERROR_HANDLER;

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = RenderTargetDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        rtvHandle.ptr += i * RenderTargetDescriptorOffset;
        Device->CreateRenderTargetView(SwapChainBuffer[i].Get(), nullptr, rtvHandle);
    }

    CurrentBackBuffer = 0;
    CurrentFrameIndex = 0;

    for (auto& buf : OverlayDepthStencilBuffers)
        buf.Reset();
    CreateOverlayDepthStencilBuffers();
}

void D3DCore::InitDebugLayer()
{
#if defined(DEBUG) || defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    ComPtr<ID3D12Debug1> debugController1;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
    {
        if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1))))
        {
            debugController->EnableDebugLayer();
            debugController->Release();
            debugController1->SetEnableGPUBasedValidation(true);
            std::cerr << "DirectX debugging enabled." << std::endl;
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

    BOOL allowTearing = FALSE;
    if (SUCCEEDED(Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing))))
        IsTearingSupported = allowTearing == TRUE;
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

void D3DCore::SubscribeDebugMessages()
{
#if defined(DEBUG) || defined(_DEBUG)
    // ID3D12InfoQueue1 needs Windows 11; without it the messages still reach the debugger output.
    ComPtr<ID3D12InfoQueue1> infoQueue;
    if (FAILED(Device.As(&infoQueue)))
        return;

    infoQueue->RegisterMessageCallback(
        [](D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID, LPCSTR description, void*)
        {
            if (severity <= D3D12_MESSAGE_SEVERITY_WARNING)
                std::cerr << "d3d12 debug layer: " << description << std::endl;
        },
        D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &MessageCallbackCookie);
#endif
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

    SwapChainMSAAQuality = msaaSampleDesc.Quality;

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
    swapChainDesc.Flags = SwapChainFlags();

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

    D3D12_DESCRIPTOR_HEAP_DESC OverlayDepthHeapDesc;
    OverlayDepthHeapDesc.NumDescriptors = SwapChainBufferCount;
    OverlayDepthHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    OverlayDepthHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    OverlayDepthHeapDesc.NodeMask = 0;
    Device->CreateDescriptorHeap(&OverlayDepthHeapDesc, IID_PPV_ARGS(OverlayDepthStencilDescriptorHeap.GetAddressOf())) >> ERROR_HANDLER;

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

void D3DCore::CreateOverlayDepthStencilBuffers()
{
    auto heap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC backBufferDesc = SwapChainBuffer[0]->GetDesc();

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = backBufferDesc.Width;
    desc.Height = backBufferDesc.Height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = RHIStructures::DXFormat(Format::D24_UNORM_S8_UINT);
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = desc.Format;

    for (UINT i = 0; i < SwapChainBufferCount; ++i)
    {
        Device->CreateCommittedResource(
            &heap,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &clearValue,
            IID_PPV_ARGS(OverlayDepthStencilBuffers[i].GetAddressOf())
        ) >> ERROR_HANDLER;

        OverlayDepthStencilBuffers[i]->SetName(L"Overlay depth stencil");

        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = OverlayDepthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        dsvHandle.ptr += i * DepthStencilDescriptorOffset;
        Device->CreateDepthStencilView(OverlayDepthStencilBuffers[i].Get(), nullptr, dsvHandle);
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

D3D12_CPU_DESCRIPTOR_HANDLE D3DCore::GetOverlayDepthStencilDescriptor()
{
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        OverlayDepthStencilDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    dsvHandle.ptr += CurrentFrameIndex * DepthStencilDescriptorOffset;
    return dsvHandle;
}

UINT D3DCore::SwapChainFlags() const
{
    return DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH | (IsTearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
}

uint64_t D3DCore::GetCompletedFrameNumber() const
{
    UINT64 completedFence = Fence->GetCompletedValue();
    uint64_t completed = 0;
    for (int i = 0; i < SwapChainBufferCount; i++)
        if (FrameFences[i] != 0 && FrameFences[i] <= completedFence)
            completed = (std::max)(completed, SlotFrameNumbers[i]);
    return completed;
}
