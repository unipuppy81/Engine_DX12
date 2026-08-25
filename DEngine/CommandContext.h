#pragma once
class CommandContext
{
public:
	void Init(ID3D12Device* device);
	// 반드시 해당 FrameResource의 Fence 완료 후 호출
	void Reset();
	void Close();

	ID3D12GraphicsCommandList* GetCommandList() const { return _commandList.Get(); }
	ID3D12CommandAllocator* GetCommandAllocator() const { return _commandAllocator.Get(); }

private:
	ComPtr<ID3D12CommandAllocator> _commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> _commandList;
};

