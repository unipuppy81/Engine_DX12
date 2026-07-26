#include "pch.h"
#include "DescriptorAllocator.h"

void DescriptorAllocator::Init(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 descriptorCount)
{
	assert(device != nullptr);
	assert(descriptorCount > 0);

	_type = type;
	_capacity = descriptorCount;
	_allocatedCount = 0;

	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = type;
	desc.NumDescriptors = descriptorCount;

	// CPU 전용 정적 Descriptor Heap
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_heap));

	assert(SUCCEEDED(hr));

	_descriptorSize = device->GetDescriptorHandleIncrementSize(type);

	_freeRanges.clear();
	_freeRanges.push_back({ 0, descriptorCount });
}

DescriptorAllocation DescriptorAllocator::Allocate(uint32 count)
{
	assert(count > 0);

	lock_guard<mutex> lock(_mutex);

	for (size_t i = 0; i < _freeRanges.size(); ++i)
	{
		FreeRange& range = _freeRanges[i];

		if (range.count < count)
			continue;

		DescriptorAllocation allocation;
		allocation.startIndex = range.startIndex;
		allocation.count = count;

		range.startIndex += count;
		range.count -= count;

		if (range.count == 0)
			_freeRanges.erase(_freeRanges.begin() + i);

		_allocatedCount += count;

		return allocation;
	}

	assert(false && "Descriptor Heap is full");
	return {};
}

void DescriptorAllocator::Free(DescriptorAllocation& allocation)
{
	if (!allocation.IsValid())
		return;

	lock_guard<mutex> lock(_mutex);

	assert(allocation.startIndex + allocation.count <= _capacity);

	_freeRanges.push_back({ allocation.startIndex, allocation.count });

	sort(_freeRanges.begin(), _freeRanges.end(),
		[](const FreeRange& a, const FreeRange& b)
		{ 
			return a.startIndex < b.startIndex; 
		});

	vector<FreeRange> mergedRanges;
	mergedRanges.reserve(_freeRanges.size());

	for (const FreeRange& range : _freeRanges)
	{
		if (mergedRanges.empty())
		{
			mergedRanges.push_back(range);
			continue;
		}

		FreeRange& previous = mergedRanges.back();
		const uint32 previousEnd = previous.startIndex + previous.count;

		// 중복 반환 또는 범위 침범
		assert(previousEnd <= range.startIndex);

		if (previousEnd == range.startIndex)
		{
			previous.count += range.count;
		}
		else
		{
			mergedRanges.push_back(range);
		}
	}

	_freeRanges = std::move(mergedRanges);

	_allocatedCount -= allocation.count;
	allocation.Reset();
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::GetCPUHandle(const DescriptorAllocation& allocation, uint32 offset) const
{
	assert(allocation.IsValid());
	assert(offset < allocation.count);
	assert(allocation.startIndex + offset < _capacity);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _heap->GetCPUDescriptorHandleForHeapStart();
	handle.ptr += static_cast<SIZE_T>(allocation.startIndex + offset) * _descriptorSize;

	return handle;
}