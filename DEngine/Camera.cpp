#include "pch.h"
#include "Camera.h"
#include "Transform.h"
#include "Scene.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "DEngine.h"
#include "Material.h"
#include "Shader.h"
#include "ParticleSystem.h"
#include "InstancingManager.h"
#include "DiagnosticsManager.h"

#include <iostream>

thread_local Matrix Camera::S_MatView;
thread_local Matrix Camera::S_MatProjection;

Camera::Camera() : Component(COMPONENT_TYPE::CAMERA)
{
	_width = static_cast<float>(GDEngine->GetWindow().width);
	_height = static_cast<float>(GDEngine->GetWindow().height);
}

Camera::~Camera()
{
}

void Camera::FinalUpdate()
{
	// VIEW 행렬 : CAMERA 의 WORLD 행렬에 역행렬
	_matView = GetTransform()->GetLocalToWorldMatrix().Invert();

	// 투영 변환 행렬
	if (_type == PROJECTION_TYPE::PERSPECTIVE)
		_matProjection = ::XMMatrixPerspectiveFovLH(_fov, _width / _height, _near, _far);
	else
		_matProjection = ::XMMatrixOrthographicLH(_width * _scale, _height * _scale, _near, _far);

	_frustum.FinalUpdate(_matView, _matProjection);
}

void Camera::SortGameObject()
{
	shared_ptr<Scene> scene = GET_SINGLE(SceneManager)->GetActiveScene();
	const vector<shared_ptr<GameObject>>& gameObjects = scene->GetGameObjects();

	_vecForward.clear();
	_vecDeferred.clear();
	_vecParticle.clear();

	uint32 total = 0;
	uint32 visible = 0;
	uint32 frustumCulled = 0;

	for (auto& gameObject : gameObjects)
	{
		// 렌더 대상 아니면 제외
		if (gameObject->GetMeshRenderer() == nullptr && gameObject->GetParticleSystem() == nullptr)
			continue;

		// camera layer에 안 보이는 객체 frustum 처리
		if (IsCulled(gameObject->GetLayerIndex()))
			continue;


		// Frustum test 대상 전체
		total++;

		if (_enableFrustumCulling && gameObject->GetCheckFrustum())
		{
			if (_frustum.ContainsSphere(
				gameObject->GetTransform()->GetWorldPosition(),
				gameObject->GetTransform()->GetBoundingSphereRadius()) == false)
			{
				frustumCulled++;
				continue;
			}
		}

		visible++;

		if (gameObject->GetMeshRenderer())
		{
			SHADER_TYPE shaderType = gameObject->GetMeshRenderer()->GetMaterial()->GetShader()->GetShaderType();
			switch (shaderType)
			{
			case SHADER_TYPE::DEFERRED:
				_vecDeferred.push_back(gameObject);
				break;
			case SHADER_TYPE::FORWARD:
				_vecForward.push_back(gameObject);
				break;
			}
		}
		else
		{
			_vecParticle.push_back(gameObject);
		}

	}
	//GET_SINGLE(DiagnosticsManager)->SetObjectCount(total, visible, frustumCulled);
	_totalCount = total;
	_visibleCount = visible;
	_frustumCulledCount = frustumCulled;
}

void Camera::SortShadowObject()
{
	shared_ptr<Scene> scene = GET_SINGLE(SceneManager)->GetActiveScene();
	const vector<shared_ptr<GameObject>>& gameObjects = scene->GetGameObjects();

	_vecShadow.clear();

	for (auto& gameObject : gameObjects)
	{
		if (gameObject->GetMeshRenderer() == nullptr)
			continue;

		if (gameObject->IsStatic())
			continue;

		if (IsCulled(gameObject->GetLayerIndex()))
			continue;

		if (gameObject->GetCheckFrustum())
		{
			if (_frustum.ContainsSphere(
				gameObject->GetTransform()->GetWorldPosition(),
				gameObject->GetTransform()->GetBoundingSphereRadius()) == false)
			{
				continue;
			}
		}

		_vecShadow.push_back(gameObject);
	}
}

void Camera::Render_Deferred()
{
	S_MatView = _matView;
	S_MatProjection = _matProjection;

	GET_SINGLE(InstancingManager)->Render(_vecDeferred);
}

void Camera::Render_Forward()
{
	S_MatView = _matView;
	S_MatProjection = _matProjection;

	// DX_LOG(L"[CULLING] Deferred Visible Count: " << _vecForward.size());

	GET_SINGLE(InstancingManager)->Render(_vecForward);

	for (auto& gameObject : _vecParticle)
	{
		gameObject->GetParticleSystem()->Render();
	}
}

void Camera::Render_Shadow()
{
	S_MatView = _matView;
	S_MatProjection = _matProjection;

	for (auto& gameObject : _vecShadow)
	{
		gameObject->GetMeshRenderer()->RenderShadow();
	}
}