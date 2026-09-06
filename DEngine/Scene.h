#pragma once

class GameObject;
class Camera;
class RenderGraph;

class Scene
{
public:
	void Awake();
	void Start();
	void Update();
	void LateUpdate();
	void FinalUpdate();

	shared_ptr<class Camera> GetMainCamera();

	void Render();

	void RenderAll();

	void RenderShadow();
	void RenderDeferred();
	void RenderLights();
	void RenderFinal();

	void RenderForward();

private:
	void PushLightData();

public:
	void AddGameObject(shared_ptr<GameObject> gameObject);
	void RemoveGameObject(shared_ptr<GameObject> gameObject);

	const vector<shared_ptr<GameObject>>& GetGameObjects() { return _gameObjects; }

	// TEST
private:
	bool _isIBLBaked = false;

	void BakeIBLIfNeeded();
	void ConvertHDRToCube_Test();

private:
	vector<shared_ptr<GameObject>> _gameObjects;
	vector<shared_ptr<class Camera>>	_cameras;
	vector<shared_ptr<class Light>>		_lights;


public:
	float _forwardOMMs = 0.f;
	float _forwardMainMs = 0.f;
	float _forwardOtherMs = 0.f;

	float _otherSortMs = 0.f;
	float _otherRenderMs = 0.f;

	float _shadowOMMs = 0.f;
	float _shadowClearMs = 0.f;
	float _shadowLightMs = 0.f;
};

