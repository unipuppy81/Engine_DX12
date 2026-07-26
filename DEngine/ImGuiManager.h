#pragma once

class ImGuiManager
{
	DECLARE_SINGLE(ImGuiManager);

public:
	void Init(HWND hwnd);
	void BeginFrame();
	void Render();
	void Shutdown();


private:
	void CreateDescriptorHeap();

private:
	void ShowPerformance();

private:
	ComPtr<ID3D12DescriptorHeap> _descriptorHeap;

	bool _showDemoWindow = false;
	bool _showDebugWindow = true;

	D3D12_CPU_DESCRIPTOR_HANDLE _fontSrvCpuHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE _fontSrvGpuHandle = {};
};

