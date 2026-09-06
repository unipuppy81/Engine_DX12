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

/*
void RenderGraphExecutor::Record(RenderGraph& renderGraph)
{
    auto start = chrono::high_resolution_clock::now();

    const auto& executionOrder = renderGraph.GetExecutionOrder();

	_recordedCommandLists.clear();
	_recordedCommandLists.resize(executionOrder.size());

    //test
    _passRecordMs.clear();
    _passRecordMs.resize(executionOrder.size());

    _passThreadIds.clear();
    _passThreadIds.resize(executionOrder.size());

    _jobWaitMs.clear();
    _jobWaitMs.resize(executionOrder.size());

    for (uint32 orderIndex = 0; orderIndex < executionOrder.size(); ++orderIndex)
    {
        uint32 passIndex = executionOrder[orderIndex];

        RenderGraphPass* pass = renderGraph.GetPass(passIndex);
        auto& barriers = renderGraph.GetPassBarriers(passIndex);

        auto enqueueTime = chrono::steady_clock::now();

        _threadPool.Enqueue(
            [this, pass, orderIndex, &barriers, enqueueTime]()
            {
                auto jobStart = chrono::steady_clock::now();

                _jobWaitMs[orderIndex] = chrono::duration<float, milli>(jobStart - enqueueTime).count();


                RecordPass(pass, orderIndex, barriers);
            });
    }

	// 모든 CommandList 기록 완료까지 대기
	_threadPool.WaitIdle();

    for (uint32 i = 0; i < _jobWaitMs.size(); ++i)
    {
        DX_LOG(
            L"Pass=" << i
            << L" JobWait=" << _jobWaitMs[i]
            << L" ms"
            << L" Record=" << _passRecordMs[i]
            << L" ms");
    }

    auto end = chrono::high_resolution_clock::now();
    float recordMs = chrono::duration<float, milli>(end - start).count();
    GET_SINGLE(DiagnosticsManager)->SetCommandRecordMs(recordMs);

    // 측정 종료 후 로그
    DX_LOG(L"===== Worker " << _workerCount
        << L" / Total " << recordMs << L" ms =====");

    for (uint32 i = 0; i < _passRecordMs.size(); ++i)
    {
        DX_LOG(
            L"Pass=" << i
            << L" Thread=" << _passThreadIds[i]
            << L" Time=" << _passRecordMs[i] << L" ms");
    }
}
*/

