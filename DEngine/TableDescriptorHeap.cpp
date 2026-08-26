#include "pch.h"
#include "TableDescriptorHeap.h"
#include "DEngine.h"

thread_local uint32 GraphicsDescriptorHeap::_threadGroupIndex = GraphicsDescriptorHeap::INVALID_GROUP_INDEX;

#pragma region Graphics DescriptorHeap

// ************************
// GraphicsDescriptorHeap
// ************************

void GraphicsDescriptorHeap::Init(uint32 count)
{
	_groupCountPerFrame = count;
	_totalGroupCount = count * FRAME_COUNT;

	const uint32 descriptorsPerGroup = CBV_SRV_REGISTER_COUNT - 1; // b0 는 전역

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = _totalGroupCount * descriptorsPerGroup;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	HRESULT hr = DEVICE->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_descHeap));
	assert(SUCCEEDED(hr));

	_handleSize = DEVICE->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_groupSize = _handleSize * descriptorsPerGroup;
}

void GraphicsDescriptorHeap::Clear(uint32 frameIndex)
{
	assert(frameIndex < FRAME_COUNT);

	const uint32 startGroupIndex = frameIndex * _groupCountPerFrame;

	_nextGroupIndex.store(startGroupIndex);
	_frameEndGroupIndex = startGroupIndex + _groupCountPerFrame;
	_threadGroupIndex = INVALID_GROUP_INDEX;
}

void GraphicsDescriptorHeap::SetCBV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, CBV_REGISTER reg)
{
	D3D12_CPU_DESCRIPTOR_HANDLE destHandle = GetCPUHandle(reg);

	uint32 destRange = 1;
	uint32 srcRange = 1;
	DEVICE->CopyDescriptors(1, &destHandle, &destRange, 1, &srcHandle, &srcRange, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void GraphicsDescriptorHeap::SetSRV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, SRV_REGISTER reg)
{
	D3D12_CPU_DESCRIPTOR_HANDLE destHandle = GetCPUHandle(reg);

	uint32 destRange = 1;
	uint32 srcRange = 1;
	DEVICE->CopyDescriptors(1, &destHandle, &destRange, 1, &srcHandle, &srcRange, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void GraphicsDescriptorHeap::CommitTable()
{
	assert(_threadGroupIndex != INVALID_GROUP_INDEX);

	ID3D12GraphicsCommandList* cmdList = GRAPHICS_CMD_LIST;
	DX_LOG(L"CMDLISTTEST DESC CMD = " << GRAPHICS_CMD_LIST);
	D3D12_GPU_DESCRIPTOR_HANDLE handle = _descHeap->GetGPUDescriptorHandleForHeapStart();

	handle.ptr += static_cast<UINT64>(_threadGroupIndex) * _groupSize;

	cmdList->SetGraphicsRootDescriptorTable(1, handle);

	_threadGroupIndex = INVALID_GROUP_INDEX;
}

void GraphicsDescriptorHeap::BeginThreadRecording()
{
	_threadGroupIndex = INVALID_GROUP_INDEX;
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDescriptorHeap::GetCPUHandle(CBV_REGISTER reg)
{
	return GetCPUHandle(static_cast<uint32>(reg));
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDescriptorHeap::GetCPUHandle(SRV_REGISTER reg)
{
	return GetCPUHandle(static_cast<uint32>(reg));
}

D3D12_CPU_DESCRIPTOR_HANDLE GraphicsDescriptorHeap::GetCPUHandle(uint8 reg)
{
	assert(reg > 0);
	assert(reg < CBV_SRV_REGISTER_COUNT);

	if (_threadGroupIndex == INVALID_GROUP_INDEX)
	{
		_threadGroupIndex = _nextGroupIndex.fetch_add(1);

		assert(_threadGroupIndex < _frameEndGroupIndex);
	}


	D3D12_CPU_DESCRIPTOR_HANDLE handle = _descHeap->GetCPUDescriptorHandleForHeapStart();

	handle.ptr += static_cast<SIZE_T>(_threadGroupIndex) * _groupSize;
	handle.ptr += static_cast<SIZE_T>(reg - 1) * _handleSize;

	return handle;
}
#pragma endregion

#pragma region Compute DescriptorHeap

// ************************
// ComputeDescriptorHeap
// ************************

void ComputeDescriptorHeap::Init()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = TOTAL_REGISTER_COUNT;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	DEVICE->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_descHeap));

	_handleSize = DEVICE->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void ComputeDescriptorHeap::SetCBV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, CBV_REGISTER reg)
{
	D3D12_CPU_DESCRIPTOR_HANDLE destHandle = GetCPUHandle(reg);

	uint32 destRange = 1;
	uint32 srcRange = 1;
	DEVICE->CopyDescriptors(1, &destHandle, &destRange, 1, &srcHandle, &srcRange, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void ComputeDescriptorHeap::SetSRV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, SRV_REGISTER reg)
{
	D3D12_CPU_DESCRIPTOR_HANDLE destHandle = GetCPUHandle(reg);

	uint32 destRange = 1;
	uint32 srcRange = 1;
	DEVICE->CopyDescriptors(1, &destHandle, &destRange, 1, &srcHandle, &srcRange, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

void ComputeDescriptorHeap::SetUAV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, UAV_REGISTER reg)
{
	D3D12_CPU_DESCRIPTOR_HANDLE destHandle = GetCPUHandle(reg);

	uint32 destRange = 1;
	uint32 srcRange = 1;
	DEVICE->CopyDescriptors(1, &destHandle, &destRange, 1, &srcHandle, &srcRange, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// TODO : 리소스 상태 변경
}

void ComputeDescriptorHeap::CommitTable()
{
	ID3D12DescriptorHeap* descHeap = _descHeap.Get();
	COMPUTE_CMD_LIST->SetDescriptorHeaps(1, &descHeap);

	D3D12_GPU_DESCRIPTOR_HANDLE handle = descHeap->GetGPUDescriptorHandleForHeapStart();
	COMPUTE_CMD_LIST->SetComputeRootDescriptorTable(0, handle);
}

D3D12_CPU_DESCRIPTOR_HANDLE ComputeDescriptorHeap::GetCPUHandle(CBV_REGISTER reg)
{
	return GetCPUHandle(static_cast<uint8>(reg));
}

D3D12_CPU_DESCRIPTOR_HANDLE ComputeDescriptorHeap::GetCPUHandle(SRV_REGISTER reg)
{
	return GetCPUHandle(static_cast<uint8>(reg));
}

D3D12_CPU_DESCRIPTOR_HANDLE ComputeDescriptorHeap::GetCPUHandle(UAV_REGISTER reg)
{
	return GetCPUHandle(static_cast<uint8>(reg));
}

D3D12_CPU_DESCRIPTOR_HANDLE ComputeDescriptorHeap::GetCPUHandle(uint8 reg)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = _descHeap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += reg * _handleSize;
	return handle;
}

#pragma endregion