#include "pch.h"
#include "SceneManager.h"
#include "Scene.h"

#include "DEngine.h"
#include "Material.h"
#include "GameObject.h"
#include "MeshRenderer.h"
#include "Transform.h"
#include "Camera.h"
#include "Light.h"

#include "TestCameraScript.h"
#include "Resources.h"
#include "ParticleSystem.h"
#include "Terrain.h"
#include "SphereCollider.h"
#include "BaseCollider.h"
#include "MeshData.h"
#include "TestDragon.h"
#include "TestDirectionalLight.h"

void SceneManager::Update()
{
	if (_activeScene == nullptr)
		return;

	_activeScene->Update();
	_activeScene->LateUpdate();
	_activeScene->FinalUpdate();
}

// TEMP
void SceneManager::Render()
{
	if (_activeScene)
		_activeScene->Render();
}


void SceneManager::LoadScene(wstring sceneName)
{
	// TODO : 기존 Scene 정리
	// TODO : 파일에서 Scene 정보 로드

	_activeScene = LoadTestScene();

	_activeScene->Awake();
	_activeScene->Start();
}

void SceneManager::SetLayerName(uint8 index, const wstring& name)
{
	// 기존 데이터 삭제
	const wstring& prevName = _layerNames[index];
	_layerIndex.erase(prevName);

	_layerNames[index] = name;
	_layerIndex[name] = index;
}

uint8 SceneManager::LayerNameToIndex(const wstring& name)
{
	auto findIt = _layerIndex.find(name);
	if (findIt == _layerIndex.end())
		return 0;

	return findIt->second;
}

shared_ptr<GameObject> SceneManager::Pick(int32 screenX, int32 screenY)
{
	shared_ptr<Camera> camera = GetActiveScene()->GetMainCamera();

	float width = static_cast<float>(GDEngine->GetWindow().width);
	float height = static_cast<float>(GDEngine->GetWindow().height);

	Matrix projectionMatrix = camera->GetProjectionMatrix();

	// ViewSpace에서 Picking 진행
	float viewX = (+2.0f * screenX / width - 1.0f) / projectionMatrix(0, 0);
	float viewY = (-2.0f * screenY / height + 1.0f) / projectionMatrix(1, 1);

	Matrix viewMatrix = camera->GetViewMatrix();
	Matrix viewMatrixInv = viewMatrix.Invert();

	auto& gameObjects = GET_SINGLE(SceneManager)->GetActiveScene()->GetGameObjects();

	float minDistance = FLT_MAX;
	shared_ptr<GameObject> picked;

	for (auto& gameObject : gameObjects)
	{
		if (gameObject->GetCollider() == nullptr)
			continue;

		// ViewSpace에서의 Ray 정의
		Vec4 rayOrigin = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
		Vec4 rayDir = Vec4(viewX, viewY, 1.0f, 0.0f);

		// WorldSpace에서의 Ray 정의
		rayOrigin = XMVector3TransformCoord(rayOrigin, viewMatrixInv);
		rayDir = XMVector3TransformNormal(rayDir, viewMatrixInv);
		rayDir.Normalize();

		// WorldSpace에서 연산
		float distance = 0.f;
		if (gameObject->GetCollider()->Intersects(rayOrigin, rayDir, OUT distance) == false)
			continue;

		if (distance < minDistance)
		{
			minDistance = distance;
			picked = gameObject;
		}
	}

	return picked;
}

shared_ptr<Scene> SceneManager::LoadTestScene()
{
#pragma region LayerMask
	SetLayerName(0, L"Default");
	SetLayerName(1, L"UI");
#pragma endregion

#pragma region HDR_Load_Test
	{
		// shared_ptr<Texture> hdr = GET_SINGLE(Resources)->Get<Texture>(L"HDR_Studio");
		// shared_ptr<Texture> envCube = GET_SINGLE(Resources)->CreateCubeMap(L"EnvironmentCubeMap", DXGI_FORMAT_R8G8B8A8_UNORM, 512, 1);
		// 
		// assert(hdr != nullptr);
		// assert(envCube != nullptr);
	}
#pragma endregion

#pragma region ComputeShader
	{
		shared_ptr<Shader> shader = GET_SINGLE(Resources)->Get<Shader>(L"ComputeShader");

		// UAV 용 Texture 생성
		shared_ptr<Texture> texture = GET_SINGLE(Resources)->CreateTexture(L"UAVTexture",
			DXGI_FORMAT_R8G8B8A8_UNORM, 1024, 1024,
			CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT), D3D12_HEAP_FLAG_NONE,
			D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

		shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"ComputeShader");
		material->SetShader(shader);
		material->SetInt(0, 1);
		GDEngine->GetComputeDescHeap()->SetUAV(texture->GetUAVHandle(), UAV_REGISTER::u0);

		// 쓰레드 그룹 (1 * 1024 * 1)
		material->Dispatch(1, 1024, 1);
	}
