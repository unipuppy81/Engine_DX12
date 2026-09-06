#include "pch.h"
#include "ConstantBuffer.h"
#include "DEngine.h"

ConstantBuffer::ConstantBuffer()
{
}

ConstantBuffer::~ConstantBuffer()
{
	_cbvHeap.Reset();
}

void ConstantBuffer::Init(CBV_REGISTER reg, uint32 size, uint32 count)
{
	_reg = reg;

	// Constant Buffer는 256-byte alignment
	_elementSize = (size + 255) & ~255;
	_elementCount = count;
	// _currentIndex = 0;
	_currentIndex.store(0);
	
	// 실제 Upload Resource는 만들지 않는다.
	// Descriptor 저장 공간만 생성한다.
	CreateView();
}

void ConstantBuffer::CreateView()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvDesc = {};
	cbvDesc.NumDescriptors = _elementCount;
	cbvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	cbvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	HRESULT hr = DEVICE->CreateDescriptorHeap(&cbvDesc, IID_PPV_ARGS(&_cbvHeap));

	assert(SUCCEEDED(hr));

	// 시작 핸들
	_cpuHandleBegin = _cbvHeap->GetCPUDescriptorHandleForHeapStart(); 

	// 핸들별 간격 GetDescriptorHandleIncrementSize 로 가져와야 함(기계 성능에 따라)
	_handleIncrementSize = DEVICE->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 
}

void ConstantBuffer::Clear(uint32 frameIndex)
{
	// UploadAllocator 자체가 FrameResource별로 나뉘므로
	// ConstantBuffer에서는 frameIndex가 더 이상 필요 없음
	(void)frameIndex;

	_currentIndex = 0;
}

void ConstantBuffer::PushGraphicsData(void* buffer, uint32 size)
{
	assert(_elementSize == ((size + 255) & ~255));
	

	uint32 index = _currentIndex.fetch_add(1);
	if (index == 42 && _reg == CBV_REGISTER::b4)
	{
		int a = 1;

	}

	// DX_LOG(L"CB reg=" << static_cast<int>(_reg)
	// 	<< L" index=" << index
	// 	<< L" count=" << _elementCount);
	assert(index < _elementCount);

	UploadAllocator& allocator = GRAPHICS_CMD_QUEUE->GetCurrentUploadAllocator();

	// 256-byte 단위로 공간 할당
	UploadAllocation allocation = allocator.Allocate(_elementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	// CPU → Upload Heap
	::memcpy(allocation.cpuAddress, buffer, size);

	// 이번 allocation을 가리키는 CBV 생성
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = allocation.gpuAddress;
	cbvDesc.SizeInBytes = _elementSize;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = GetCpuHandle(index);
	DEVICE->CreateConstantBufferView(&cbvDesc, cpuHandle);

	// 기존 GraphicsDescriptorHeap 구조 그대로 사용
	GDEngine->GetGraphicsDescHeap()->SetCBV(cpuHandle, _reg);

	// _currentIndex++;

}

void ConstantBuffer::SetGraphicsGlobalData(void* buffer, uint32 size)
{
	assert(_elementSize == ((size + 255) & ~255));

	UploadAllocator& allocator = GRAPHICS_CMD_QUEUE->GetCurrentUploadAllocator();
	UploadAllocation allocation = allocator.Allocate(_elementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	::memcpy(allocation.cpuAddress, buffer, size);

	// 주소 보관
	_globalGpuAddress = allocation.gpuAddress;

	// b0은 기존대로 Root CBV 사용
	GRAPHICS_CMD_LIST->SetGraphicsRootConstantBufferView(0, allocation.gpuAddress);
}

void ConstantBuffer::PushComputeData(void* buffer, uint32 size)
{
	DX_LOG(L"[PushComputeData] reg=" << static_cast<int>(_reg)
		<< L" index=" << _currentIndex.load()
		<< L" count=" << _elementCount);

	assert(_currentIndex < _elementCount);
	assert(_elementSize == ((size + 255) & ~255));

	UploadAllocator& allocator = GRAPHICS_CMD_QUEUE->GetCurrentUploadAllocator();
	UploadAllocation allocation = allocator.Allocate(_elementSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

	::memcpy(allocation.cpuAddress, buffer, size);

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

	cbvDesc.BufferLocation = allocation.gpuAddress;
	cbvDesc.SizeInBytes = _elementSize;

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = GetCpuHandle(_currentIndex);

	DEVICE->CreateConstantBufferView(&cbvDesc,cpuHandle);
	GDEngine->GetComputeDescHeap()->SetCBV(cpuHandle, _reg);

	_currentIndex++;
}

D3D12_CPU_DESCRIPTOR_HANDLE ConstantBuffer::GetCpuHandle(uint32 index)
{
	//DX_LOG(L"[GetCpuHandle] reg=" << static_cast<int>(_reg)
	//	<< L" index=" << index
	//	<< L" count=" << _elementCount);

	assert(index < _elementCount);

	return CD3DX12_CPU_DESCRIPTOR_HANDLE(_cpuHandleBegin, index * _handleIncrementSize);
}