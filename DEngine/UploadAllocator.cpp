#include "pch.h"
#include "UploadAllocator.h"

UploadAllocator::~UploadAllocator()
{
    if (_uploadBuffer && _mappedAddress)
    {
        _uploadBuffer->Unmap(0, nullptr);
        _mappedAddress = nullptr;
    }
}

void UploadAllocator::Init(ID3D12Device* device, uint64 capacity)
{
    _capacity = capacity;
    _offset = 0;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Alignment = 0;
    desc.Width = capacity;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_UNKNOWN;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_uploadBuffer));

    assert(SUCCEEDED(hr));
    D3D12_RANGE readRange = { 0, 0 };

    hr = _uploadBuffer->Map(0, &readRange, reinterpret_cast<void**>(&_mappedAddress));

    assert(SUCCEEDED(hr));
    _gpuBaseAddress = _uploadBuffer->GetGPUVirtualAddress();
}

uint64 UploadAllocator::AlignUp(uint64 value, uint64 alignment)
{
    assert(alignment != 0);
    assert((alignment & (alignment - 1)) == 0);

    return (value + alignment - 1) & ~(alignment - 1);
}

UploadAllocation UploadAllocator::Allocate(uint64 size, uint64 alignment)
{
    uint64 alignedOffset = AlignUp(_offset, alignment);

    uint64 endOffset = alignedOffset + size;
    assert(endOffset <= _capacity);

    UploadAllocation allocation;
    allocation.cpuAddress = _mappedAddress + alignedOffset;
    allocation.gpuAddress = _gpuBaseAddress + alignedOffset;
    allocation.offset = alignedOffset;
    allocation.size = size;

    _offset = endOffset;

    return allocation;
}

void UploadAllocator::Reset()
{
    _offset = 0;
}