#pragma endregion

	shared_ptr<Scene> scene = make_shared<Scene>();

#pragma region Camera
	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->SetName(L"Main_Camera");
		camera->AddComponent(make_shared<Transform>());
		camera->AddComponent(make_shared<Camera>()); // Near=1, Far=1000, FOV=45도
		camera->AddComponent(make_shared<TestCameraScript>());

		camera->GetCamera()->SetFar(1000);
		camera->GetTransform()->SetLocalPosition(Vec3(100.f, 0.f, 0.f));
		uint8 layerIndex = GET_SINGLE(SceneManager)->LayerNameToIndex(L"UI");
		camera->GetCamera()->SetCullingMaskLayerOnOff(layerIndex, true); // UI는 안 찍음
		scene->AddGameObject(camera);
	}
#pragma endregion

#pragma region UI_Camera
	{
		shared_ptr<GameObject> camera = make_shared<GameObject>();
		camera->SetName(L"Orthographic_Camera");
		camera->AddComponent(make_shared<Transform>());
		camera->AddComponent(make_shared<Camera>()); // Near=1, Far=1000, 800*600
		camera->GetTransform()->SetLocalPosition(Vec3(0.f, 0.f, 0.f));
		camera->GetCamera()->SetProjectionType(PROJECTION_TYPE::ORTHOGRAPHIC);
		uint8 layerIndex = GET_SINGLE(SceneManager)->LayerNameToIndex(L"UI");
		camera->GetCamera()->SetCullingMaskAll(); // 다 끄고
		camera->GetCamera()->SetCullingMaskLayerOnOff(layerIndex, false); // UI만 찍음
		scene->AddGameObject(camera);
	}
#pragma endregion

#pragma region SkyBox
	{
		shared_ptr<GameObject> skybox = make_shared<GameObject>();
		skybox->AddComponent(make_shared<Transform>());
		skybox->GetTransform()->SetLocalScale(Vec3(500.f, 500.f, 500.f));
		skybox->GetTransform()->SetLocalPosition(Vec3(0.f, 0.f, 0.f));
		skybox->SetCheckFrustum(false);
		shared_ptr<MeshRenderer> meshRenderer = make_shared<MeshRenderer>();
		{
			shared_ptr<Mesh> sphereMesh = GET_SINGLE(Resources)->LoadCubeMesh();
			meshRenderer->SetMesh(sphereMesh);
		}
		{
			shared_ptr<Shader> shader = GET_SINGLE(Resources)->Get<Shader>(L"Skybox");
			shared_ptr<Texture> texture = GET_SINGLE(Resources)->Load<Texture>(L"Sky", L"..\\Resources\\Texture\\Sky.jpg");
			
			//shared_ptr<Shader> shader = GET_SINGLE(Resources)->Get<Shader>(L"SkyboxCube");
			//shared_ptr<Texture> texture = GET_SINGLE(Resources)->Get<Texture>(L"EnvironmentCubeMap");

			assert(shader != nullptr);
			assert(texture != nullptr);

			shared_ptr<Material> material = make_shared<Material>();
			material->SetShader(shader);
			material->SetTexture(0, texture);

			assert(material->GetShader() == shader);

			meshRenderer->SetMaterial(material);
		}
		skybox->AddComponent(meshRenderer);
		scene->AddGameObject(skybox);
	}
#pragma endregion

