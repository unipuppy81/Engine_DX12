#pragma once

// ************************
// GraphicsDescriptorHeap
// ************************

class GraphicsDescriptorHeap
{
public:
	void Init(uint32 count);

	void Clear(uint32 frameIndex);
	void SetCBV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, CBV_REGISTER reg);
	void SetSRV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, SRV_REGISTER reg);

	void CommitTable();
	void BeginThreadRecording();

	ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap() { return _descHeap; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(CBV_REGISTER reg);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(SRV_REGISTER reg);

private:
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint8 reg);

private:
	ComPtr<ID3D12DescriptorHeap> _descHeap;
	uint64					_handleSize = 0;
	uint64					_groupSize = 0;					// Descriptor Table 한 그룹의 바이트 크기
	uint32					_groupCountPerFrame = 0;		// 프레임당 그룹 수
	uint32					_totalGroupCount = 0;			// 전체 그룹 수
	
	atomic<uint32> _nextGroupIndex = 0;
	uint32 _frameEndGroupIndex = 0;

	static thread_local uint32 _threadGroupIndex;
	static constexpr uint32 INVALID_GROUP_INDEX = UINT32_MAX;
};

// ************************
// ComputeDescriptorHeap
// ************************

class ComputeDescriptorHeap
{
public:
	void Init();

	void SetCBV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, CBV_REGISTER reg);
	void SetSRV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, SRV_REGISTER reg);
	void SetUAV(D3D12_CPU_DESCRIPTOR_HANDLE srcHandle, UAV_REGISTER reg);

	void CommitTable();

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(CBV_REGISTER reg);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(SRV_REGISTER reg);
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UAV_REGISTER reg);

private:
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(uint8 reg);

private:

	ComPtr<ID3D12DescriptorHeap> _descHeap;
	uint64						_handleSize = 0;
};