void RenderGraphExecutor::Record(RenderGraph& renderGraph)
{
    const auto& executionOrder = renderGraph.GetExecutionOrder();

    _recordedCommandLists.clear();
    _recordedCommandLists.resize(executionOrder.size());

    _passRecordMs.clear();
    _passRecordMs.resize(executionOrder.size());

    _passThreadIds.clear();
    _passThreadIds.resize(executionOrder.size());

    _jobWaitMs.clear();
    _jobWaitMs.resize(executionOrder.size());

    _acquireMs.resize(executionOrder.size());
    _setupMs.resize(executionOrder.size());
    _barrierMs.resize(executionOrder.size());
    _executeMs.resize(executionOrder.size());
    _closeMs.resize(executionOrder.size());

    // Command Recording 전체 측정 시작
    auto start = chrono::steady_clock::now();

    for (uint32 orderIndex = 0; orderIndex < executionOrder.size(); ++orderIndex)
    {
        uint32 passIndex = executionOrder[orderIndex];

        RenderGraphPass* pass = renderGraph.GetPass(passIndex);
        auto& barriers = renderGraph.GetPassBarriers(passIndex);

        auto enqueueTime = chrono::steady_clock::now();

        _threadPool.Enqueue(
            [this, pass, orderIndex, &barriers, enqueueTime]()
            {
                auto jobStart = chrono::steady_clock::now();

                _jobWaitMs[orderIndex] = chrono::duration<float, milli>(jobStart - enqueueTime).count();

                RecordPass(pass, orderIndex, barriers);
            });


        uint64 waitNs = _graphicsQueue->GetCurrentUploadAllocator().ConsumeLockWaitNs();
        uint64 count = _graphicsQueue->GetCurrentUploadAllocator().ConsumeAllocationCount();

        double totalWaitMs = waitNs / 1000000.0;
        double avgWaitUs =
            count > 0 ? (waitNs / 1000.0) / count : 0.0;

        DX_LOG(
            L"UploadAllocator Count=" << count
            << L" LockWait=" << totalWaitMs
            << L" ms"
            << L" AvgWait=" << avgWaitUs
            << L" us");
    }

    // 모든 Pass Recording 완료
    _threadPool.WaitIdle();

    // 여기서 측정 종료
    auto end = chrono::steady_clock::now();

    float recordMs =
        chrono::duration<float, milli>(end - start).count();

    GET_SINGLE(DiagnosticsManager)->SetCommandRecordMs(recordMs);


    // ===== 여기부터는 측정 끝났으므로 로그 =====

    DX_LOG(
        L"===== Worker "
        << _workerCount
        << L" / Total "
        << recordMs
        << L" ms =====");

    for (uint32 i = 0; i < _passRecordMs.size(); ++i)
    {
        DX_LOG(
            L"Pass=" << i
            << L" Thread=" << _passThreadIds[i]
            << L" JobWait=" << _jobWaitMs[i] << L" ms"
            << L" Record=" << _passRecordMs[i] << L" ms");
    }

    for (uint32 i = 0; i < _passRecordMs.size(); ++i)
    {
        DX_LOG(
            L"Pass=" << i
            << L" JobWait=" << _jobWaitMs[i]
            << L" Acquire=" << _acquireMs[i]
            << L" Setup=" << _setupMs[i]
            << L" Barrier=" << _barrierMs[i]
            << L" Execute=" << _executeMs[i]
            << L" Close=" << _closeMs[i]
            << L" Total=" << _passRecordMs[i]
            << L" ms");
    }


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

void RenderGraphExecutor::RecordPass(
    RenderGraphPass* pass,
    uint32 orderIndex,
    const vector<D3D12_RESOURCE_BARRIER>& barriers)
{
    auto start = chrono::steady_clock::now();
    auto threadId = hash<thread::id>{}(this_thread::get_id());


    // =========================
    // 1. Acquire
    // =========================
    auto t0 = chrono::steady_clock::now();

    CommandContext* context = _graphicsQueue->AcquireCommandContext();
    ID3D12GraphicsCommandList* cmdList = context->GetCommandList();

    auto t1 = chrono::steady_clock::now();


    // =========================
    // 2. CommandList Setup
    // =========================

    if (orderIndex == 0)
    {
        uint32 frameIndex = _graphicsQueue->GetCurrentFrameIndex();
        GET_SINGLE(DiagnosticsManager)->BeginGpuTimer(frameIndex, cmdList);
    }

    ThreadCommandContext::SetGraphicsCommandList(cmdList);

    GDEngine->GetGraphicsDescHeap()->BeginThreadRecording();

    cmdList->SetGraphicsRootSignature(GRAPHICS_ROOT_SIGNATURE.Get());

    auto globalCB =
        GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::GLOBAL);

    cmdList->SetGraphicsRootConstantBufferView(
        0,
        globalCB->GetGlobalGpuAddress());

    auto graphicsDescHeap = GDEngine->GetGraphicsDescHeap();

    ID3D12DescriptorHeap* heap =
        graphicsDescHeap->GetDescriptorHeap().Get();

    cmdList->SetDescriptorHeaps(1, &heap);

    auto t2 = chrono::steady_clock::now();


    // =========================
    // 3. Barrier
    // =========================

    if (!barriers.empty())
    {
        cmdList->ResourceBarrier(
            static_cast<UINT>(barriers.size()),
            barriers.data());
    }

    auto t3 = chrono::steady_clock::now();


    // =========================
    // 4. Pass Execute
    // =========================

    pass->Execute(cmdList);

    auto t4 = chrono::steady_clock::now();


    // =========================
    // 5. Close
    // =========================

    ThreadCommandContext::ClearGraphicsCommandList();

    context->Close();

    _recordedCommandLists[orderIndex] = cmdList;

    auto t5 = chrono::steady_clock::now();


    // =========================
    // 결과 저장
    // =========================

    _acquireMs[orderIndex] =
        chrono::duration<float, milli>(t1 - t0).count();

    _setupMs[orderIndex] =
        chrono::duration<float, milli>(t2 - t1).count();

    _barrierMs[orderIndex] =
        chrono::duration<float, milli>(t3 - t2).count();

    _executeMs[orderIndex] =
        chrono::duration<float, milli>(t4 - t3).count();

    _closeMs[orderIndex] =
        chrono::duration<float, milli>(t5 - t4).count();

    _passRecordMs[orderIndex] =
        chrono::duration<float, milli>(t5 - start).count();

    _passThreadIds[orderIndex] = threadId;
}

/*
void RenderGraphExecutor::RecordPass(RenderGraphPass* pass, uint32 orderIndex, const vector<D3D12_RESOURCE_BARRIER>& barriers)
{
    auto start = chrono::high_resolution_clock::now();
    auto threadId = hash<thread::id>{}(this_thread::get_id());


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


    auto end = chrono::high_resolution_clock::now();
    float ms = chrono::duration<float, milli>(end - start).count();

    _passRecordMs[orderIndex] = ms;
    _passThreadIds[orderIndex] = threadId;
}
*/