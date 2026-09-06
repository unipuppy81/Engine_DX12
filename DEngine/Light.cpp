#include "pch.h"
#include "Light.h"
#include "Transform.h"
#include "DEngine.h"
#include "Resources.h"
#include "Camera.h"
#include "Texture.h"
#include "SceneManager.h"

Light::Light() : Component(COMPONENT_TYPE::LIGHT)
{
	_shadowCamera = make_shared<GameObject>();
	_shadowCamera->AddComponent(make_shared<Transform>());
	_shadowCamera->AddComponent(make_shared<Camera>());
	uint8 layerIndex = GET_SINGLE(SceneManager)->LayerNameToIndex(L"UI");
	_shadowCamera->GetCamera()->SetCullingMaskLayerOnOff(layerIndex, true); // UI는 안 찍음
}

Light::~Light()
{
}

void Light::FinalUpdate()
{
	_lightInfo.position = GetTransform()->GetWorldPosition();

	_shadowCamera->GetTransform()->SetLocalPosition(GetTransform()->GetLocalPosition());
	_shadowCamera->GetTransform()->SetLocalRotation(GetTransform()->GetLocalRotation());
	_shadowCamera->GetTransform()->SetLocalScale(GetTransform()->GetLocalScale());

	_shadowCamera->FinalUpdate();
}

void Light::Render()
{
	assert(_lightIndex >= 0);

	GetTransform()->PushData();

	if (static_cast<LIGHT_TYPE>(_lightInfo.lightType) == LIGHT_TYPE::DIRECTIONAL_LIGHT)
	{
		shared_ptr<Texture> shadowTex = GET_SINGLE(Resources)->Get<Texture>(L"ShadowTarget");
		_lightMaterial->SetTexture(2, shadowTex);

		Matrix matVP = _shadowCamera->GetCamera()->GetViewMatrix() * _shadowCamera->GetCamera()->GetProjectionMatrix();
		_lightMaterial->SetMatrix(0, matVP);
	}
	else
	{
		float scale = 2 * _lightInfo.range;
		GetTransform()->SetLocalScale(Vec3(scale, scale, scale));
	}

	_lightMaterial->SetInt(0, _lightIndex);
	_lightMaterial->PushGraphicsData();

	_volumeMesh->Render();
}

void Light::RenderPBR()
{
	assert(_lightIndex >= 0);

	if (_pbrLightMaterial == nullptr)
		return;

	if (static_cast<LIGHT_TYPE>(_lightInfo.lightType) != LIGHT_TYPE::DIRECTIONAL_LIGHT)
		return;

	GetTransform()->PushData();

	if (static_cast<LIGHT_TYPE>(_lightInfo.lightType) == LIGHT_TYPE::DIRECTIONAL_LIGHT)
	{
		// 기존 구조 유지:
		// PBR_DirLight Material의 t0~t3은 Resources::CreateDefaultMaterial()에서 세팅한다.
		// t2는 DiffuseTarget/Albedo로 사용한다.
		// Shadow는 PBR에 아직 연결하지 않는다.
		//shared_ptr<Texture> shadowTex = GET_SINGLE(Resources)->Get<Texture>(L"ShadowTarget");
		//_pbrLightMaterial->SetTexture(2, shadowTex);

		Matrix matVP = _shadowCamera->GetCamera()->GetViewMatrix() * _shadowCamera->GetCamera()->GetProjectionMatrix();
		_pbrLightMaterial->SetMatrix(0, matVP);
	}
	else
	{
		float scale = 2 * _lightInfo.range;
		GetTransform()->SetLocalScale(Vec3(scale, scale, scale));
	}

	_pbrLightMaterial->SetInt(0, _lightIndex);
	_pbrLightMaterial->PushGraphicsData();

	_volumeMesh->Render();
}

void Light::RenderShadow()
{
	auto t0 = chrono::steady_clock::now();

	_shadowCamera->GetCamera()->SortShadowObject();

	auto t1 = chrono::steady_clock::now();

	_shadowCamera->GetCamera()->Render_Shadow();

	auto t2 = chrono::steady_clock::now();

	_shadowSortMs = chrono::duration<float, milli>(t1 - t0).count();
	_shadowRenderMs = chrono::duration<float, milli>(t2 - t1).count();
}

void Light::SetLightDirection(Vec3 direction)
{
	direction.Normalize();

	_lightInfo.direction = direction;

	GetTransform()->LookAt(direction);
}

void Light::SetLightType(LIGHT_TYPE type)
{
	_lightInfo.lightType = static_cast<int32>(type);

	switch (type)
	{
	case LIGHT_TYPE::DIRECTIONAL_LIGHT:
		_volumeMesh = GET_SINGLE(Resources)->Get<Mesh>(L"Rectangle");
		_lightMaterial = GET_SINGLE(Resources)->Get<Material>(L"DirLight");
		_pbrLightMaterial = GET_SINGLE(Resources)->Get<Material>(L"PBR_DirLight");

		_shadowCamera->GetCamera()->SetProjectionType(PROJECTION_TYPE::ORTHOGRAPHIC); 
		_shadowCamera->GetCamera()->SetScale(1.f);

		_shadowCamera->GetCamera()->SetWidth(1000.f);
		_shadowCamera->GetCamera()->SetHeight(1000.f);

		_shadowCamera->GetCamera()->SetNear(1.f);
		_shadowCamera->GetCamera()->SetFar(3000.f);
		break;
	case LIGHT_TYPE::POINT_LIGHT:
		_volumeMesh = GET_SINGLE(Resources)->Get<Mesh>(L"Sphere");

		_lightMaterial = GET_SINGLE(Resources)->Get<Material>(L"PointLight");
		_pbrLightMaterial = GET_SINGLE(Resources)->Get<Material>(L"PBR_PointLight");
		break;
	case LIGHT_TYPE::SPOT_LIGHT:
		_volumeMesh = GET_SINGLE(Resources)->Get<Mesh>(L"Sphere");

		_lightMaterial = GET_SINGLE(Resources)->Get<Material>(L"PointLight");
		_pbrLightMaterial = GET_SINGLE(Resources)->Get<Material>(L"PBR_PointLight");
		break;
	}
}