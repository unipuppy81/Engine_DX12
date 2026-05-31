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

    void SetGpuFrameMs(float value) { _gpuFrameMs = value; }

public:
    // GPU Frame
    void Init();

    void BeginGpuTimer();
    void EndGpuTimer();
    void ResolveGpuTimer();
    void UpdateGpuResult();

private:
    // Performance
    uint32 _fps = 0;
    float _cpuFrameMs = 0.0f;
    float _gpuFrameMs = 0.0f;

    // Display smoothing
    float _displayfps = 0.0f;
    float _displayCpuFrameMs = 0.0f;
    float _displayGpuFrameMs = 0.0f;

    float _displayElapsed = 0.0f;
    float _displayInterval = 0.25f;


private:
    shared_ptr<GpuTimer> _gpuTimer = make_shared<GpuTimer>();
};

