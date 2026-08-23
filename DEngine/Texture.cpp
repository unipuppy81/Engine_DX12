#include "pch.h"
#include "Texture.h"
#include "DEngine.h"

Texture::Texture() : Object(OBJECT_TYPE::TEXTURE)
{

}

Texture::~Texture()
{
	ReleaseGpuResources();
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetSRVHandle()
{
	assert(_srvAllocation.IsValid());
	return GDEngine->GetResourceDescriptorAllocator()->GetCPUHandle(_srvAllocation);
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetUAVHandle()
{
	assert(_uavAllocation.IsValid());
	return GDEngine->GetResourceDescriptorAllocator()->GetCPUHandle(_uavAllocation);
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetDSVHandle()
{
	assert(_dsvAllocation.IsValid());
	return GDEngine->GetDSVDescriptorAllocator()->GetCPUHandle(_dsvAllocation);
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRTVHandle(uint32 index)
{
	assert(_rtvAllocation.IsValid());
	assert(index < _rtvAllocation.count);
	return GDEngine->GetRTVDescriptorAllocator()->GetCPUHandle(_rtvAllocation, index);
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRTVHandle(uint32 faceIndex, uint32 mipLevel)
{
	assert(faceIndex < 6);
	assert(mipLevel < _desc.MipLevels);

	const uint32 index = mipLevel * 6 + faceIndex;

	return GetRTVHandle(index);
}

void Texture::CreateDefaultViews()
{
	assert(_tex2D != nullptr);

	auto resourceAllocator = GDEngine->GetResourceDescriptorAllocator();
	auto rtvAllocator = GDEngine->GetRTVDescriptorAllocator();
	auto dsvAllocator = GDEngine->GetDSVDescriptorAllocator();

	// Depth Texture는 현재 DSV만 생성
	if (_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		_dsvAllocation = dsvAllocator->Allocate();
		D3D12_CPU_DESCRIPTOR_HANDLE handle = dsvAllocator->GetCPUHandle(_dsvAllocation);
		DEVICE->CreateDepthStencilView(_tex2D.Get(), nullptr, handle);

		return;
	}

	// RTV
	if (_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		_rtvAllocation = rtvAllocator->Allocate();
		D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvAllocator->GetCPUHandle(_rtvAllocation);
		DEVICE->CreateRenderTargetView(_tex2D.Get(), nullptr, handle);
	}

	// UAV
	if (_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
	{
		_uavAllocation = resourceAllocator->Allocate();
		D3D12_CPU_DESCRIPTOR_HANDLE handle = resourceAllocator->GetCPUHandle(_uavAllocation);

		D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
		desc.Format = _desc.Format;
		desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		DEVICE->CreateUnorderedAccessView(_tex2D.Get(), nullptr, &desc, handle);
	}

	// SRV
	_srvAllocation = resourceAllocator->Allocate();

	D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = resourceAllocator->GetCPUHandle(_srvAllocation);
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = _desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = _desc.MipLevels;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	DEVICE->CreateShaderResourceView(_tex2D.Get(), &srvDesc, srvHandle);
}

void Texture::CreateCubeMapViews()
{
	assert(_tex2D != nullptr);
	assert(_desc.DepthOrArraySize == 6);

	auto resourceAllocator = GDEngine->GetResourceDescriptorAllocator();
	auto rtvAllocator = GDEngine->GetRTVDescriptorAllocator();

	// Cubemap SRV
	_srvAllocation = resourceAllocator->Allocate();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = _desc.Format;
	srvDesc.ViewDimension =	D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = _desc.MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	DEVICE->CreateShaderResourceView(_tex2D.Get(), &srvDesc, resourceAllocator->GetCPUHandle(_srvAllocation));

	// Cubemap이 RenderTarget일 때만 RTV 생성
	if (!(_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
	{
		return;
	}

	const uint32 rtvCount = 6 * static_cast<uint32>(_desc.MipLevels);
	_rtvAllocation = rtvAllocator->Allocate(rtvCount);

	for (uint32 mip = 0; mip < _desc.MipLevels; ++mip)
	{
		for (uint32 face = 0; face < 6; ++face)
		{
			const uint32 index = mip * 6 + face;

			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};

			rtvDesc.Format = _desc.Format;
			rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
			rtvDesc.Texture2DArray.MipSlice = mip;
			rtvDesc.Texture2DArray.FirstArraySlice = face;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.PlaneSlice = 0;

			D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvAllocator->GetCPUHandle(_rtvAllocation, index);
			DEVICE->CreateRenderTargetView(_tex2D.Get(), &rtvDesc, handle);
		}
	}
}

void Texture::Load(const wstring& path)
{
	ReleaseGpuResources();
	_image.Release();

	// 파일 확장자 얻기
	const wstring ext = fs::path(path).extension();

	if (ext == L".dds" || ext == L".DDS")
		::LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, nullptr, _image);
	else if (ext == L".tga" || ext == L".TGA")
		::LoadFromTGAFile(path.c_str(), nullptr, _image);
	else if (ext == L".hdr" || ext == L".HDR")
		LoadFromHDRFile(path.c_str(), nullptr, _image);
	else // png, jpg, jpeg, bmp
		::LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, nullptr, _image);

	HRESULT hr = ::CreateTexture(DEVICE.Get(), _image.GetMetadata(), &_tex2D);
	assert(SUCCEEDED(hr));

	_desc = _tex2D->GetDesc();

	vector<D3D12_SUBRESOURCE_DATA> subResources;

	hr = ::PrepareUpload(
		DEVICE.Get(),
		_image.GetImages(),
		_image.GetImageCount(),
		_image.GetMetadata(),
		subResources);

	assert(SUCCEEDED(hr));

	const uint64 bufferSize = ::GetRequiredIntermediateSize(_tex2D.Get(), 0, static_cast<uint32>(subResources.size()));

	D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	ComPtr<ID3D12Resource> textureUploadHeap;

	hr = DEVICE->CreateCommittedResource(&heapProperty, D3D12_HEAP_FLAG_NONE, &uploadDesc, 
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));

	assert(SUCCEEDED(hr));

	::UpdateSubresources(RESOURCE_CMD_LIST.Get(), _tex2D.Get(), textureUploadHeap.Get(), 
		0, 0, static_cast<uint32>(subResources.size()), subResources.data());

	GDEngine->GetGraphicsCmdQueue()->FlushResourceCommandQueue();
	CreateDefaultViews();
}


void Texture::Create(DXGI_FORMAT format, uint32 width, uint32 height,
	const D3D12_HEAP_PROPERTIES& heapProperty, D3D12_HEAP_FLAGS heapFlags,
	D3D12_RESOURCE_FLAGS resFlags, Vec4 clearColor)
{
	ReleaseGpuResources();

	_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
	_desc.Flags = resFlags;

	D3D12_CLEAR_VALUE clearValue = {};
	D3D12_CLEAR_VALUE* clearValuePointer = nullptr;
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

	if (resFlags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		clearValue = CD3DX12_CLEAR_VALUE(format, 1.0f, 0);
		clearValuePointer = &clearValue;
	}
	else if (resFlags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		const float color[4] =
		{
			clearColor.x,
			clearColor.y,
			clearColor.z,
			clearColor.w
		};

		clearValue = CD3DX12_CLEAR_VALUE(format, color);
		clearValuePointer = &clearValue;
	}

	HRESULT hr = DEVICE->CreateCommittedResource(
		&heapProperty,
		heapFlags,
		&_desc,
		initialState,
		clearValuePointer,
		IID_PPV_ARGS(&_tex2D));

	assert(SUCCEEDED(hr));

	_resourceState = initialState;

	_desc = _tex2D->GetDesc();
	CreateDefaultViews();
}


void Texture::CreateCubeMap(DXGI_FORMAT format, uint32 size, uint32 mipLevels,
	const D3D12_HEAP_PROPERTIES& heapProperty,	D3D12_HEAP_FLAGS heapFlags, D3D12_RESOURCE_FLAGS resFlags)
{
	ReleaseGpuResources();

	// CubeMap = Texture2D Array 6개
	_desc = CD3DX12_RESOURCE_DESC::Tex2D(
		format,
		size,
		size,
		6,									// arraySize
		static_cast<uint16>(mipLevels));     // mipLevels

	_desc.Flags = resFlags;

	const D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;

	HRESULT hr = DEVICE->CreateCommittedResource(
		&heapProperty,
		heapFlags,
		&_desc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&_tex2D));

	assert(SUCCEEDED(hr));

	_resourceState = initialState;

	_desc = _tex2D->GetDesc();
	CreateCubeMapViews();
}

void Texture::CreateFromResource(ComPtr<ID3D12Resource> tex2D)
{
	assert(tex2D != nullptr);

	ReleaseGpuResources();

	_tex2D = std::move(tex2D);
	_desc = _tex2D->GetDesc();

	_resourceState = D3D12_RESOURCE_STATE_PRESENT;

	CreateDefaultViews();

	/*
	// 주요 조합
	// - DSV 단독 (조합X)
	// - SRV
	// - RTV + SRV
	if (_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		// DSV
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		heapDesc.NodeMask = 0;
		DEVICE->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_dsvHeap));

		D3D12_CPU_DESCRIPTOR_HANDLE hDSVHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();
		DEVICE->CreateDepthStencilView(_tex2D.Get(), nullptr, hDSVHandle);
	}
	else
	{
		if (_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
		{
			// RTV
			D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
			heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			heapDesc.NumDescriptors = 1;
			heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			heapDesc.NodeMask = 0;
			DEVICE->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&_rtvHeap));

			D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapBegin = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
			DEVICE->CreateRenderTargetView(_tex2D.Get(), nullptr, rtvHeapBegin);
		}

		if (_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
		{
			// UAV
			D3D12_DESCRIPTOR_HEAP_DESC uavHeapDesc = {};
			uavHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			uavHeapDesc.NumDescriptors = 1;
			uavHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			uavHeapDesc.NodeMask = 0;
			DEVICE->CreateDescriptorHeap(&uavHeapDesc, IID_PPV_ARGS(&_uavHeap));

			_uavHeapBegin = _uavHeap->GetCPUDescriptorHandleForHeapStart();

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = _desc.Format; //_image.GetMetadata().format;
			uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

			DEVICE->CreateUnorderedAccessView(_tex2D.Get(), nullptr, &uavDesc, _uavHeapBegin);
		}

		// SRV
		D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
		srvHeapDesc.NumDescriptors = 1;
		srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		DEVICE->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvHeap));

		_srvHeapBegin = _srvHeap->GetCPUDescriptorHandleForHeapStart();

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc.Format = _desc.Format; //_image.GetMetadata().format;
		srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = 1;
		DEVICE->CreateShaderResourceView(_tex2D.Get(), &srvDesc, _srvHeapBegin);
	}
	*/
}

void Texture::ReleaseGpuResources()
{
	const bool hasResource = _tex2D != nullptr;
	const bool hasDescriptor = _srvAllocation.IsValid() || _uavAllocation.IsValid() || _rtvAllocation.IsValid() || _dsvAllocation.IsValid();

	if (!hasResource && !hasDescriptor)
		return;

	auto queue = GDEngine->GetGraphicsCmdQueue();
	assert(queue != nullptr);

	queue->DeferredRelease(_tex2D);
	queue->DeferredFreeDescriptor(GDEngine->GetResourceDescriptorAllocator(), _srvAllocation);
	queue->DeferredFreeDescriptor(GDEngine->GetResourceDescriptorAllocator(), _uavAllocation);
	queue->DeferredFreeDescriptor(GDEngine->GetRTVDescriptorAllocator(), _rtvAllocation);
	queue->DeferredFreeDescriptor(GDEngine->GetDSVDescriptorAllocator(), _dsvAllocation);

	_desc = {};
}