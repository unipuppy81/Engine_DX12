#include "pch.h"
#include "Scene.h"
#include "GameObject.h"
#include "Camera.h"
#include "DEngine.h"
#include "ConstantBuffer.h"
#include "Light.h"
#include "Resources.h"
#include "RenderGraph.h"

void Scene::Awake()
{
	for (const shared_ptr<GameObject>& gameObject : _gameObjects)
	{
		gameObject->Awake();
	}
}

void Scene::Start()
{
	for (const shared_ptr<GameObject>& gameObject : _gameObjects)
	{
		gameObject->Start();
	}
}

void Scene::Update()
{
	for (const shared_ptr<GameObject>& gameObject : _gameObjects)
	{
		gameObject->Update();
	}
}

void Scene::LateUpdate()
{
	for (const shared_ptr<GameObject>& gameObject : _gameObjects)
	{
		gameObject->LateUpdate();
	}
}

void Scene::FinalUpdate()
{
	for (const shared_ptr<GameObject>& gameObject : _gameObjects)
	{
		gameObject->FinalUpdate();
	}
}

shared_ptr<class Camera> Scene::GetMainCamera()
{
	if (_cameras.empty())
		return nullptr;

	return _cameras[0];
}

void Scene::Render()
{
	PushLightData();

	ClearRTV();

	BakeIBLIfNeeded();


	// RenderGraph
	RenderGraph graph(GRAPHICS_CMD_LIST.Get());
	

	// Shadow
	auto shadowGroup = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SHADOW);
	auto rgShadowRT = graph.ImportTexture(shadowGroup->GetRTTexture(0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	auto rgShadowDepth = graph.ImportTexture(shadowGroup->GetDSTexture(), D3D12_RESOURCE_STATE_DEPTH_WRITE);

	graph.AddPass("Shadow",
		[&](RenderGraphBuilder& builder)
		{
			builder.Write(rgShadowRT, RGResourceUsage::RenderTarget);
			builder.Write(rgShadowDepth, RGResourceUsage::DepthWrite);
		},
		[&](ID3D12GraphicsCommandList*)
		{
			RenderShadow();
		});

	// G-Buffer
	auto gBufferGroup = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::G_BUFFER);

	auto rgGBuffer0 = graph.ImportTexture(gBufferGroup->GetRTTexture(0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	auto rgGBuffer1 = graph.ImportTexture(gBufferGroup->GetRTTexture(1), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	auto rgGBuffer2 = graph.ImportTexture(gBufferGroup->GetRTTexture(2), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	auto rgGBuffer3 = graph.ImportTexture(gBufferGroup->GetRTTexture(3), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	graph.AddPass("GBuffer",
		[&](RenderGraphBuilder& builder)
		{
			builder.Write(rgGBuffer0, RGResourceUsage::RenderTarget);
			builder.Write(rgGBuffer1, RGResourceUsage::RenderTarget);
			builder.Write(rgGBuffer2, RGResourceUsage::RenderTarget);
			builder.Write(rgGBuffer3, RGResourceUsage::RenderTarget);
		},

		[&](ID3D12GraphicsCommandList*)
		{
			RenderDeferred();
		});


	// Light
	auto lightingGroup = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::LIGHTING);
	auto rgLighting = graph.ImportTexture(lightingGroup->GetRTTexture(0), D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	graph.AddPass("Lighting",
		[&](RenderGraphBuilder& builder)
		{
			// GBuffer 읽기
			builder.Read(rgGBuffer0, RGResourceUsage::PixelSRV);
			builder.Read(rgGBuffer1, RGResourceUsage::PixelSRV);
			builder.Read(rgGBuffer2, RGResourceUsage::PixelSRV);
			builder.Read(rgGBuffer3, RGResourceUsage::PixelSRV);

			// 실제 Lighting Shader가 읽는 Shadow Texture를 넣기
			builder.Read(rgShadowRT, RGResourceUsage::PixelSRV);

			// Lighting 결과
			builder.Write(rgLighting, RGResourceUsage::RenderTarget);
		},

		[&](ID3D12GraphicsCommandList*)
		{
			RenderLights();
		});


	// Final
	int8 backIndex = GDEngine->GetSwapChain()->GetBackBufferIndex();
	auto swapChainGroup = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SWAP_CHAIN);
	auto rgBackBuffer = graph.ImportTexture(swapChainGroup->GetRTTexture(backIndex), D3D12_RESOURCE_STATE_RENDER_TARGET);

	graph.AddPass("Final",
		[&](RenderGraphBuilder& builder)
		{
			// Lighting 결과 읽기
			builder.Read(rgLighting, RGResourceUsage::PixelSRV);
			// BackBuffer에 출력
			builder.Write(rgBackBuffer,RGResourceUsage::RenderTarget);
		},

		[&](ID3D12GraphicsCommandList*)
		{
			RenderFinal();
		});

	graph.Execute();




	//RenderShadow();
	//RenderDeferred();
	//RenderLights();
	//RenderFinal();
	RenderForward();
}

void Scene::ClearRTV()
{
	// SwapChain Group 초기화
	int8 backIndex = GDEngine->GetSwapChain()->GetBackBufferIndex();
	GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SWAP_CHAIN)->ClearRenderTargetView(backIndex);

	// Shadow Group 초기화
	GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SHADOW)->ClearRenderTargetView();

	// Deferred Group 초기화
	GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::G_BUFFER)->ClearRenderTargetView();

	// Lighting Group 초기화
	GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::LIGHTING)->ClearRenderTargetView();
}