#pragma region Object
	{
		{
			//shared_ptr<GameObject> obj = make_shared<GameObject>();
			//obj->SetName(L"OBJ_TYPE_A"); 
			//obj->AddComponent(make_shared<Transform>());
			//obj->AddComponent(make_shared<SphereCollider>());
			//obj->GetTransform()->SetLocalPosition(Vec3(0.f, 100.f, 500.f));
			//obj->GetTransform()->SetLocalScale(Vec3(100.f, 100.f, 100.f));
			//obj->SetStatic(false);
			//shared_ptr<MeshRenderer> meshRenderer = make_shared<MeshRenderer>();
			//{
			//	shared_ptr<Mesh> sphereMesh = GET_SINGLE(Resources)->LoadSphereMesh();
			//	meshRenderer->SetMesh(sphereMesh);
			//}
			//{
			//	shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"GameObject");
			//	meshRenderer->SetMaterial(material->Clone());
			//}
			//dynamic_pointer_cast<SphereCollider>(obj->GetCollider())->SetRadius(0.5f);
			//dynamic_pointer_cast<SphereCollider>(obj->GetCollider())->SetCenter(Vec3(0.f, 0.f, 0.f));
			//obj->AddComponent(meshRenderer);
			//scene->AddGameObject(obj);
		}

		{
			// shared_ptr<GameObject> obj = make_shared<GameObject>();
			// obj->SetName(L"OBJ_TYPE_B");
			// obj->AddComponent(make_shared<Transform>());
			// obj->AddComponent(make_shared<SphereCollider>());
			// obj->GetTransform()->SetLocalScale(Vec3(20.f, 20.f, 20.f));
			// obj->GetTransform()->SetLocalPosition(Vec3(25.0f, 0.f, 300.f));
			// obj->SetStatic(false);
			// shared_ptr<MeshRenderer> meshRenderer = make_shared<MeshRenderer>();
			// {
			// 	shared_ptr<Mesh> sphereMesh = GET_SINGLE(Resources)->LoadSphereMesh();
			// 	meshRenderer->SetMesh(sphereMesh);
			// }
			// {
			// 	shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"PBR_GameObject");
			// 	meshRenderer->SetMaterial(material->Clone());
			// }
			// dynamic_pointer_cast<SphereCollider>(obj->GetCollider())->SetRadius(0.5f);
			// dynamic_pointer_cast<SphereCollider>(obj->GetCollider())->SetCenter(Vec3(0.f, 0.f, 0.f));
			// obj->AddComponent(meshRenderer);
			// scene->AddGameObject(obj);
		}

		// BenchmarkObjects
		{
			shared_ptr<Mesh> mesh = GET_SINGLE(Resources)->LoadSphereMesh();
			shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"GameObject");
			
			const int countX = 40;
			const int countY = 40;
			const float spacing = 15.f;
			for (int y = 0; y < countY; ++y)
			{
				for (int x = 0; x < countX; ++x)
				{
					shared_ptr<GameObject> obj = make_shared<GameObject>();
			
					obj->AddComponent(make_shared<Transform>());
					obj->GetTransform()->SetLocalScale(Vec3(5.f, 5.f, 5.f));
					obj->GetTransform()->SetLocalPosition(Vec3(100.f + (x - 10) * spacing, (y - 10) * spacing, 300.f));
			
					obj->SetStatic(false);
					obj->SetCheckFrustum(true);
			
					shared_ptr<MeshRenderer> renderer = make_shared<MeshRenderer>();
			
					renderer->SetMesh(mesh);
					renderer->SetMaterial(material);
			
					obj->AddComponent(renderer);
			
					scene->AddGameObject(obj);
				}
			}
		}

		// Showcase
		{
			// shared_ptr<Mesh> sphereMesh = GET_SINGLE(Resources)->LoadSphereMesh();
			// shared_ptr<Material> baseMaterial = GET_SINGLE(Resources)->Get<Material>(L"PBR_GameObject");
			// 
			// const int count = 5;
			// const float spacing = 20.f;
			// 
			// for (int y = 0; y < count; ++y)
			// {
			// 	for (int x = 0; x < count; ++x)
			// 	{
			// 		shared_ptr<GameObject> obj = make_shared<GameObject>();
			// 
			// 		obj->SetName(L"PBR_Sphere");
			// 		obj->AddComponent(make_shared<Transform>());
			// 
			// 		obj->GetTransform()->SetLocalScale(Vec3(12.f, 12.f, 12.f));
			// 		obj->GetTransform()->SetLocalPosition(Vec3(-70.f + x * spacing, 70.f - y * spacing, 300.f));
			// 
			// 		obj->SetStatic(false);
			// 		obj->SetCheckFrustum(true);
			// 
			// 		shared_ptr<MeshRenderer> renderer = make_shared<MeshRenderer>();
			// 
			// 		renderer->SetMesh(sphereMesh);
			// 
			// 		// 각 구체가 Metallic/Roughness 값이 다르므로 Clone 필요
			// 		shared_ptr<Material> material = baseMaterial->Clone();
			// 
			// 		float metallic = static_cast<float>(x) / (count - 1);
			// 		float roughness = static_cast<float>(y) / (count - 1);
			// 
			// 		material->SetPBRMetallic(metallic);
			// 		material->SetPBRRoughness(roughness);
			// 		material->SetPBRAO(1.0f);
			// 
			// 		renderer->SetMaterial(material);
			// 
			// 		obj->AddComponent(renderer);
			// 		scene->AddGameObject(obj);
			// 	}
			// }
		}


		// roughness / metallic
		{
			// shared_ptr<Mesh> sphereMesh = GET_SINGLE(Resources)->LoadSphereMesh();
			// shared_ptr<Material> baseMaterial = GET_SINGLE(Resources)->Get<Material>(L"PBR_GameObject");
			// 
			// const int count = 5;
			// const float spacing = 20.f;
			// 
			// for (int y = 0; y < count; ++y)
			// {
			// 	for (int x = 0; x < count; ++x)
			// 	{
			// 		shared_ptr<GameObject> obj = make_shared<GameObject>();
			// 
			// 		obj->SetName(L"PBR_Sphere");
			// 		obj->AddComponent(make_shared<Transform>());
			// 
			// 		obj->GetTransform()->SetLocalScale(Vec3(12.f, 12.f, 12.f));
			// 		obj->GetTransform()->SetLocalPosition(Vec3(-70.f + x * spacing, 70.f - y * spacing, 300.f));
			// 
			// 		obj->SetStatic(false);
			// 		obj->SetCheckFrustum(true);
			// 
			// 		shared_ptr<MeshRenderer> renderer = make_shared<MeshRenderer>();
			// 		renderer->SetMesh(sphereMesh);
			// 
			// 		shared_ptr<Material> material = baseMaterial->Clone();
			// 		material->SetPBRBaseColor(Vec4(0.9f, 0.5f, 0.25f, 1.f));
			// 		
			// 		float metallic = static_cast<float>(x) / (count - 1);
			// 		float roughness = static_cast<float>(y) / (count - 1);
			// 
			// 		material->SetPBRMetallic(metallic);
			// 		material->SetPBRRoughness(roughness);
			// 		material->SetPBRAO(1.0f);
			// 
			// 		renderer->SetMaterial(material);
			// 
			// 		obj->AddComponent(renderer);
			// 		scene->AddGameObject(obj);
			// 	}
			// }
		}

		
		// Shadow_Floor
		{
			// shared_ptr<GameObject> floor = make_shared<GameObject>();
			// 
			// floor->SetName(L"Shadow_Floor");
			// floor->AddComponent(make_shared<Transform>());
			// 
			// floor->GetTransform()->SetLocalScale(Vec3(400.f, 5.f, 400.f));
			// floor->GetTransform()->SetLocalPosition(Vec3(0.f, -80.f, 300.f));
			// 
			// floor->SetCheckFrustum(false);
			// floor->SetStatic(true);
			// 
			// shared_ptr<MeshRenderer> renderer = make_shared<MeshRenderer>();
			// renderer->SetMesh(GET_SINGLE(Resources)->LoadCubeMesh());
			// 
			// shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"GameObject")->Clone();
			// renderer->SetMaterial(material);
			// 
			// floor->AddComponent(renderer);
			// scene->AddGameObject(floor);
		}
	}
