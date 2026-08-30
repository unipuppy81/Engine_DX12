#pragma once
class GpuTimer
{
public:
	void Init();

	void Begin(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList);
	void End(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList);
	void Resolve(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList);
	void UpdateResult(uint32 frameIndex);

	float GetGpuMs() const { return _gpuMs; }

private:
	ComPtr<ID3D12QueryHeap> _queryHeap;
	ComPtr<ID3D12Resource> _readbackBuffer;

	uint64 _timestampFrequency = 0;
	float _gpuMs = 0.0f;

	bool _hasResult[SWAP_CHAIN_BUFFER_COUNT] = {};
};
