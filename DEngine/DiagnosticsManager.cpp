#include "pch.h"
#include "DiagnosticsManager.h"
#include "Timer.h"

void DiagnosticsManager::UpdateFrame(float cpuMs)
{
	uint32 fps = GET_SINGLE(Timer)->GetFps();
	float deltaTime = GET_SINGLE(Timer)->GetDeltaTime();

	_cpuFrameMs = cpuMs;
	_fps = static_cast<float>(fps);

	_displayElapsed += deltaTime;

	if (_displayElapsed >= _displayInterval)
	{
		_displayfps = _fps;
		_displayCpuFrameMs = _cpuFrameMs;
		_displayGpuFrameMs = _gpuFrameMs;

		_displayElapsed = 0.0f;
	}
}

void DiagnosticsManager::Init()
{
	_gpuTimer->Init();
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