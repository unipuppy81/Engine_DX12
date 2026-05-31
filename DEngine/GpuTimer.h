#pragma once
class GpuTimer
{
public:
	void Init();
	void Begin();
	void End();
	void Resolve();
	void UpdateResult();

	float GetGpuMs() const { return _gpuMs; }

private:
	ComPtr<ID3D12QueryHeap> _queryHeap;
	ComPtr<ID3D12Resource> _readbackBuffer;

	uint64 _timestampFrequency = 0;
	float _gpuMs = 0.0f;
};
