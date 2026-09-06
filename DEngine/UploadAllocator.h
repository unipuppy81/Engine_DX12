#pragma once

struct UploadAllocation
{
    BYTE* cpuAddress = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = 0;

    uint64 offset = 0;
    uint64 size = 0;
};

class UploadAllocator
{
public:
    UploadAllocator() = default;
    ~UploadAllocator();

public:
    void Init(ID3D12Device* device, uint64 capacity);
    void Reset();

    UploadAllocation Allocate(uint64 size, uint64 alignment = 1);

private:
    static uint64 AlignUp(uint64 value, uint64 alignment);

private:
    mutex _mutex;

    ComPtr<ID3D12Resource> _uploadBuffer;

    BYTE* _mappedAddress = nullptr;

    uint64 _capacity = 0;
    uint64 _offset = 0;

    D3D12_GPU_VIRTUAL_ADDRESS _gpuBaseAddress = 0;

    atomic<uint64_t> _lockWaitNs{ 0 };
    atomic<uint64_t> _allocationCount{ 0 };


public:
    uint64_t ConsumeLockWaitNs()
    {
        return _lockWaitNs.exchange(0);
    }

    uint64_t ConsumeAllocationCount()
    {
        return _allocationCount.exchange(0);
    }
};

