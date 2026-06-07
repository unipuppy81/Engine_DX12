#include "pch.h"
#include "GpuTimer.h"
#include "DEngine.h"

void GpuTimer::Init()
{
	// QueryHeap 생성
	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.Count = 2;
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

	HRESULT hr = DEVICE.Get()->CreateQueryHeap(
		&queryHeapDesc,
		IID_PPV_ARGS(&_queryHeap)
	);
	assert(SUCCEEDED(hr));

	// Timestamp 결과 2개 읽기용 버퍼
	uint64 bufferSize = sizeof(uint64) * 2;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_READBACK);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	hr = DEVICE.Get()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&_readbackBuffer)
	);
	assert(SUCCEEDED(hr));

	// GPU timestamp 주파수
	GRAPHICS_CMD_QUEUE->GetCmdQueue()->GetTimestampFrequency(&_timestampFrequency);
}

void GpuTimer::Begin()
{
	GRAPHICS_CMD_LIST.Get()->EndQuery(
		_queryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		0
	);
}

void GpuTimer::End()
{
	GRAPHICS_CMD_LIST.Get()->EndQuery(
		_queryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		1
	);
}

void GpuTimer::Resolve()
{
	GRAPHICS_CMD_LIST.Get()->ResolveQueryData(
		_queryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		0,
		2,
		_readbackBuffer.Get(),
		0
	);
}

void GpuTimer::UpdateResult()
{
	uint64* data = nullptr;

	D3D12_RANGE readRange = {};
	readRange.Begin = 0;
	readRange.End = sizeof(uint64) * 2;

	HRESULT hr = _readbackBuffer->Map(
		0,
		&readRange,
		reinterpret_cast<void**>(&data)
	);

	if (FAILED(hr))
		return;

	uint64 start = data[0];
	uint64 end = data[1];

	_readbackBuffer->Unmap(0, nullptr);

	WCHAR text[256];
	// [Log] GPU
	//swprintf_s(text, L"GPU Timestamp start=%llu end=%llu freq=%llu\n", start, end, _timestampFrequency);
	//::OutputDebugString(text);

	if (end > start && _timestampFrequency > 0)
	{
		_gpuMs = static_cast<float>(
			static_cast<double>(end - start) /
			static_cast<double>(_timestampFrequency) * 1000.0
			);
	}
}