#pragma endregion

#pragma region Terrain
	{
		shared_ptr<GameObject> obj = make_shared<GameObject>();
		obj->AddComponent(make_shared<Transform>());
		obj->AddComponent(make_shared<Terrain>());
		obj->AddComponent(make_shared<MeshRenderer>());
		
		obj->GetTransform()->SetLocalScale(Vec3(50.f, 250.f, 50.f));
		obj->GetTransform()->SetLocalPosition(Vec3(-100.f, -200.f, 200.f));
		//obj->GetTransform()->SetLocalPosition(Vec3(0.f, 0.f, 0.f));
		obj->SetStatic(true);
		obj->GetTerrain()->Init(64, 64);
		obj->SetCheckFrustum(false);
		
		scene->AddGameObject(obj);
	}
#pragma endregion

#pragma region UI_Test
	
	for (int32 i = 0; i < 6; i++)
	{
		shared_ptr<GameObject> obj = make_shared<GameObject>();
		obj->SetLayerIndex(GET_SINGLE(SceneManager)->LayerNameToIndex(L"UI")); // UI
		obj->AddComponent(make_shared<Transform>());
		obj->GetTransform()->SetLocalScale(Vec3(100.f, 100.f, 100.f));
		obj->GetTransform()->SetLocalPosition(Vec3(-730.f + (i * 120.f), 380.f, 500.f));
		shared_ptr<MeshRenderer> meshRenderer = make_shared<MeshRenderer>();
		{
			shared_ptr<Mesh> mesh = GET_SINGLE(Resources)->LoadRectangleMesh();
			meshRenderer->SetMesh(mesh);
		}
		{
			shared_ptr<Shader> shader = GET_SINGLE(Resources)->Get<Shader>(L"Texture");
	
			shared_ptr<Texture> texture;
			if (i < 3)
			{
				texture = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::G_BUFFER)->GetRTTexture(i);
	
			}
			else if (i < 5)
			{
				texture = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::LIGHTING)->GetRTTexture(i - 3);
	
			}
			else
			{
				//texture = GET_SINGLE(Resources)->Get<Texture>(L"BRDFLUT");
				//texture = GET_SINGLE(Resources)->Get<Texture>(L"HDR_Studio");
				texture = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SHADOW)->GetRTTexture(0);
			}
	
			shared_ptr<Material> material = make_shared<Material>();
			material->SetShader(shader);
			material->SetTexture(0, texture);
			meshRenderer->SetMaterial(material);
	
			DX_LOG(L"[UI] " << i << L" SRV Index: " << texture->GetSRVIndex());
	
		}
		obj->AddComponent(meshRenderer);
		scene->AddGameObject(obj);
	}
	
