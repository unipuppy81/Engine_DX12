#include "pch.h"
#include "ImGuiManager.h"
#include "DEngine.h"
#include "TableDescriptorHeap.h"
#include "Timer.h"
#include "Camera.h"
#include "InstancingManager.h"
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
    // RenderGraph CommandList의 RTV 상태는 Main CommandList에 이어지지 않음.
    // 현재 BackBuffer RTV를 Main CommandList에 다시 바인딩.
    uint32 backIndex = GDEngine->GetSwapChain()->GetBackBufferIndex();

    GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SWAP_CHAIN)->OMSetRenderTargets(1, backIndex);

    // ImGui Descriptor Heap
    ID3D12DescriptorHeap* heaps[] = { _descriptorHeap.Get() };
    GRAPHICS_CMD_LIST->SetDescriptorHeaps(1, heaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), GRAPHICS_CMD_LIST);
}

void ImGuiManager::Shutdown()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    _descriptorHeap.Reset();
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
    ImGui::Text("Performance");
    ImGui::Text("FPS :  %.0f", GET_SINGLE(DiagnosticsManager)->GetFps());
    ImGui::Text("CPU Frame: %.3f ms", GET_SINGLE(DiagnosticsManager)->GetCpuFrameMs());
    ImGui::Text("GPU Frame: %.3f ms", GET_SINGLE(DiagnosticsManager)->GetGpuFrameMs());
    ImGui::Text("Render Prepare: %.3f ms", GET_SINGLE(DiagnosticsManager)->GetRenderPrepareMs());
    ImGui::Text("RenderGraph Compile: %.3f ms", GET_SINGLE(DiagnosticsManager)->GetRenderGraphCompileMs());
    ImGui::Text("Command Record: %.3f ms", GET_SINGLE(DiagnosticsManager)->GetCommandRecordMs());
    ImGui::Text("DrawCall Count: %d", GET_SINGLE(DiagnosticsManager)->GetDrawCallCount());
    ImGui::Text("Triangle Count: %d", GET_SINGLE(DiagnosticsManager)->GetTriangleCount());
    ImGui::Text(
        "Objects: Total %u / Visible %u / Culled %u",
        GET_SINGLE(DiagnosticsManager)->GetTotalObjectCount(),
        GET_SINGLE(DiagnosticsManager)->GetVisibleObjectCount(),
        GET_SINGLE(DiagnosticsManager)->GetCulledObjectCount());

    ImGui::Text("RenderGraph Pass: %u", GET_SINGLE(DiagnosticsManager)->GetPassCount());
    ImGui::Text("RenderGraph Barrier: %u", GET_SINGLE(DiagnosticsManager)->GetBarrierCount());

    ShowRenderThreadCount();

    bool cullingEnabled = Camera::IsFrustumCullingEnabled();
    if (ImGui::Checkbox("Frustum Culling", &cullingEnabled))
    {
        Camera::SetFrustumCullingEnabled(cullingEnabled);
    }


    bool instancingEnabled = GET_SINGLE(InstancingManager)->IsEnabled();

    if (ImGui::Checkbox("Instancing", &instancingEnabled))
    {
        GET_SINGLE(InstancingManager)->SetEnabled(instancingEnabled);
    }
}

void ImGuiManager::ShowRenderThreadCount()
{
    auto executor = GDEngine->GetRenderGraphExecutor();
    int workerCount = static_cast<int>(executor->GetWorkerCount());

    if (ImGui::RadioButton("Worker 1", workerCount == 1))
        executor->SetWorkerCount(1);

    ImGui::SameLine();

    if (ImGui::RadioButton("Worker 2", workerCount == 2))
        executor->SetWorkerCount(2);

    ImGui::SameLine();

    if (ImGui::RadioButton("Worker 4", workerCount == 4))
        executor->SetWorkerCount(4);

    ImGui::SameLine();

    if (ImGui::RadioButton("Worker 8", workerCount == 8))
        executor->SetWorkerCount(8);

    ImGui::Text("Worker Count: %u", executor->GetWorkerCount());
}
