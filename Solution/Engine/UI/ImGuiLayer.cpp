#include "ImGuiLayer.h"

#include "ForwardInterface.h"
#include <commctrl.h>
#include <wrl/client.h>
#include <vector>
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_vulkan.h"
#include "Renderer.h"
#include "Window.h"

#pragma comment(lib, "comctl32.lib")

using Microsoft::WRL::ComPtr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

struct SubscribedDraw
{
    ImGuiLayer::Subscription ID;
    ImGuiLayer::DrawCallback Callback;
};

static constexpr UINT SrvHeapSize = 64;
static constexpr UINT_PTR WindowSubclassID = 1;
static constexpr int OverlayPassOrder = 100;

static bool HasStarted = false;
static std::vector<SubscribedDraw> SubscribedDraws;
static ImGuiLayer::Subscription NextSubscription = 0;
static Renderer::ExternalPassID ExternalPassID = 0;
static ComPtr<ID3D12DescriptorHeap> SrvHeap;
static UINT SrvDescriptorSize = 0;
static UINT NextSrvSlot = 0;

static LRESULT CALLBACK ImGuiWindowSubclass(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
        return TRUE;
    return DefSubclassProc(hwnd, msg, wParam, lParam);
}

static void StartDirectX12Backend()
{
    ID3D12Device* device = ForwardInterface::GetD3D12Device();

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = SrvHeapSize;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&SrvHeap));
    SrvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    NextSrvSlot = 0;

    ImGui_ImplDX12_InitInfo info = {};
    info.Device = device;
    info.CommandQueue = ForwardInterface::GetD3D12CommandQueue();
    info.NumFramesInFlight = static_cast<int>(ForwardInterface::GetFramesInFlight());
    info.RTVFormat = ForwardInterface::GetD3D12RenderTargetFormat();
    info.DSVFormat = ForwardInterface::GetD3D12OverlayDepthStencilFormat();
    info.SrvDescriptorHeap = SrvHeap.Get();
    info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
    {
        UINT slot = NextSrvSlot++;
        cpu->ptr = SrvHeap->GetCPUDescriptorHandleForHeapStart().ptr + static_cast<SIZE_T>(slot) * SrvDescriptorSize;
        gpu->ptr = SrvHeap->GetGPUDescriptorHandleForHeapStart().ptr + static_cast<UINT64>(slot) * SrvDescriptorSize;
    };
    info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE) {};
    ImGui_ImplDX12_Init(&info);
}

static void StartVulkanBackend()
{
    ImGui_ImplVulkan_InitInfo info = {};
    info.ApiVersion = VK_API_VERSION_1_3;
    info.Instance = ForwardInterface::GetVkInstance();
    info.PhysicalDevice = ForwardInterface::GetVkPhysicalDevice();
    info.Device = ForwardInterface::GetVkDevice();
    info.QueueFamily = ForwardInterface::GetVkQueueFamilyIndex();
    info.Queue = ForwardInterface::GetVkGraphicsQueue();
    info.DescriptorPoolSize = SrvHeapSize;
    info.MinImageCount = 2;
    info.ImageCount = ForwardInterface::GetFramesInFlight();
    info.PipelineInfoMain.RenderPass = ForwardInterface::GetVkOverlayRenderPass();
    info.PipelineInfoMain.Subpass = 0;
    info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&info);
}

static void BuildFrame()
{
    if (ForwardInterface::GetCurrentAPI() == DirectX12)
        ImGui_ImplDX12_NewFrame();
    else
        ImGui_ImplVulkan_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    for (size_t i = 0; i < SubscribedDraws.size(); i++)
        SubscribedDraws[i].Callback();

    ImGui::Render();
}

static void RecordDrawData()
{
    if (ForwardInterface::GetCurrentAPI() == DirectX12)
    {
        ID3D12GraphicsCommandList* commandList = ForwardInterface::GetD3D12CommandList();
        ID3D12DescriptorHeap* heaps[] = { SrvHeap.Get() };
        commandList->SetDescriptorHeaps(1, heaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
    }
    else
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), ForwardInterface::GetVkCommandBuffer());
}

void ImGuiLayer::Start(bool isLayoutSavedToDisk)
{
    if (HasStarted)
        return;

    HWND windowHandle = Renderer::GetWindow()->GetWindowHandle();

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    if (!isLayoutSavedToDisk)
        ImGui::GetIO().IniFilename = nullptr;

    ImGui_ImplWin32_Init(windowHandle);
    SetWindowSubclass(windowHandle, ImGuiWindowSubclass, WindowSubclassID, 0);

    if (ForwardInterface::GetCurrentAPI() == DirectX12)
        StartDirectX12Backend();
    else
        StartVulkanBackend();

    ExternalPassID = Renderer::EnlistExternalPass({ .Prepare = BuildFrame, .Render = RecordDrawData, .Order = OverlayPassOrder, .Name = "ImGui" });
    HasStarted = true;
}

void ImGuiLayer::Shutdown()
{
    if (!HasStarted)
        return;

    Renderer::WaitForGpu();
    Renderer::RetireExternalPass(ExternalPassID);
    RemoveWindowSubclass(Renderer::GetWindow()->GetWindowHandle(), ImGuiWindowSubclass, WindowSubclassID);

    if (ForwardInterface::GetCurrentAPI() == DirectX12)
        ImGui_ImplDX12_Shutdown();
    else
        ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    SrvHeap.Reset();
    SubscribedDraws.clear();
    HasStarted = false;
}

bool ImGuiLayer::IsStarted()
{
    return HasStarted;
}

ImGuiLayer::Subscription ImGuiLayer::Subscribe(DrawCallback callback)
{
    Subscription id = NextSubscription++;
    SubscribedDraws.push_back({ id, std::move(callback) });
    return id;
}

void ImGuiLayer::Unsubscribe(Subscription subscription)
{
    std::erase_if(SubscribedDraws, [subscription](const SubscribedDraw& draw) { return draw.ID == subscription; });
}
