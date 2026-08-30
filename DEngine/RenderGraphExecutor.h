#pragma once

#include "ThreadPool.h"

class RenderGraphPass;
class RenderGraph;
class GraphicsCommandQueue;

class RenderGraphExecutor
{
public:
    void Init(GraphicsCommandQueue* graphicsQeueu);
    void Shutdown();

    void Record(RenderGraph& renderGraph);
    void Submit();

    void SetWorkerCount(uint32 count);
    uint32 GetWorkerCount() const { return _workerCount; }
private:
    void RecordPass(RenderGraphPass* pass, uint32 orderIndex, const vector<D3D12_RESOURCE_BARRIER>& barriers);

private:
    ThreadPool _threadPool;
    GraphicsCommandQueue* _graphicsQueue = nullptr;

    vector<ID3D12CommandList*> _recordedCommandLists;

    uint32 _workerCount = 1;
};

