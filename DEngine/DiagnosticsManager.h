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
    uint32 GetDrawCallCount() const { return _drawCallCount; }
    uint32 GetTriangleCount() const { return _triangleCount; }

    void SetGpuFrameMs(float value) { _gpuFrameMs = value; }


public:
    // GPU Frame
    void Init();
    void Reset();

    void BeginGpuTimer();
    void EndGpuTimer();
    void ResolveGpuTimer();
    void UpdateGpuResult();

    void AddDrawCallData(uint32 triangleCount);


private:
    // Performance
    uint32 _fps = 0;
    float _cpuFrameMs = 0.0f;
    float _gpuFrameMs = 0.0f;

    // DrawCall
    uint32 _drawCallCount = 0;
    uint32 _triangleCount = 0;

    uint32 _totalObjectCount = 0;
    uint32 _visibleObjectCount = 0;
    uint32 _culledObjectCount = 0;



    // Display 
    uint32 _displayfps = 0.0f;
    float _displayCpuFrameMs = 0.0f;
    float _displayGpuFrameMs = 0.0f;
    uint32 _displayTriangleCount = 0;
    uint32 _displayDrawCallCount = 0;


    float _displayElapsed = 0.0f;
    float _displayInterval = 0.25f;


private:
    shared_ptr<GpuTimer> _gpuTimer = make_shared<GpuTimer>();
};

