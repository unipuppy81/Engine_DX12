#pragma once


class Device
{
public:
	void Init();

	ComPtr<IDXGIFactory> GetDXGI() { return	_dxgi; }		// 화면 기능
	ComPtr<ID3D12Device> GetDevice() { return _device; }	// 객체 생성

private:
	// COM(Component Object Model)
	// ComPtr = 일종의 스마트 포인터
	ComPtr<ID3D12Debug>			_debugController;
	ComPtr<IDXGIFactory>		_dxgi;		// 화면 기능
	ComPtr<ID3D12Device>		_device;	// 객체 생성
};

