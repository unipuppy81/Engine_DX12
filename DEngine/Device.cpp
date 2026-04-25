#include "pch.h"
#include "Device.h"

void Device::Init()
{
	// D3D12 디버그층 활성화
#ifdef _DEBUG
	::D3D12GetDebugInterface(IID_PPV_ARGS(&_debugController));
	_debugController->EnableDebugLayer();
#endif

	// DXGI(DirectX Graphics Infrastructure)
	// Direct3D 와 함께 쓰이는 API
	::CreateDXGIFactory(IID_PPV_ARGS(&_dxgi));

	// D3D_FEATURE_LEVEL_11_0 : 응용 프로그램이 요구하는 최소 기능 수준
	::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device));

}
