#pragma once
#include "GpuTimer.h"

class DiagnosticsManager
{
    DECLARE_SINGLE(DiagnosticsManager)

public:
    void UpdateFrame(float cpuMs);

    float GetFps() const { return _displayfps; }
    float GetCpuFrameMs() const { return _displayCpuFrameMs; }
    float GetGpuFrameMs() const { return _displayGpuFrameMs; }
    uint32 GetDrawCallCount() const { return _drawCallCount.load(); }
    uint64 GetTriangleCount() const { return _triangleCount.load(); } 
    float GetRenderPrepareMs() const { return _renderPrepareMs; }
    float GetRenderGraphCompileMs() const { return _renderGraphCompileMs; }
    float GetCommandRecordMs() const { return _commandRecordMs; }

    uint32 GetTotalObjectCount() const { return _totalObjectCount; }
    uint32 GetVisibleObjectCount() const { return _visibleObjectCount; }
    uint32 GetCulledObjectCount() const { return _culledObjectCount; }
    uint32 GetPassCount() const { return _passCount; }
    uint32 GetBarrierCount() const { return _barrierCount; }

    void SetGpuFrameMs(float value) { _gpuFrameMs = value; }
    void SetRenderPrepareMs(float ms) { _renderPrepareMs = ms; }   
    void SetRenderGraphCompileMs(float ms) { _renderGraphCompileMs = ms; }
    void SetCommandRecordMs(float ms) { _commandRecordMs = ms; }
    
    void SetObjectCount(uint32 total, uint32 visible, uint32 culled)
    {
        _totalObjectCount = total;
        _visibleObjectCount = visible;
        _culledObjectCount = culled;
    }
    
    void SetRenderGraphStats(uint32 passCount, uint32 barrierCount)
    {
        _passCount = passCount;
        _barrierCount = barrierCount;
    }

public:
    // GPU Frame
    void Init();
    void Reset();

    void BeginGpuTimer(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList);
    void EndGpuTimer(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList);
    void ResolveGpuTimer(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList);
    void UpdateGpuResult(uint32 frameIndex);

    void AddDrawCallData(uint32 triangleCount);


private:
    // Performance
    uint32 _fps = 0;
    float _cpuFrameMs = 0.0f;
    float _gpuFrameMs = 0.0f;

    // DrawCall
    atomic<uint32> _drawCallCount = 0;
    atomic<uint64> _triangleCount = 0;

    uint32 _totalObjectCount = 0;
    uint32 _visibleObjectCount = 0;
    uint32 _culledObjectCount = 0;



    // Display 
    uint32 _displayfps = 0.0f;
    float _displayCpuFrameMs = 0.0f;
    float _displayGpuFrameMs = 0.0f;
    uint32 _displayTriangleCount = 0;
    uint32 _displayDrawCallCount = 0;
    float _renderPrepareMs = 0.0f;
    float _renderGraphCompileMs = 0.0f;
    float _commandRecordMs = 0.0f;
    uint32 _passCount = 0;
    uint32 _barrierCount = 0;


    float _displayElapsed = 0.0f;
    float _displayInterval = 0.25f;


private:
    shared_ptr<GpuTimer> _gpuTimer = make_shared<GpuTimer>();
};

