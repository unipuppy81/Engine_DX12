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
		_displayDrawCallCount = _drawCallCount.load(memory_order_relaxed);
		_displayTriangleCount = _triangleCount.load(memory_order_relaxed);

		_displayElapsed = 0.0f;
	}
}

void DiagnosticsManager::Init()
{
	_gpuTimer->Init();
}

void DiagnosticsManager::Reset()
{
	_drawCallCount.store(0, memory_order_relaxed);
	_triangleCount.store(0, memory_order_relaxed);

	_totalObjectCount = 0;
	_visibleObjectCount = 0;
	_culledObjectCount = 0;

	_renderPrepareMs = 0.0f;
	_renderGraphCompileMs = 0.0f;
	_commandRecordMs = 0.0f;

	_passCount = 0;
	_barrierCount = 0;
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
	_drawCallCount.fetch_add(1, memory_order_relaxed);
	_triangleCount.fetch_add(triangleCount, memory_order_relaxed);	
}
