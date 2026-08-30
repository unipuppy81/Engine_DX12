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

void DiagnosticsManager::BeginGpuTimer(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList)
{
	_gpuTimer->Begin(frameIndex, cmdList);
}

void DiagnosticsManager::EndGpuTimer(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList)
{
	_gpuTimer->End(frameIndex, cmdList);
}

void DiagnosticsManager::ResolveGpuTimer(uint32 frameIndex, ID3D12GraphicsCommandList* cmdList)
{
	_gpuTimer->Resolve(frameIndex, cmdList);
}

void DiagnosticsManager::UpdateGpuResult(uint32 frameIndex)
{
	_gpuTimer->UpdateResult(frameIndex);
	_gpuFrameMs = _gpuTimer->GetGpuMs();
}

void DiagnosticsManager::AddDrawCallData(uint32 triangleCount)
{
	_drawCallCount++;
	_triangleCount += triangleCount;
}