#pragma endregion

#pragma region GBuffer_Debug
	{
		//for (int32 i = 0; i < 4; ++i)
		//{
		//	shared_ptr<GameObject> obj = make_shared<GameObject>();
		//
		//	obj->SetLayerIndex(GET_SINGLE(SceneManager)->LayerNameToIndex(L"UI"));
		//	obj->AddComponent(make_shared<Transform>());
		//
		//	// 1600x900 기준
		//	obj->GetTransform()->SetLocalScale(Vec3(180.f, 180.f, 100.f));
		//	obj->GetTransform()->SetLocalPosition(Vec3(-600.f + i * 400.f, 340.f, 500.f));
		//
		//	shared_ptr<MeshRenderer> renderer = make_shared<MeshRenderer>();
		//
		//	renderer->SetMesh(GET_SINGLE(Resources)->LoadRectangleMesh());
		//
		//	shared_ptr<Material> material = make_shared<Material>();
		//
		//	material->SetShader(GET_SINGLE(Resources)->Get<Shader>(L"Texture"));
		//	material->SetTexture(0,GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::G_BUFFER)->GetRTTexture(i));
		//	renderer->SetMaterial(material);
		//
		//	obj->AddComponent(renderer);
		//	scene->AddGameObject(obj);
		//}
	}
#pragma endregion

#pragma region Directional Light
	// 위에서 아래로
	{
		shared_ptr<GameObject> light = make_shared<GameObject>();
		light->AddComponent(make_shared<Transform>());
		light->GetTransform()->SetLocalPosition(Vec3(0, 1000, 500));
		light->AddComponent(make_shared<Light>());

		light->AddComponent(make_shared<TestDirectionalLight>());
		
		light->GetLight()->SetLightDirection(Vec3(0.4f, -1.f, 0.3f));
		//light->GetLight()->SetLightDirection(Vec3(0.3f, -1.f, 0.5f));
		light->GetLight()->SetLightType(LIGHT_TYPE::DIRECTIONAL_LIGHT);

		light->GetLight()->SetDiffuse(Vec3(1.f, 1.f, 1.f));
		light->GetLight()->SetAmbient(Vec3(0.3f, 0.3f, 0.3f));
		light->GetLight()->SetSpecular(Vec3(0.1f, 0.1f, 0.1f));
		//light->GetLight()->SetDiffuse(Vec3(3.f, 3.f, 3.f));
		//light->GetLight()->SetAmbient(Vec3(0.2f, 0.2f, 0.2f));
		//light->GetLight()->SetSpecular(Vec3(1.f, 1.f, 1.f));

		scene->AddGameObject(light);
	}

	// 아래에서 위로
	{
		// shared_ptr<GameObject> fillLight = make_shared<GameObject>();
		// fillLight->AddComponent(make_shared<Transform>());
		// fillLight->AddComponent(make_shared<Light>());
		// 
		// fillLight->GetLight()->SetLightType(LIGHT_TYPE::DIRECTIONAL_LIGHT);
		// 
		// // 주광 반대쪽
		// fillLight->GetLight()->SetLightDirection(Vec3(0.f, 0.5f, -1.f));
		// 
		// fillLight->GetLight()->SetDiffuse(Vec3(0.3f, 0.3f, 0.3f));
		// fillLight->GetLight()->SetAmbient(Vec3(0.f, 0.f, 0.f));
		// fillLight->GetLight()->SetSpecular(Vec3(0.1f, 0.1f, 0.1f));
		// 
		// scene->AddGameObject(fillLight);
	}
