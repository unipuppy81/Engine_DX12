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

private:
	void AddParam(uint64 instanceId, InstancingParams& data);

private:
	mutex _mutex;

	map<uint64/*instanceId*/, shared_ptr<InstancingBuffer>> _buffers;
};

