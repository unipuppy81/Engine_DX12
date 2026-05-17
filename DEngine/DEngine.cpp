#include "pch.h"
#include "DEngine.h"

void DEngine::Init(const WindowInfo& info)
{
	_window = info;

	_viewport = { 0, 0, static_cast<FLOAT>(info.width), static_cast<FLOAT>(info.height), 0.0f, 1.0f };
	_scissorRect = CD3DX12_RECT(0, 0, info.width, info.height);

	_device->Init();
	_cmdQueue->Init(_device->GetDevice(), _swapChain);
	_swapChain->Init(info, _device->GetDevice(), _device->GetDXGI(), _cmdQueue->GetCmdQueue());
	_rootSignature->Init();
	_constantBuffer->Init(sizeof(Transform), 256);
	_tableDescHeap->Init(256);
	_depthStencilBuffer->Init(_window);

	_input->Init(info.hwnd);
	_timer->Init();

	ResizeWindow(_window.width, _window.height);
}

void DEngine::Render()
{
	RenderBegin();

	// TODO : ±×¸®±â
		
	RenderEnd();
}

void DEngine::Update()
{
	_input->Update();
	_timer->Update();

	ShowFps();
}

void DEngine::RenderBegin()
{
	_cmdQueue->RenderBegin(&_viewport, &_scissorRect);
}

void DEngine::RenderEnd()
{
	_cmdQueue->RenderEnd();
}

void DEngine::ResizeWindow(int32 width, int32 height)
{
	_window.width = width;
	_window.height = height;

	RECT rect = { 0,0, width, height };
	::AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
	::SetWindowPos(_window.hwnd, 0, 100, 100, width, height, 0);

	_depthStencilBuffer->Init(_window);
}

void DEngine::ShowFps()
{
	uint32 fps = _timer->GetFps();

	WCHAR text[100] = L"";
	::wsprintf(text, L"FPS : %d", fps);

	::SetWindowText(_window.hwnd, text);
}