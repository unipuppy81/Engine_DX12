#include "pch.h"
#include "ThreadCommandContext.h"

thread_local ID3D12GraphicsCommandList*
ThreadCommandContext::_graphicsCmdList = nullptr;

thread_local ID3D12GraphicsCommandList*
ThreadCommandContext::_computeCmdList = nullptr;

void ThreadCommandContext::SetGraphicsCommandList(ID3D12GraphicsCommandList* cmdList)
{
    _graphicsCmdList = cmdList;
}

ID3D12GraphicsCommandList* ThreadCommandContext::GetGraphicsCommandList()
{
    assert(_graphicsCmdList != nullptr);
    return _graphicsCmdList;
}

void ThreadCommandContext::ClearGraphicsCommandList()
{
    _graphicsCmdList = nullptr;
}

void ThreadCommandContext::SetComputeCommandList(ID3D12GraphicsCommandList* cmdList)
{
    _computeCmdList = cmdList;
}

ID3D12GraphicsCommandList* ThreadCommandContext::GetComputeCommandList()
{
    assert(_computeCmdList != nullptr);
    return _computeCmdList;
}

void ThreadCommandContext::ClearComputeCommandList()
{
    _computeCmdList = nullptr;
}