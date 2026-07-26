#include "pch.h"
#include "DeferredReleaseQueue.h"

void DeferredReleaseQueue::Commit(uint64 fenceValue)
{
	if (_pendingResources.empty() && _pendingCallbacks.empty())
		return;

	ReleaseBatch batch;
	batch.fenceValue = fenceValue;
	batch.resources = std::move(_pendingResources);
	batch.callbacks = std::move(_pendingCallbacks);

	_releaseBatches.push_back(std::move(batch));

	_pendingResources.clear();
	_pendingCallbacks.clear();
}

void DeferredReleaseQueue::Process(uint64 completedFenceValue)
{
	while (!_releaseBatches.empty())
	{
		ReleaseBatch& batch = _releaseBatches.front();

		if (completedFenceValue < batch.fenceValue)
			break;

		for (auto& callback : batch.callbacks)
			callback();

		// deque에서 제거되면서 ComPtr들이 실제 해제됨
		_releaseBatches.pop_front();
	}
}

void DeferredReleaseQueue::Clear()
{
	_pendingResources.clear();
	_releaseBatches.clear();
}