void Scene::RenderShadow()
{
	auto shadowGroup = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SHADOW);
	
	shadowGroup->OMSetRenderTargets(); 
	//shadowGroup->ClearRenderTargetView();

	for (auto& light : _lights)
	{
		if (light->GetLightType() != LIGHT_TYPE::DIRECTIONAL_LIGHT)
			continue;

		light->RenderShadow();
	}

	//GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SHADOW)->WaitTargetToResource();
}

void Scene::RenderDeferred()
{
	auto gBufferGroup = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::G_BUFFER);

	gBufferGroup->OMSetRenderTargets();
	//gBufferGroup->ClearRenderTargetView();
	
	shared_ptr<Camera> mainCamera = _cameras[0];
	mainCamera->SortGameObject();
	mainCamera->Render_Deferred();
	//gBufferGroup->WaitTargetToResource();

	//GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::G_BUFFER)->WaitTargetToResource();
}

void Scene::RenderLights()
{
	shared_ptr<Camera> mainCamera = _cameras[0];

	Camera::S_MatView = mainCamera->GetViewMatrix();
	Camera::S_MatProjection = mainCamera->GetProjectionMatrix();

	auto lightingGroup = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::LIGHTING);

	lightingGroup->OMSetRenderTargets();
	//lightingGroup->ClearRenderTargetView();

	for (auto& light : _lights)
	{
		light->Render();
		light->RenderPBR();
	}

	//GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::LIGHTING)->WaitTargetToResource();
}

void Scene::RenderFinal()
{
	shared_ptr<Camera> mainCamera = _cameras[0];

	CameraParams cameraParams = {};
	cameraParams.matView = mainCamera->GetViewMatrix();
	cameraParams.matProjection = mainCamera->GetProjectionMatrix();
	cameraParams.matViewInv = cameraParams.matView.Invert();

	CONST_BUFFER(CONSTANT_BUFFER_TYPE::CAMERA)->PushGraphicsData(&cameraParams, sizeof(CameraParams));

	// Swapchain OMSet
	int8 backIndex = GDEngine->GetSwapChain()->GetBackBufferIndex();

	auto swapChainGroup = GDEngine->GetRTGroup(RENDER_TARGET_GROUP_TYPE::SWAP_CHAIN);
	swapChainGroup->OMSetRenderTargets(1, backIndex);

	// ClearRTV()에서 여기로 이동
	// swapChainGroup->ClearRenderTargetView(backIndex);

	//GET_SINGLE(Resources)->Get<Material>(L"Final")->PushGraphicsData();
	GET_SINGLE(Resources)->Get<Material>(L"PBR_Final")->PushGraphicsData();
	GET_SINGLE(Resources)->Get<Mesh>(L"Rectangle")->Render();
}

