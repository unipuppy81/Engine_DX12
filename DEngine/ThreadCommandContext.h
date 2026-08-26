#pragma once
class ThreadCommandContext
{
public:
    static void SetGraphicsCommandList(ID3D12GraphicsCommandList* cmdList);
    static ID3D12GraphicsCommandList* GetGraphicsCommandList();
    static void ClearGraphicsCommandList();

    static void SetComputeCommandList(ID3D12GraphicsCommandList* cmdList);
    static ID3D12GraphicsCommandList* GetComputeCommandList();
    static void ClearComputeCommandList();

private:
    static thread_local ID3D12GraphicsCommandList* _graphicsCmdList;
    static thread_local ID3D12GraphicsCommandList* _computeCmdList;
};

