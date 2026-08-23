#include "pch.h"
#include "CommandQueue.h"
#include "SwapChain.h"
#include "DEngine.h"


#pragma region Graphics CommandQueue

GraphicsCommandQueue::~GraphicsCommandQueue()
{
	if (_cmdQueue && _fence)
		FlushDeferredReleases();

	if (_fenceEvent != nullptr &&
		_fenceEvent != INVALID_HANDLE_VALUE)
	{
		::CloseHandle(_fenceEvent);
		_fenceEvent = nullptr;
	}
}

void GraphicsCommandQueue::Init(ComPtr<ID3D12Device> device, shared_ptr<SwapChain> swapChain)
{
	_swapChain = swapChain;

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	for (FrameResource& frame : _frames)
	{
		device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.cmdAllocator));
		frame.uploadAllocator.Init(device.Get(), 8 * 1024 * 1024); // 8MB
	}

	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _frames[0].cmdAllocator.Get(), nullptr, IID_PPV_ARGS(&_cmdList));

	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_cmdQueue));
	// device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAlloc));
	
	// GPU 하나인 시스템 = 0
	// 초기 상태 (그리기 명령을 nullptr 로 초기화)
	// device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&_cmdList));

	_cmdList->Close();

	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_resCmdAlloc));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _resCmdAlloc.Get(), nullptr, IID_PPV_ARGS(&_resCmdList));

	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
	_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
	_currentFrameIndex = 0;
	_currentFrame = &_frames[0];
}

void GraphicsCommandQueue::WaitSync()
{
	_fenceValue++;
	_cmdQueue->Signal(_fence.Get(), _fenceValue);
	
	if (_fence->GetCompletedValue() < _fenceValue) 
	{
		_fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
	
		// CPU는 GPU 에 명령 전달 후 _fenceEvent 이벤트가 활성화 될 떄까지 기다린다.
		// CPU가 기다리는 게 좋은 코드는 아님
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void GraphicsCommandQueue::WaitForFence(uint64 fenceValue)
{
	if (fenceValue == 0 || _fence->GetCompletedValue() >= fenceValue)
		return;

	HRESULT hr = _fence->SetEventOnCompletion(fenceValue, _fenceEvent);
	assert(SUCCEEDED(hr));

	::WaitForSingleObject(_fenceEvent, INFINITE);
}

uint64 GraphicsCommandQueue::Signal()
{
	const uint64 fenceValue = ++_fenceValue;

	HRESULT hr = _cmdQueue->Signal(_fence.Get(), fenceValue);
	assert(SUCCEEDED(hr));

	return fenceValue;
}

void GraphicsCommandQueue::BeginInitCommands()
{
	_currentFrameIndex = 0;
	_currentFrame = &_frames[0];

	WaitForFence(_currentFrame->fenceValue);
	
	_currentFrame->cmdAllocator->Reset();
	_currentFrame->uploadAllocator.Reset();
	_cmdList->Reset(_currentFrame->cmdAllocator.Get(), nullptr);
	
	GDEngine->GetGraphicsDescHeap()->Clear(0);
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::IBL_CUBEMAP)->Clear(0);

	_cmdList->SetGraphicsRootSignature(GRAPHICS_ROOT_SIGNATURE.Get());

	ID3D12DescriptorHeap* heap = GDEngine->GetGraphicsDescHeap()->GetDescriptorHeap().Get();
	_cmdList->SetDescriptorHeaps(1, &heap);
}

void GraphicsCommandQueue::EndInitCommands()
{
	_cmdList->Close();

	ID3D12CommandList* lists[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(1, lists);

	WaitSync();
}

void GraphicsCommandQueue::RenderBegin()
{
	_currentFrameIndex = _swapChain->GetBackBufferIndex();
	_currentFrame = &_frames[_currentFrameIndex];

	// 이 FrameResource를 GPU가 다 썼는지 확인
	WaitForFence(_currentFrame->fenceValue);
	
	// Fence 완료된 리소스 해제
	_deferredReleaseQueue.Process(_fence->GetCompletedValue());

	// 이번 프레임에서 재사용
	_currentFrame->cmdAllocator->Reset();
	_currentFrame->uploadAllocator.Reset();
	_cmdList->Reset(_currentFrame->cmdAllocator.Get(), nullptr);


	_cmdList->SetGraphicsRootSignature(GRAPHICS_ROOT_SIGNATURE.Get());
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::GLOBAL)->Clear(_currentFrameIndex);
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::TRANSFORM)->Clear(_currentFrameIndex);
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::MATERIAL)->Clear(_currentFrameIndex);
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::MATERIAL_PBR)->Clear(_currentFrameIndex);
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::IBL_CUBEMAP)->Clear(_currentFrameIndex);
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::CAMERA)->Clear(_currentFrameIndex);

	GDEngine->GetGraphicsDescHeap()->Clear(_currentFrameIndex);

	ID3D12DescriptorHeap* descHeap = GDEngine->GetGraphicsDescHeap()->GetDescriptorHeap().Get();
	_cmdList->SetDescriptorHeaps(1, &descHeap); 	// 무거운 연산이므로 프레임당 한 번만 하는게 좋음
}

