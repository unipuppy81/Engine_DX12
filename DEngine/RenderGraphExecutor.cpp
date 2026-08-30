#include "pch.h"
#include "RenderGraphExecutor.h"
#include "CommandQueue.h"
#include "CommandContext.h"
#include "RenderGraphPass.h"
#include "RenderGraph.h"
#include "DEngine.h"
#include "ThreadCommandContext.h"
#include "DiagnosticsManager.h"

void RenderGraphExecutor::Init(GraphicsCommandQueue* graphicsQueue)
{
    _graphicsQueue = graphicsQueue;

    //_threadPool.Init(1);
    //return;

    uint32 threadCount = thread::hardware_concurrency();

    if (threadCount > 1)
        --threadCount;

    if (threadCount == 0)
        threadCount = 1;

    _workerCount = threadCount;
    _threadPool.Init(_workerCount);
}

void RenderGraphExecutor::Shutdown()
{
    _threadPool.Shutdown();
}

void RenderGraphExecutor::Record(RenderGraph& renderGraph)
{
    auto start = chrono::high_resolution_clock::now();

    const auto& executionOrder = renderGraph.GetExecutionOrder();

	_recordedCommandLists.clear();
	_recordedCommandLists.resize(executionOrder.size());

    for (uint32 orderIndex = 0; orderIndex < executionOrder.size(); ++orderIndex)
    {
        uint32 passIndex = executionOrder[orderIndex];

        RenderGraphPass* pass = renderGraph.GetPass(passIndex);
        auto& barriers = renderGraph.GetPassBarriers(passIndex);
        _threadPool.Enqueue(
            [this, pass, orderIndex, &barriers]()
            {
                RecordPass(pass, orderIndex, barriers);
            });
    }

	// 모든 CommandList 기록 완료까지 대기
	_threadPool.WaitIdle();


    auto end = chrono::high_resolution_clock::now();
    float recordMs = chrono::duration<float, milli>(end - start).count();
    GET_SINGLE(DiagnosticsManager)->SetCommandRecordMs(recordMs);
}

void RenderGraphExecutor::Submit()
{
    _graphicsQueue->ExecuteCommandLists(_recordedCommandLists);
}

void RenderGraphExecutor::SetWorkerCount(uint32 count)
{
    if (count == _workerCount)
        return;

    _threadPool.Shutdown();

    _workerCount = count;
    _threadPool.Init(_workerCount);
}

void RenderGraphExecutor::RecordPass(RenderGraphPass* pass, uint32 orderIndex, const vector<D3D12_RESOURCE_BARRIER>& barriers)
{
	CommandContext* context = _graphicsQueue->AcquireCommandContext();
	ID3D12GraphicsCommandList* cmdList = context->GetCommandList();

    // GPU Frame Time 시작
    // RenderGraph 실행 순서상 첫 번째 CommandList에 Timestamp 기록
    if (orderIndex == 0)
    {
        uint32 frameIndex = _graphicsQueue->GetCurrentFrameIndex();

        GET_SINGLE(DiagnosticsManager)->BeginGpuTimer(frameIndex, cmdList);
    }

    // 이 Worker Thread의 GRAPHICS_CMD_LIST 지정
    ThreadCommandContext::SetGraphicsCommandList(cmdList);
    
    GDEngine->GetGraphicsDescHeap()->BeginThreadRecording();

    // RootSignature
    cmdList->SetGraphicsRootSignature(GRAPHICS_ROOT_SIGNATURE.Get());

    // b0
    auto globalCB = GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::GLOBAL);
    cmdList->SetGraphicsRootConstantBufferView(0, GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::GLOBAL)->GetGlobalGpuAddress());


    // Descriptor Heap
    auto graphicsDescHeap = GDEngine->GetGraphicsDescHeap();
    ID3D12DescriptorHeap* heap = GDEngine->GetGraphicsDescHeap()->GetDescriptorHeap().Get();
    cmdList->SetDescriptorHeaps(1, &heap);

	// barrier
	if (!barriers.empty())
	{
		cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
	}

    // 실제 Pass 기록
	pass->Execute(cmdList);
    ThreadCommandContext::ClearGraphicsCommandList();
	context->Close();

	_recordedCommandLists[orderIndex] = cmdList;
}