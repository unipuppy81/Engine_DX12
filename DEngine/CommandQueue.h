#pragma once

#include "DeferredReleaseQueue.h"
#include "UploadAllocator.h"
#include "CommandContext.h"

class SwapChain;
class DescriptorHeap;
class DescriptorAllocator;
class DescriptorAllocation;

struct FrameResource
{
	ComPtr<ID3D12CommandAllocator> cmdAllocator;
	UploadAllocator uploadAllocator;

	uint64 fenceValue = 0;

	// Multi-threaded Command Recording
	vector<CommandContext> commandContexts;
	atomic<uint32> commandContextIndex = 0;
};


// ************************
// GraphicsCommandQueue
// ************************

class GraphicsCommandQueue
{
public:
	~GraphicsCommandQueue();

	void Init(ComPtr<ID3D12Device> device, shared_ptr<SwapChain> swapChain);
	void WaitSync();
	void WaitForFence(uint64 fenceValue);
	uint64 Signal();

	void BeginInitCommands();
	void EndInitCommands();

	void RenderBegin();
	void RenderEnd();

	void FlushResourceCommandQueue();
	void FlushDeferredReleases();

	ComPtr<ID3D12CommandQueue>			GetCmdQueue() { return _cmdQueue; }
	ComPtr<ID3D12GraphicsCommandList>	GetGraphicsCmdList() { return _cmdList; }
	ComPtr<ID3D12GraphicsCommandList>	GetResCmdList() { return _resCmdList; }
	UploadAllocator& GetCurrentUploadAllocator() 
	{ 
		assert(_currentFrame != nullptr);
		return _currentFrame->uploadAllocator; 
	}

	template<typename T>
	void DeferredRelease(ComPtr<T>& resource)
	{
		_deferredReleaseQueue.Enqueue(resource);
	}

	void DeferredFreeDescriptor(shared_ptr<DescriptorAllocator> allocator, DescriptorAllocation& allocation);

	CommandContext* AcquireCommandContext();
	void ExecuteCommandLists(const vector<ID3D12CommandList*>& commandList);

private:
	ComPtr<ID3D12CommandQueue>			_cmdQueue;

	ComPtr<ID3D12CommandAllocator>		_resCmdAlloc;
	ComPtr<ID3D12GraphicsCommandList>	_resCmdList;
	ComPtr<ID3D12GraphicsCommandList>	_cmdList;

	ComPtr<ID3D12Fence>					_fence;
	uint64								_fenceValue = 0;
	HANDLE								_fenceEvent = INVALID_HANDLE_VALUE;

	shared_ptr<SwapChain>				_swapChain;

	array<FrameResource, 2 >			_frames;
	uint32								_currentFrameIndex = 0;
	FrameResource*						_currentFrame = nullptr;

	DeferredReleaseQueue				_deferredReleaseQueue;

};

// ************************
// ComputeCommandQueue
// ************************

class ComputeCommandQueue
{
public:
	~ComputeCommandQueue();

	void Init(ComPtr<ID3D12Device> device);
	void WaitSync();
	void FlushComputeCommandQueue();

	ComPtr<ID3D12CommandQueue> GetCmdQueue() { return _cmdQueue; }
	ComPtr<ID3D12GraphicsCommandList> GetComputeCmdList() { return _cmdList; }

private:
	ComPtr<ID3D12CommandQueue>			_cmdQueue;
	ComPtr<ID3D12CommandAllocator>		_cmdAlloc;
	ComPtr<ID3D12GraphicsCommandList>	_cmdList;

	ComPtr<ID3D12Fence>					_fence;
	uint32								_fenceValue = 0;
	HANDLE								_fenceEvent = INVALID_HANDLE_VALUE;
};