void GraphicsCommandQueue::RenderEnd()
{
	_cmdList->Close();	

	ID3D12CommandList* cmdListArr[] = { _cmdList.Get() };

	// 1. 명령 제출
	_cmdQueue->ExecuteCommandLists(1, cmdListArr);

	// 2. Present
	_swapChain->Present();
	
	// 3. 제출된 작업 이후에 Fence 삽입
	const uint64 fenceValue = Signal();
	_currentFrame->fenceValue = fenceValue;

	// 4. 이번 프레임 삭제 요청을 Fence와 연결
	_deferredReleaseQueue.Commit(fenceValue);

	_swapChain->SwapIndex();
}

void GraphicsCommandQueue::FlushResourceCommandQueue()
{
	_resCmdList->Close();

	ID3D12CommandList* cmdListArr[] = { _resCmdList.Get() };
	_cmdQueue->ExecuteCommandLists(_countof(cmdListArr), cmdListArr);

	WaitSync();

	_resCmdAlloc->Reset();
	_resCmdList->Reset(_resCmdAlloc.Get(), nullptr);
}

void GraphicsCommandQueue::FlushDeferredReleases()
{
	// 지금까지 Queue에 제출된 작업 뒤에 Fence 삽입
	const uint64 fenceValue = Signal();

	// 아직 RenderEnd에 연결되지 않은 해제 요청까지 Fence에 연결
	_deferredReleaseQueue.Commit(fenceValue);

	// GPU 완료 대기
	WaitForFence(fenceValue);

	// 실제 ComPtr 해제
	_deferredReleaseQueue.Process(_fence->GetCompletedValue());
}

void GraphicsCommandQueue::DeferredFreeDescriptor(shared_ptr<DescriptorAllocator> allocator, DescriptorAllocation& allocation)
{
	if (!allocation.IsValid())
		return;

	DescriptorAllocation releasedAllocation = allocation;
	allocation.Reset();

	_deferredReleaseQueue.EnqueueCallback(
		[allocator, releasedAllocation]() mutable
		{
			allocator->Free(releasedAllocation);
		});
}
#pragma endregion

#pragma region Compute CommandQueue

ComputeCommandQueue::~ComputeCommandQueue()
{
	::CloseHandle(_fenceEvent);
}

void ComputeCommandQueue::Init(ComPtr<ID3D12Device> device)
{
	D3D12_COMMAND_QUEUE_DESC computeQueueDesc = {};
	computeQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	computeQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	device->CreateCommandQueue(&computeQueueDesc, IID_PPV_ARGS(&_cmdQueue));
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&_cmdAlloc));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, _cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&_cmdList));
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));

	_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void ComputeCommandQueue::WaitSync()
{
	_fenceValue++;

	_cmdQueue->Signal(_fence.Get(), _fenceValue);

	if (_fence->GetCompletedValue() < _fenceValue)
	{
		_fence->SetEventOnCompletion(_fenceValue, _fenceEvent);
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void ComputeCommandQueue::FlushComputeCommandQueue()
{
	_cmdList->Close();

	ID3D12CommandList* cmdListArr[] = { _cmdList.Get() };
	auto t = _countof(cmdListArr);
	_cmdQueue->ExecuteCommandLists(_countof(cmdListArr), cmdListArr);

	WaitSync();

	_cmdAlloc->Reset();
	_cmdList->Reset(_cmdAlloc.Get(), nullptr);

	COMPUTE_CMD_LIST->SetComputeRootSignature(COMPUTE_ROOT_SIGNATURE.Get());
}
#pragma endregion