#pragma endregion

#pragma region FBX
	{
		// shared_ptr<MeshData> meshData = GET_SINGLE(Resources)->LoadFBX(L"..\\Resources\\FBX\\Dragon.fbx");
		// 
		// vector<shared_ptr<GameObject>> gameObjects = meshData->Instantiate();
		// 
		// for (auto& gameObject : gameObjects)
		// {
		// 	gameObject->SetName(L"Dragon");
		// 
		// 	gameObject->SetStatic(false);
		// 	gameObject->SetCheckFrustum(false);
		// 	gameObject->GetTransform()->SetLocalPosition(Vec3(300.f, -100.f, 500.f));
		// 	gameObject->GetTransform()->SetLocalScale(Vec3(1.f, 1, 1.f));
		// 	scene->AddGameObject(gameObject);
		// 	gameObject->AddComponent(make_shared<TestDragon>());
		// }
	}
#pragma endregion

#pragma region ParticleSystem
//	{
//		shared_ptr<GameObject> particle = make_shared<GameObject>();
//		particle->AddComponent(make_shared<Transform>());
//		particle->AddComponent(make_shared<ParticleSystem>());
//		particle->SetCheckFrustum(false);
//		particle->GetTransform()->SetLocalPosition(Vec3(0.f, 0.f, 100.f));
//		scene->AddGameObject(particle);
//	}
#pragma endregion

#pragma region NormalMap_Showcase
	{
		// shared_ptr<Mesh> sphereMesh = GET_SINGLE(Resources)->LoadSphereMesh();
		// shared_ptr<Material> baseMaterial = GET_SINGLE(Resources)->Get<Material>(L"GameObject");
		// 
		// // =========================
		// // LEFT : Normal Map OFF
		// // =========================
		// {
		// 	shared_ptr<GameObject> obj = make_shared<GameObject>();
		// 
		// 	obj->AddComponent(make_shared<Transform>());
		// 	obj->GetTransform()->SetLocalPosition(Vec3(40.f, 0.f, 500.f));
		// 	obj->GetTransform()->SetLocalScale(Vec3(80.f, 80.f, 80.f));
		// 
		// 	obj->SetStatic(false);
		// 	obj->SetCheckFrustum(false);
		// 
		// 	shared_ptr<MeshRenderer> renderer = make_shared<MeshRenderer>();
		// 
		// 	renderer->SetMesh(sphereMesh);
		// 
		// 	shared_ptr<Material> material = baseMaterial->Clone();
		// 
		// 	material->SetTexture(0, nullptr); // Albedo 제거
		// 	material->SetTexture(1, nullptr); // Normal Map OFF
		// 
		// 	renderer->SetMaterial(material);
		// 
		// 	obj->AddComponent(renderer);
		// 	scene->AddGameObject(obj);
		// }
		// 
		// // =========================
		// // RIGHT : Normal Map ON
		// // =========================
		// {
		// 	shared_ptr<GameObject> obj = make_shared<GameObject>();
		// 
		// 	obj->AddComponent(make_shared<Transform>());
		// 	obj->GetTransform()->SetLocalPosition(Vec3(160.f, 0.f, 500.f));
		// 	obj->GetTransform()->SetLocalScale(Vec3(80.f, 80.f, 80.f));
		// 
		// 	obj->SetStatic(false);
		// 	obj->SetCheckFrustum(false);
		// 
		// 	shared_ptr<MeshRenderer> renderer = make_shared<MeshRenderer>();
		// 
		// 	renderer->SetMesh(sphereMesh);
		// 
		// 	shared_ptr<Material> material = baseMaterial->Clone();
		// 
		// 	material->SetTexture(0, nullptr); // Albedo 제거
		// 	// t1은 기존 Leather_Normal 그대로 유지
		// 
		// 	renderer->SetMaterial(material);
		// 
		// 	obj->AddComponent(renderer);
		// 	scene->AddGameObject(obj);
		// }
	}
#pragma endregion

	return scene;
}