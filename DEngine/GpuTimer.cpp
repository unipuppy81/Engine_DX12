#include "pch.h"
#include "GpuTimer.h"
#include "DEngine.h"

void GpuTimer::Init()
{
	constexpr uint32 QUERY_COUNT_PER_FRAME = 2;
	const uint32 totalQueryCount = SWAP_CHAIN_BUFFER_COUNT * QUERY_COUNT_PER_FRAME;
	
	// QueryHeap 생성
	D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
	queryHeapDesc.Count = totalQueryCount;
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;

	HRESULT hr = DEVICE.Get()->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&_queryHeap));
	assert(SUCCEEDED(hr));

	// Timestamp 결과 2개 읽기용 버퍼
	uint64 bufferSize = sizeof(uint64) * totalQueryCount;

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

	// GPU timestamp
	GRAPHICS_CMD_QUEUE->GetCmdQueue()->GetTimestampFrequency(&_timestampFrequency);
}

void GpuTimer::Begin(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList)
{
	uint32 queryIndex = frameIndex * 2;

	cmdList->EndQuery(
		_queryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		queryIndex
	);
}

void GpuTimer::End(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList)
{
	uint32 queryIndex = frameIndex * 2 + 1;

	cmdList->EndQuery(
		_queryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		queryIndex

	);
}

void GpuTimer::Resolve(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList)
{
	uint32 queryStartIndex = frameIndex * 2;
	uint64 destinationOffset = sizeof(uint64) * queryStartIndex;


	cmdList->ResolveQueryData(
		_queryHeap.Get(),
		D3D12_QUERY_TYPE_TIMESTAMP,
		queryStartIndex,
		2,
		_readbackBuffer.Get(),
		destinationOffset
	);

	_hasResult[frameIndex] = true;
}

void GpuTimer::UpdateResult(uint32 frameIndex)
{
	if (!_hasResult[frameIndex])
		return;

	uint64 offset = sizeof(uint64) * frameIndex * 2;
	uint64* data = nullptr;

	D3D12_RANGE readRange = {};
	readRange.Begin = offset;
	readRange.End = offset + sizeof(uint64) * 2;

	HRESULT hr = _readbackBuffer->Map(
		0,
		&readRange,
		reinterpret_cast<void**>(&data)
	);

	if (FAILED(hr))
		return;

	uint8* bytes = reinterpret_cast<uint8*>(data);
	uint64* timestamps = reinterpret_cast<uint64*>(bytes + offset);

	uint64 start = timestamps[0];
	uint64 end = timestamps[1];

	D3D12_RANGE writeRange = { 0, 0 };
	_readbackBuffer->Unmap(0, &writeRange);


	if (end > start && _timestampFrequency > 0)
	{
		_gpuMs = static_cast<float>(static_cast<double>(end - start) 
			/ static_cast<double>(_timestampFrequency) * 1000.0);
	}
}