void Scene::RenderForward()
{
	shared_ptr<Camera> mainCamera = _cameras[0];

	for (auto& camera : _cameras)
	{
		if (camera == mainCamera)
		{
			mainCamera->Render_Forward();
			continue;
		}
		
		camera->SortGameObject();
		camera->Render_Forward();
	}
}

void Scene::PushLightData()
{
	LightParams lightParams = {};

	for (auto& light : _lights)
	{
		const LightInfo& lightInfo = light->GetLightInfo();

		light->SetLightIndex(lightParams.lightCount);

		lightParams.lights[lightParams.lightCount] = lightInfo;
		lightParams.lightCount++;
	}

	// Root_Constant_Buffer 에 저장
	CONST_BUFFER(CONSTANT_BUFFER_TYPE::GLOBAL)->SetGraphicsGlobalData(&lightParams, sizeof(lightParams));
}

void Scene::AddGameObject(shared_ptr<GameObject> gameObject)
{
	if (gameObject->GetCamera() != nullptr)
	{
		_cameras.push_back(gameObject->GetCamera());
	}
	else if (gameObject->GetLight() != nullptr)
	{
		_lights.push_back(gameObject->GetLight());
	}

	_gameObjects.push_back(gameObject);
}

void Scene::RemoveGameObject(shared_ptr<GameObject> gameObject)
{
	if (gameObject->GetCamera())
	{
		auto findIt = std::find(_cameras.begin(), _cameras.end(), gameObject->GetCamera());
		if (findIt != _cameras.end())
			_cameras.erase(findIt);
	}
	else if (gameObject->GetLight())
	{
		auto findIt = std::find(_lights.begin(), _lights.end(), gameObject->GetLight());
		if (findIt != _lights.end())
			_lights.erase(findIt);
	}

	auto findIt = std::find(_gameObjects.begin(), _gameObjects.end(), gameObject);
	if (findIt != _gameObjects.end())
		_gameObjects.erase(findIt);
}	


void Scene::BakeIBLIfNeeded()
{
	if (_isIBLBaked)
		return;

	ConvertHDRToCube_Test();

	_isIBLBaked = true;
}

void Scene::ConvertHDRToCube_Test()
{
	shared_ptr<Texture> envCube = GET_SINGLE(Resources)->Get<Texture>(L"EnvironmentCubeMap");
	assert(envCube != nullptr);
	assert(envCube->GetTex2D() != nullptr);

	shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"IBL_Environment");
	assert(material != nullptr);

	shared_ptr<Mesh> cubeMesh = GET_SINGLE(Resources)->LoadCubeMesh();
	assert(cubeMesh != nullptr);

	ComPtr<ID3D12GraphicsCommandList> cmdList = GRAPHICS_CMD_LIST;
	assert(cmdList != nullptr);

	D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		envCube->GetTex2D().Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_RENDER_TARGET);

	cmdList->ResourceBarrier(1, &barrier);

	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.f;
	viewport.TopLeftY = 0.f;
	viewport.Width = 512.f;
	viewport.Height = 512.f;
	viewport.MinDepth = 0.f;
	viewport.MaxDepth = 1.f;

	D3D12_RECT scissor = {};
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = 512;
	scissor.bottom = 512;

	cmdList->RSSetViewports(1, &viewport);
	cmdList->RSSetScissorRects(1, &scissor);

	float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };

	for (uint32 face = 0; face < 6; face++)
	{
		uint32 srcFace = face;

		// +Y / -Y 방향만 서로 교체해서 저장
		if (face == 2)
			srcFace = 3;
		else if (face == 3)
			srcFace = 2;

		IBLCubemapParams cubeParams = {};
		cubeParams.cubeFaceIndex = static_cast<int32>(srcFace);

		GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::IBL_CUBEMAP)->PushGraphicsData(&cubeParams, sizeof(cubeParams));

		D3D12_CPU_DESCRIPTOR_HANDLE rtv = envCube->GetRTVHandle(face);

		cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
		cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

		material->PushGraphicsData();
		cubeMesh->Render();
	}

	barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		envCube->GetTex2D().Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

	cmdList->ResourceBarrier(1, &barrier);
}