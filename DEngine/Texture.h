#pragma once
#include "Object.h"
#include "DescriptorAllocator.h"

class Texture : public Object
{
public:
	Texture();
	virtual ~Texture();
	virtual void Load(const wstring& path) override;

	void Create(DXGI_FORMAT format, uint32 width, uint32 height,
		const D3D12_HEAP_PROPERTIES& heapProperty, D3D12_HEAP_FLAGS heapFlags,
		D3D12_RESOURCE_FLAGS resFlags, Vec4 clearColor = Vec4());

	void CreateCubeMap(DXGI_FORMAT format, uint32 size, uint32 mipLevels,
		const D3D12_HEAP_PROPERTIES& heapProperty, D3D12_HEAP_FLAGS heapFlags,
		D3D12_RESOURCE_FLAGS resFlags);

	void CreateFromResource(ComPtr<ID3D12Resource> tex2D);
	void ReleaseGpuResources();

	uint32 GetSRVIndex() const
	{
		return _srvAllocation.startIndex;
	}

public:
	ComPtr<ID3D12Resource> GetTex2D() { return _tex2D; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE GetUAVHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32 index = 0);
	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle(uint32 faceIndex, uint32 mipLevel);

	float GetWidth() const { return static_cast<float>(_desc.Width); }
	float GetHeight() const { return static_cast<float>(_desc.Height); }
	uint32 GetMipLevels() const { return static_cast<uint32>(_desc.MipLevels); }

private:
	void CreateDefaultViews();
	void CreateCubeMapViews();

private:
	ScratchImage			 		_image;
	D3D12_RESOURCE_DESC				_desc = {};
	ComPtr<ID3D12Resource>			_tex2D;

	DescriptorAllocation _srvAllocation;
	DescriptorAllocation _uavAllocation;
	DescriptorAllocation _rtvAllocation;
	DescriptorAllocation _dsvAllocation;
};

