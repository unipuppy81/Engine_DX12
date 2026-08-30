#pragma once
#include "InstancingBuffer.h"

class GameObject;

class InstancingManager
{
	DECLARE_SINGLE(InstancingManager);

public:
	void Render(vector<shared_ptr<GameObject>>& gameObjects);

	void ClearBuffer();
	void Clear();

	void SetEnabled(bool enabled) { _enabled = enabled; }
	bool IsEnabled() const { return _enabled; }
private:
	void AddParam(uint64 instanceId, InstancingParams& data);

private:
	bool _enabled = true;
	mutex _mutex;

	map<uint64/*instanceId*/, shared_ptr<InstancingBuffer>> _buffers;
};

