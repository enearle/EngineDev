#include "D3DCore.h"

#include <exception>
#include <stdexcept>

using namespace Win32ErrorHandler;

D3DCore& D3DCore::GetInstance()
{
    static D3DCore instance;
    return instance;
}

bool D3DCore::InitDirect3D()
{
    try
    {
        if (Initialized == true) throw std::runtime_error("Direct3D already initialized.");
        Initialized = true;
        InitDebugLayer();
        CreateFactory();
        CreateDevice();

        
    }
    catch (const std::exception& e)
    {
        ErrorMessage(e.what());
    }
    
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
    Factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) >> ERROR_HANDLER;
    D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&Device)) >> ERROR_HANDLER;
    adapter->Release();
}
