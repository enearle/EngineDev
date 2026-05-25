#define WIN32_LEAN_AND_MEAN
#define IN_EDITOR
#include <windows.h>
#include <commctrl.h>
#include <wrl/client.h>
#include <d3d12.h>
#include "GraphicsSettings.h"
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "../RHI/Renderer.h"
#include "ForwardInterface.h"
#include "Window.h"
#include "../Game/Game.h"
#include "FileExplorer.h"
#include "Modals/FileMove.h"
#include "Modals/Importer.h"
#include "Modals/NewDirectory.h"
#include "Resources/ResourceManager.h"
#pragma comment(lib, "comctl32.lib")

using Microsoft::WRL::ComPtr;

// ---- ImGui SRV heap (dedicated to the Editor, separate from engine heaps) ----
static ComPtr<ID3D12DescriptorHeap> IM_GUI_SRV_HEAP;
static UINT  IM_GUI_SRV_DESC_SIZE  = 0;
static UINT  IM_GUI_SRV_NEXT_SLOT  = 0;
static constexpr UINT IMGUI_SRV_HEAP_SIZE = 64;

// ---- Editor state ----
static bool IS_PLAYING      = false;
static bool SHOULD_START    = false;
static bool SHOULD_STOP     = false;

// ---- ImGui Win32 subclass: intercepts messages for ImGui input ----
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

static LRESULT CALLBACK EditorSubclassProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp,
                                            UINT_PTR /*id*/, DWORD_PTR /*data*/)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp)) return TRUE;
    return DefSubclassProc(hwnd, msg, wp, lp);
}

