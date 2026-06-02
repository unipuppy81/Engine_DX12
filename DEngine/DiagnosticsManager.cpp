#include "pch.h"
#include "DiagnosticsManager.h"
#include "Timer.h"

void DiagnosticsManager::UpdateFrame(float cpuMs)
{
	uint32 fps = GET_SINGLE(Timer)->GetFps();
	float deltaTime = GET_SINGLE(Timer)->GetDeltaTime();

	_cpuFrameMs = cpuMs;
	_fps = fps;


	_displayElapsed += deltaTime;

	if (_displayElapsed >= _displayInterval)
	{
		_displayfps = fps;
		_displayCpuFrameMs = _cpuFrameMs;
		_displayGpuFrameMs = _gpuFrameMs;
		_displayDrawCallCount = _drawCallCount;
		_displayTriangleCount = _triangleCount;

		_displayElapsed = 0.0f;
	}
}

void DiagnosticsManager::Init()
{
	_gpuTimer->Init();
}

void DiagnosticsManager::Reset()
{
	_drawCallCount = 0;
	_triangleCount = 0;
	_totalObjectCount = 0;
	_visibleObjectCount = 0;
	_culledObjectCount = 0;
}

void DiagnosticsManager::BeginGpuTimer()
{
	_gpuTimer->Begin();
}

void DiagnosticsManager::EndGpuTimer()
{
	_gpuTimer->End();
}

void DiagnosticsManager::ResolveGpuTimer()
{
	_gpuTimer->Resolve();
}

void DiagnosticsManager::UpdateGpuResult()
{
	_gpuTimer->UpdateResult();
	_gpuFrameMs = _gpuTimer->GetGpuMs();
}

void DiagnosticsManager::AddDrawCallData(uint32 triangleCount)
{
	_drawCallCount++;
	_triangleCount += triangleCount;
}
