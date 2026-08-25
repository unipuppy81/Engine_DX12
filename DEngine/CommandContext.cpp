#include "pch.h"
#include "CommandContext.h"

void CommandContext::Init(ID3D12Device* device)
{
	device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));
	device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), nullptr, IID_PPV_ARGS(&_commandList));

	// CreateCommandList 직후에는 Open 상태이므로
	// 이후 Reset()을 위해 먼저 Close
	_commandList->Close();
}

void CommandContext::Reset()
{
	_commandAllocator->Reset();
	_commandList->Reset(_commandAllocator.Get(), nullptr);
}

void CommandContext::Close()
{
	_commandList->Close();
}