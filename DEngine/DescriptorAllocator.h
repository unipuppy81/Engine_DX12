#pragma once

#include <mutex>

struct DescriptorAllocation
{
	uint32 startIndex = UINT32_MAX;
	uint32 count = 0;

	bool IsValid() const { return startIndex != UINT32_MAX && count > 0; }

	void Reset()
	{
		startIndex = UINT32_MAX;
		count = 0;
	}
};

class DescriptorAllocator
{
private:
	struct FreeRange
	{
		uint32 startIndex = 0;
		uint32 count = 0;
	};

public:
	void Init(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 descriptorCount);

	DescriptorAllocation Allocate(uint32 count = 1);
	void Free(DescriptorAllocation& allocation);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(const DescriptorAllocation& allocation, uint32 offset = 0) const;

	ComPtr<ID3D12DescriptorHeap> GetHeap() const { return _heap; }
	uint32 GetCapacity() const { return _capacity; }
	uint32 GetAllocatedCount() const { return _allocatedCount; }

private:
	ComPtr<ID3D12DescriptorHeap> _heap;

	D3D12_DESCRIPTOR_HEAP_TYPE _type = {};
	uint32 _descriptorSize = 0;
	uint32 _capacity = 0;
	uint32 _allocatedCount = 0;

	vector<FreeRange> _freeRanges;
	mutable mutex _mutex;
};