#include "pch.h"
#include "Texture.h"
#include "DEngine.h"

Texture::Texture() : Object(OBJECT_TYPE::TEXTURE)
{

}

Texture::~Texture()
{

}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRTVHandle(uint32 index)
{
	if (_rtvHeap == nullptr)
	{
		::OutputDebugStringW(L"[Texture::GetRTVHandle] _rtvHeap is nullptr\n");
		assert(false);
	}
	
	uint32 rtvDescriptorSize = DEVICE->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtvHeap->GetCPUDescriptorHandleForHeapStart(), index, rtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE Texture::GetRTVHandle(uint32 faceIndex, uint32 mipLevel)
{
	assert(_rtvHeap != nullptr);
	assert(faceIndex < 6);
	assert(mipLevel < _desc.MipLevels);

	uint32 descriptorSize = DEVICE->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	uint32 descriptorIndex = mipLevel * 6 + faceIndex;

	return CD3DX12_CPU_DESCRIPTOR_HANDLE(_rtvHeap->GetCPUDescriptorHandleForHeapStart(),descriptorIndex, descriptorSize);
}

void Texture::Load(const wstring& path)
{
	// 파일 확장자 얻기
	wstring ext = fs::path(path).extension();

	if (ext == L".dds" || ext == L".DDS")
		::LoadFromDDSFile(path.c_str(), DDS_FLAGS_NONE, nullptr, _image);
	else if (ext == L".tga" || ext == L".TGA")
		::LoadFromTGAFile(path.c_str(), nullptr, _image);
	else if (ext == L".hdr" || ext == L".HDR")
		LoadFromHDRFile(path.c_str(), nullptr, _image);
	else // png, jpg, jpeg, bmp
		::LoadFromWICFile(path.c_str(), WIC_FLAGS_NONE, nullptr, _image);

	HRESULT hr = ::CreateTexture(DEVICE.Get(), _image.GetMetadata(), &_tex2D);
	if (FAILED(hr))
		assert(nullptr);

	_desc = _tex2D->GetDesc();

	vector<D3D12_SUBRESOURCE_DATA> subResources;

	hr = ::PrepareUpload(DEVICE.Get(),
		_image.GetImages(),
		_image.GetImageCount(),
		_image.GetMetadata(),
		subResources);

	if (FAILED(hr))
		assert(nullptr);

	const uint64 bufferSize = ::GetRequiredIntermediateSize(_tex2D.Get(), 0, static_cast<uint32>(subResources.size()));

	D3D12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

	ComPtr<ID3D12Resource> textureUploadHeap;
	hr = DEVICE->CreateCommittedResource(
		&heapProperty,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(textureUploadHeap.GetAddressOf()));

	if (FAILED(hr))
		assert(nullptr);

	::UpdateSubresources(RESOURCE_CMD_LIST.Get(),
		_tex2D.Get(),
		textureUploadHeap.Get(),
		0, 0,
		static_cast<unsigned int>(subResources.size()),
		subResources.data());

	GDEngine->GetGraphicsCmdQueue()->FlushResourceCommandQueue();

	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DEVICE->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvHeap));

	_srvHeapBegin = _srvHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = _image.GetMetadata().format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = _image.GetMetadata().mipLevels;
	DEVICE->CreateShaderResourceView(_tex2D.Get(), &srvDesc, _srvHeapBegin);
}


void Texture::Create(DXGI_FORMAT format, uint32 width, uint32 height,
	const D3D12_HEAP_PROPERTIES& heapProperty, D3D12_HEAP_FLAGS heapFlags,
	D3D12_RESOURCE_FLAGS resFlags, Vec4 clearColor)
{
	_desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height);
	_desc.Flags = resFlags;

	D3D12_CLEAR_VALUE optimizedClearValue = {};
	D3D12_CLEAR_VALUE* pOptimizedClearValue = nullptr;

	D3D12_RESOURCE_STATES resourceStates = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;

	if (resFlags & D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		resourceStates = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_DEPTH_WRITE;
		optimizedClearValue = CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0);
		pOptimizedClearValue = &optimizedClearValue;
	}
	else if (resFlags & D3D12_RESOURCE_FLAGS::D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		resourceStates = D3D12_RESOURCE_STATES::D3D12_RESOURCE_STATE_COMMON;
		float arrFloat[4] = { clearColor.x, clearColor.y, clearColor.z, clearColor.w };
		optimizedClearValue = CD3DX12_CLEAR_VALUE(format, arrFloat);
		pOptimizedClearValue = &optimizedClearValue;
	}

	// Create Texture2D
	HRESULT hr = DEVICE->CreateCommittedResource(
		&heapProperty,
		heapFlags,
		&_desc,
		resourceStates,
		pOptimizedClearValue,
		IID_PPV_ARGS(&_tex2D));

	assert(SUCCEEDED(hr));

	CreateFromResource(_tex2D);
}


void Texture::CreateCubeMap(DXGI_FORMAT format, uint32 size, uint32 mipLevels,
	const D3D12_HEAP_PROPERTIES& heapProperty,	D3D12_HEAP_FLAGS heapFlags, D3D12_RESOURCE_FLAGS resFlags)
{
	// CubeMap = Texture2D Array 6개
	_desc = CD3DX12_RESOURCE_DESC::Tex2D(
		format,
		size,
		size,
		6,      // arraySize
		mipLevels);     // mipLevels

	_desc.Flags = resFlags;

	HRESULT hr = DEVICE->CreateCommittedResource(
		&heapProperty,
		heapFlags,
		&_desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&_tex2D));

	assert(SUCCEEDED(hr));

	// SRV 생성
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = DEVICE->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvHeap));
	assert(SUCCEEDED(hr));

	_srvHeapBegin = _srvHeap->GetCPUDescriptorHandleForHeapStart();

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = _desc.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MipLevels = _desc.MipLevels;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	DEVICE->CreateShaderResourceView(_tex2D.Get(), &srvDesc, _srvHeapBegin);

	// RTV Heap 생성: CubeMap face 6개
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.NumDescriptors = 6 * _desc.MipLevels;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = DEVICE->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&_rtvHeap));
	assert(SUCCEEDED(hr));
	assert(_rtvHeap != nullptr);

	uint32 rtvDescriptorSize = DEVICE->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (uint32 mip = 0; mip < _desc.MipLevels; ++mip)
	{
		for (uint32 face = 0; face < 6; ++face)
		{
			D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
			rtvDesc.Format = _desc.Format;
			rtvDesc.ViewDimension =
				D3D12_RTV_DIMENSION_TEXTURE2DARRAY;

			rtvDesc.Texture2DArray.MipSlice = mip;
			rtvDesc.Texture2DArray.FirstArraySlice = face;
			rtvDesc.Texture2DArray.ArraySize = 1;
			rtvDesc.Texture2DArray.PlaneSlice = 0;

			DEVICE->CreateRenderTargetView(_tex2D.Get(), &rtvDesc, rtvHandle);

			rtvHandle.ptr += rtvDescriptorSize;
		}
	}
}

void Texture::CreateFromResource(ComPtr<ID3D12Resource> tex2D)
{
	_tex2D = tex2D;

	_desc = tex2D->GetDesc();

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
}