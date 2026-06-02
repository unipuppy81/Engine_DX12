#include "pch.h"
#include "ImGuiManager.h"
#include "DEngine.h"
#include "TableDescriptorHeap.h"
#include "Timer.h"
#include "DiagnosticsManager.h"

void ImGuiManager::Init(HWND hwnd)
{
    CreateDescriptorHeap();

    // Show the window
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplDX12_InitInfo initInfo = {};
    initInfo.Device = DEVICE.Get();
    initInfo.CommandQueue = GRAPHICS_CMD_QUEUE->GetCmdQueue().Get();
    initInfo.NumFramesInFlight = SWAP_CHAIN_BUFFER_COUNT;
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    initInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
    initInfo.SrvDescriptorHeap = _descriptorHeap.Get();

    initInfo.SrvDescriptorAllocFn =
        [](ImGui_ImplDX12_InitInfo* info,
            D3D12_CPU_DESCRIPTOR_HANDLE* outCpu,
            D3D12_GPU_DESCRIPTOR_HANDLE* outGpu)
        {
            ID3D12DescriptorHeap* heap = info->SrvDescriptorHeap;
            *outCpu = heap->GetCPUDescriptorHandleForHeapStart();
            *outGpu = heap->GetGPUDescriptorHandleForHeapStart();
        };

    initInfo.SrvDescriptorFreeFn =
        [](ImGui_ImplDX12_InitInfo*,
            D3D12_CPU_DESCRIPTOR_HANDLE,
            D3D12_GPU_DESCRIPTOR_HANDLE)
        {
            // 최소 구현: 해제 처리 안 함
        };

    bool result = ImGui_ImplDX12_Init(&initInfo);
    assert(result);
}

void ImGuiManager::BeginFrame()
{
    // 새 ImGui 프레임 시작
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowSize(ImVec2(430, 520), ImGuiCond_FirstUseEver);

    ImGui::Begin("Renderer Debug");
    
    ShowPerformance();

    ImGui::End();

    // ImGui 내부 DrawData 생성
    ImGui::Render();
}

void ImGuiManager::Render()
{
    ID3D12DescriptorHeap* heaps[] = { _descriptorHeap.Get() };

    // ImGui가 사용하는 SRV DescriptorHeap 바인딩
    GRAPHICS_CMD_LIST->SetDescriptorHeaps(1, heaps);

    // ImGui DrawData 렌더링
    ImGui_ImplDX12_RenderDrawData(
        ImGui::GetDrawData(),
        GRAPHICS_CMD_LIST.Get()
    );
}

void ImGuiManager::Shutdown()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiManager::CreateDescriptorHeap()
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 64;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = DEVICE.Get()->CreateDescriptorHeap(
        &desc,
        IID_PPV_ARGS(&_descriptorHeap)
    );

    assert(SUCCEEDED(hr));
}

void ImGuiManager::ShowPerformance()
{
    uint32 fps = GET_SINGLE(Timer)->GetFps();

    ImGui::Text("Performance");
    ImGui::Text("FPS :  %.0f", GET_SINGLE(DiagnosticsManager)->GetFps());
    ImGui::Text("CPU Frame: %.3f ms", GET_SINGLE(DiagnosticsManager)->GetCpuFrameMs());
    ImGui::Text("GPU Frame: %.3f ms", GET_SINGLE(DiagnosticsManager)->GetGpuFrameMs());
    ImGui::Text("DrawCall Count: %d", GET_SINGLE(DiagnosticsManager)->GetDrawCallCount());
    ImGui::Text("Triangle Count: %d", GET_SINGLE(DiagnosticsManager)->GetTriangleCount());
}
