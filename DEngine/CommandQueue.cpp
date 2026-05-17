#include "pch.h"
#include "CommandQueue.h"
#include "SwapChain.h"
#include "DEngine.h"

CommandQueue::~CommandQueue()
{
	::CloseHandle(_fenceEvent);
}

void CommandQueue::Init(ComPtr<ID3D12Device> device, shared_ptr<SwapChain> swapChain)
{
	_swapChain = swapChain;

	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&_cmdQueue));
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAlloc));
	// GPU 하나인 시스템 = 0
	// 초기 상태 (그리기 명령을 nullptr 로 초기화)
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAlloc.Get(), nullptr, IID_PPV_ARGS(&_cmdList));

	_cmdList->Close();

	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_resCmdAlloc));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _resCmdAlloc.Get(), nullptr, IID_PPV_ARGS(&_resCmdList));

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
		_swapChain->GetBackRTVBuffer().Get(),
		D3D12_RESOURCE_STATE_PRESENT,			// 현재 출력
		D3D12_RESOURCE_STATE_RENDER_TARGET		// 백 버퍼
	);	

	_cmdList->SetGraphicsRootSignature(ROOT_SIGNATURE.Get());
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::TRANSFORM)->Clear();
	GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::MATERIAL)->Clear();
	GDEngine->GetTableDescHeap()->Clear();

	ID3D12DescriptorHeap* descHeap = GDEngine->GetTableDescHeap()->GetDescriptorHeap().Get();
	_cmdList->SetDescriptorHeaps(1, &descHeap); 	// 무거운 연산이므로 프레임당 한 번만 하는게 좋음

	_cmdList->ResourceBarrier(1, &barrier);

	// _cmdList->Reset() 했으니 다시 설정
	_cmdList->RSSetViewports(1, vp);
	_cmdList->RSSetScissorRects(1, rect);

	D3D12_CPU_DESCRIPTOR_HANDLE backBufferView = _swapChain->GetBackRTV();
	_cmdList->ClearRenderTargetView(backBufferView, Colors::LightSteelBlue, 0, nullptr);

	D3D12_CPU_DESCRIPTOR_HANDLE depthStencilView = GDEngine->GetDepthStencilBuffer()->GetDSVCpuHandle();
	_cmdList->OMSetRenderTargets(1, &backBufferView, FALSE, &depthStencilView);

	_cmdList->ClearDepthStencilView(depthStencilView, D3D12_CLEAR_FLAG_DEPTH, 1.0F, 0, 0, nullptr);
}

void CommandQueue::RenderEnd()
{
	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		_swapChain->GetBackRTVBuffer().Get(),
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

void CommandQueue::FlushResourceCommandQueue()
{
	_resCmdList->Close();

	ID3D12CommandList* cmdListArr[] = { _resCmdList.Get() };
	_cmdQueue->ExecuteCommandLists(_countof(cmdListArr), cmdListArr);

	WaitSync();

	_resCmdAlloc->Reset();
	_resCmdList->Reset(_resCmdAlloc.Get(), nullptr);
}