#pragma once

#include <deque>

class DeferredReleaseQueue
{
private:
	struct ReleaseBatch
	{
		uint64 fenceValue = 0;
		vector<ComPtr<IUnknown>> resources;
	};

public:
	template<typename T>
	void Enqueue(ComPtr<T>& resource)
	{
		if (resource == nullptr)
			return;

		ComPtr<IUnknown> unknownResource;

		HRESULT hr = resource.As(&unknownResource);
		assert(SUCCEEDED(hr));

		// 원래 소유자는 참조 해제
		resource.Reset();

		// Queue가 마지막 참조를 보관
		_pendingResources.push_back(
			std::move(unknownResource)
		);
	}

	void Commit(uint64 fenceValue);
	void Process(uint64 completedFenceValue);

	// GPU가 완전히 정지한 뒤에만 호출
	void Clear();

private:
	vector<ComPtr<IUnknown>> _pendingResources;
	deque<ReleaseBatch> _releaseBatches;
};