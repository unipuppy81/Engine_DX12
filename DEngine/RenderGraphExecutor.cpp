#include "pch.h"
#include "RenderGraphExecutor.h"
#include "CommandQueue.h"
#include "CommandContext.h"
#include "RenderGraphPass.h"
#include "RenderGraph.h"
#include "DEngine.h"

void RenderGraphExecutor::Init(GraphicsCommandQueue* graphicsQueue)
{
    _graphicsQueue = graphicsQueue;

    uint32 threadCount = thread::hardware_concurrency();

    if (threadCount > 1)
        --threadCount;

    if (threadCount == 0)
        threadCount = 1;

    _threadPool.Init(threadCount);
}

void RenderGraphExecutor::Shutdown()
{
    _threadPool.Shutdown();
}

void RenderGraphExecutor::Record(RenderGraph& renderGraph)
{
	const auto& executionOrder = renderGraph.GetExecutionOrder();

	_recordedCommandLists.clear();
	_recordedCommandLists.resize(executionOrder.size());

    for (uint32 orderIndex = 0; orderIndex < executionOrder.size(); ++orderIndex)
    {
        uint32 passIndex = executionOrder[orderIndex];

        RenderGraphPass* pass = renderGraph.GetPass(passIndex);

        const auto& barriers = renderGraph.GetPassBarriers(passIndex);

        _threadPool.Enqueue(
            [this, pass, orderIndex, &barriers]()
            {
                RecordPass(
                    pass,
                    orderIndex,
                    barriers);
            });
    }

	// 모든 CommandList 기록 완료까지 대기
	_threadPool.WaitIdle();
}

void RenderGraphExecutor::Submit()
{
    _graphicsQueue->ExecuteCommandLists(_recordedCommandLists);
}

void RenderGraphExecutor::RecordPass(RenderGraphPass* pass, uint32 orderIndex, const vector<D3D12_RESOURCE_BARRIER>& barriers)
{
	CommandContext* context = _graphicsQueue->AcquireCommandContext();
	ID3D12GraphicsCommandList* cmdList = context->GetCommandList();

    // 각 CommandList마다 설정 필요
    cmdList->SetGraphicsRootSignature(GRAPHICS_ROOT_SIGNATURE.Get());

    ID3D12DescriptorHeap* descHeap = GDEngine->GetGraphicsDescHeap()->GetDescriptorHeap().Get();
    cmdList->SetDescriptorHeaps(1, &descHeap);

	// RenderGraph Compile 단계에서 이미 계산된 Barrier 기록
	if (!barriers.empty())
	{
		cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
	}

    // 실제 Pass 기록
	pass->Execute(cmdList);

	context->Close();

	_recordedCommandLists[orderIndex] = cmdList;
}