// ---- Entry point ----
int main()
{
    GRAPHICS_SETTINGS.SetEditorMode(true);
    ResourceManager::LoadRegistry();
    
    // The Engine's Window creates the Win32 window and handles resize/input routing
    Window* window = new Window(L"Engine Editor", Win32, 1600, 900);
    Renderer::Start(window);
    ShowWindow(window->GetWindowHandle(), SW_SHOW);

    // Subclass the engine window to inject ImGui message handling without
    // replacing the engine's existing WndProc
    SetWindowSubclass(window->GetWindowHandle(), EditorSubclassProc, 1, 0);

    // Initialize ImGui — reuse the engine's D3D12 device and command queue
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(window->GetWindowHandle());

    // Dedicated SRV descriptor heap for ImGui font texture and images
    {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = IMGUI_SRV_HEAP_SIZE;
        desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ForwardInterface::GetD3D12Device()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&IM_GUI_SRV_HEAP));
        IM_GUI_SRV_DESC_SIZE = ForwardInterface::GetD3D12Device()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    auto srvAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
                         D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        UINT slot = IM_GUI_SRV_NEXT_SLOT++;
        cpu->ptr  = IM_GUI_SRV_HEAP->GetCPUDescriptorHandleForHeapStart().ptr + (SIZE_T)(slot * IM_GUI_SRV_DESC_SIZE);
        gpu->ptr  = IM_GUI_SRV_HEAP->GetGPUDescriptorHandleForHeapStart().ptr + (UINT64)(slot * IM_GUI_SRV_DESC_SIZE);
    };
    auto srvFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};

    ImGui_ImplDX12_InitInfo dx12Info = {};
    dx12Info.Device              = ForwardInterface::GetD3D12Device();
    dx12Info.CommandQueue        = ForwardInterface::GetD3D12CommandQueue();
    dx12Info.NumFramesInFlight   = static_cast<int>(ForwardInterface::GetD3D12FramesInFlight());
    dx12Info.RTVFormat           = ForwardInterface::GetD3D12RenderTargetFormat();
    dx12Info.DSVFormat           = DXGI_FORMAT_UNKNOWN;
    dx12Info.SrvDescriptorHeap   = IM_GUI_SRV_HEAP.Get();
    dx12Info.SrvDescriptorAllocFn = srvAllocFn;
    dx12Info.SrvDescriptorFreeFn  = srvFreeFn;
    ImGui_ImplDX12_Init(&dx12Info);

    // Subscribe ImGui rendering to the engine's end-of-frame hook.
    // This fires inside Renderer::DrawFrame() after game geometry is rendered,
    // with the back buffer bound as the render target.
    Renderer::EventOnEndOfFrame().Subscribe([&]()
    {
        if (!IS_PLAYING)
        {
            ID3D12GraphicsCommandList* cmdList = ForwardInterface::GetD3D12CommandList();
            D3D12_CPU_DESCRIPTOR_HANDLE rtv = ForwardInterface::GetD3D12RenderTargetDescriptor();
            float clearColor[] = { 0.1f, 0.1f, 0.1f, 1.0f };
            cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
        }

        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Full-screen host panel (no title bar, no padding)
        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    ImVec2(0, 0));
        ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_NoTitleBar  | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize    | ImGuiWindowFlags_NoMove     |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_MenuBar    |
            ImGuiWindowFlags_NoBackground;
        ImGui::Begin("##Host", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        // Menu bar with Play / Stop controls
        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Exit")) PostQuitMessage(0);
                ImGui::EndMenu();
            }

            float btnOffset = ImGui::GetContentRegionAvail().x * 0.5f - 60.f;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + btnOffset);

            ImGui::BeginDisabled(IS_PLAYING);
            if (ImGui::Button(" Play "))
            {
                IS_PLAYING   = true;
                SHOULD_START = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();

            ImGui::BeginDisabled(!IS_PLAYING);
            if (ImGui::Button(" Stop "))
            {
                IS_PLAYING  = false;
                SHOULD_STOP = true;
            }
            ImGui::EndDisabled();

            ImGui::SameLine();
            ImGui::TextUnformatted(IS_PLAYING ? "Playing" : "Stopped");

            ImGui::EndMenuBar();
        }

        // Left sidebar: Scene Hierarchy + Properties
        float totalW = ImGui::GetContentRegionAvail().x;
        float totalH = ImGui::GetContentRegionAvail().y;
        float sideW  = 220.f;

        ImGui::BeginChild("##LeftPanel", ImVec2(sideW, totalH), true);
        ImGui::TextUnformatted("Scene Hierarchy");
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextUnformatted("Files");
        ImGui::SameLine();
        if (ImGui::Button("Import")) Importer::GetInstance().Open();
        ImGui::Separator();
        FileExplorer::ShowFileTree("../Game/Assets");
        ImGui::EndChild();
        ImGui::SameLine();

        // Game viewport panel — record its screen rect so the renderer can restrict the final pass
        ImGui::BeginChild("##GameViewport", ImVec2(totalW - sideW, totalH), false, ImGuiWindowFlags_NoScrollbar);
        {
            ImVec2 vpPos  = ImGui::GetWindowPos();
            ImVec2 vpSize = ImGui::GetWindowSize();
            Renderer::SetGameViewport(
                static_cast<uint32_t>(vpPos.x),  static_cast<uint32_t>(vpPos.y),
                static_cast<uint32_t>(vpSize.x), static_cast<uint32_t>(vpSize.y));
        }
        ImGui::EndChild();
        
        NewDirectory::GetInstance().Render();
        FileMove::GetInstance().Render();
        Importer::GetInstance().Render();

        ImGui::End();
        ImGui::Render();

        // Bind ImGui's SRV heap and submit draw data to the engine's open command list
        ID3D12GraphicsCommandList* cmdList = ForwardInterface::GetD3D12CommandList();
        ID3D12DescriptorHeap* heaps[] = { IM_GUI_SRV_HEAP.Get() };
        cmdList->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmdList);
    });

    // Main loop
    while (!window->PeekMessages())
    {
        if (SHOULD_START) { GameInit();     SHOULD_START = false; }
        if (SHOULD_STOP)  { GameShutdown(); SHOULD_STOP  = false; }
        if (IS_PLAYING)
            GameRunFrame(0.0f);
        Renderer::DrawFrame();
    }

    // Cleanup
    if (IS_PLAYING)
    {
        IS_PLAYING = false;
        GameShutdown();
    }

    Renderer::WaitForGpu();
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return Renderer::End();
}
