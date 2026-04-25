#include "pch.h"
#include "CommandQueue.h"
#include "SwapChain.h"
#include "DescriptorHeap.h"

CommandQueue::~CommandQueue()
{
	::CloseHandle(_fenceEvent);
}

void CommandQueue::Init(ComPtr<ID3D12Device> device, shared_ptr<SwapChain> swapChain, shared_ptr<DescriptorHeap> descHeap)
{
	_swapChain = swapChain;
	_descHeap = descHeap;

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_cmdQueue));

	// D3D12_COMMAND_LIST_TYPE_DIRECT : GPU 가 직접 실행하는 명려여 목록
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAlloc));

	// GPU 하나인 시스템 = 0
	// 초기 상태 (그리기 명령을 nullptr 로 초기화)
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&_cmdList));

	// CommandList 는 Open / Close
	// Open 상태에서 Command 넣다가 Close 한 다음 제출하는 개념
	_cmdList->Close();

	// CreateForce
	// CPU-GPU 동기화 수단
	device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
	_fenceEvent = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

void CommandQueue::WaitSync()
{
	// Advance the fence value to mark command up to this fence point. 
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


void CommandQueue::RenderBegin(const D3D12_VIEWPORT* vp, const D3D12_RECT* rect)
{
	_cmdAlloc->Reset();
	_cmdList->Reset(_cmdAlloc.Get(), nullptr);


	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		_swapChain->GetCurrentBackBufferResource().Get(),
		D3D12_RESOURCE_STATE_PRESENT,			// 현재 출력
		D3D12_RESOURCE_STATE_RENDER_TARGET);	// 백 버퍼

	_cmdList->ResourceBarrier(1, &barrier);

	// _cmdList->Reset() 했으니 다시 설정
	_cmdList->RSSetViewports(1, vp);
	_cmdList->RSSetScissorRects(1, rect);

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferView = _descHeap->GetBackBufferView();
	_cmdList->ClearRenderTargetView(backBufferView, Colors::LightSteelBlue, 0, nullptr);
	_cmdList->OMSetRenderTargets(1, &backBufferView, FALSE, nullptr);
}

void CommandQueue::RenderEnd()
{
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		_swapChain->GetCurrentBackBufferResource().Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, // 백 버퍼
		D3D12_RESOURCE_STATE_PRESENT);		// 현재 출력

	_cmdList->ResourceBarrier(1, &barrier);
	_cmdList->Close();	// 이전까지는 커맨드 리스트에 밀어넣는 과정

	// 커맨드 리스트 실행 = 그리기 요청
	ID3D12CommandList* cmdListArr[] = { _cmdList.Get() };
	_cmdQueue->ExecuteCommandLists(_countof(cmdListArr), cmdListArr);
	_swapChain->Present();

	WaitSync();

	_swapChain->SwapIndex();
}