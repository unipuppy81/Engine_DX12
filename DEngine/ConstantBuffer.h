#pragma once

#include "UploadAllocator.h"

enum class CONSTANT_BUFFER_TYPE : uint8
{
	GLOBAL,
	TRANSFORM,
	MATERIAL,
	MATERIAL_PBR,
	IBL_CUBEMAP,
	CAMERA,
	// ...
	END
};

enum
{
	CONSTANT_BUFFER_COUNT = static_cast<uint8>(CONSTANT_BUFFER_TYPE::END)
};

constexpr uint32 FRAME_COUNT = 2;

class ConstantBuffer
{
public:
	ConstantBuffer();
	~ConstantBuffer();

	void Init(CBV_REGISTER reg, uint32 size, uint32 count);
	
	void Clear(uint32 frameIndex);
	void PushGraphicsData(void* buffer, uint32 size);
	void SetGraphicsGlobalData(void* buffer, uint32 size);
	void PushComputeData(void* buffer, uint32 size);

	D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32 index);

	D3D12_GPU_VIRTUAL_ADDRESS GetGlobalGpuAddress() const { return _globalGpuAddress; }

private:
	void CreateView();

private:
	// 256 byte 정렬된 CB 크기
	uint32 _elementSize = 0;

	// 프레임에서 최대 몇 번 사용할지
	uint32 _elementCount = 0;
	//uint32 _currentIndex = 0;
	atomic<uint32> _currentIndex{ 0 };

	// CPU 전용 CBV Descriptor Heap
	ComPtr<ID3D12DescriptorHeap> _cbvHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE _cpuHandleBegin = {};
	uint32 _handleIncrementSize = 0;

	CBV_REGISTER _reg = {};
	D3D12_GPU_VIRTUAL_ADDRESS _globalGpuAddress = 0;
};

