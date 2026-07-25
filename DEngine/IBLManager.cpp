#include "pch.h"
#include "IBLManager.h"
#include "Resources.h"
#include "Texture.h"
#include "Material.h"
#include "DEngine.h"
#include "ConstantBuffer.h"

void IBLManager::Init()
{
	CreateEnvironmentCube();
	CreateIrradianceMap();
	//CreatePrefilteredMap();
	//CreateBRDFLUT();
}

void IBLManager::CreateEnvironmentCube()
{
    _environmentMap = GET_SINGLE(Resources)->Get<Texture>(L"EnvironmentCubeMap");
    shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"IBL_Environment");

    assert(_environmentMap != nullptr);
    assert(_environmentMap->GetTex2D() != nullptr);
    assert(material != nullptr);

    ComPtr<ID3D12GraphicsCommandList> cmdList = GRAPHICS_CMD_LIST;
    assert(cmdList != nullptr);

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        _environmentMap->GetTex2D().Get(),
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

    const float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };

    for (uint32 face = 0; face < 6; ++face)
    {
        uint32 directionFace = face;
     
        if (face == 2)
            directionFace = 3;
        else if (face == 3)
            directionFace = 2;

        IBLCubemapParams cubeParams = {};
        cubeParams.cubeFaceIndex = directionFace;

        GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::IBL_CUBEMAP)->PushGraphicsData(&cubeParams, sizeof(cubeParams));

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = _environmentMap->GetRTVHandle(face);

        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        material->PushGraphicsData();

        // VS   SV_VertexID   Fullscreen Triangle     
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawInstanced(3, 1, 0, 0);
    }

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        _environmentMap->GetTex2D().Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmdList->ResourceBarrier(1, &barrier);
}

void IBLManager::CreateIrradianceMap()
{
    _irradianceMap = GET_SINGLE(Resources)->Get<Texture>(L"IrradianceMap");
    shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"IBL_Irradiance");

    assert(_environmentMap != nullptr);
    assert(_irradianceMap != nullptr);
    assert(_irradianceMap->GetTex2D() != nullptr);
    assert(material != nullptr);
      
    material->SetTexture(0, _environmentMap);

    ComPtr<ID3D12GraphicsCommandList> cmdList = GRAPHICS_CMD_LIST;

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        _irradianceMap->GetTex2D().Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmdList->ResourceBarrier(1, &barrier);

    constexpr uint32 irradianceSize = 32;

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(irradianceSize);
    viewport.Height = static_cast<float>(irradianceSize);
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;

    D3D12_RECT scissor = {};
    scissor.right = irradianceSize;
    scissor.bottom = irradianceSize;

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    const float clearColor[4] = { 1.f, 0.f, 0.f, 1.f };
    
    for (uint32 face = 0; face < 6; ++face)
    {
        uint32 directionFace = face;
        if (face == 2) directionFace = 3;
        else if (face == 3) directionFace = 2;

        IBLCubemapParams cubeParams = {};
        cubeParams.cubeFaceIndex = directionFace;

        D3D12_CPU_DESCRIPTOR_HANDLE rtv = _irradianceMap->GetRTVHandle(face);

        // RTV 먼저 바인딩
        cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

        // t0, b2, b3, PSO
        material->PushGraphicsData();

        // b4
        GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::IBL_CUBEMAP)
            ->PushGraphicsData(&cubeParams, sizeof(cubeParams));

        // descriptor table 실제 바인딩
        GDEngine->GetGraphicsDescHeap()->CommitTable();

        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList->DrawInstanced(3, 1, 0, 0);
    }

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        _irradianceMap->GetTex2D().Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmdList->ResourceBarrier(1, &barrier);
}

void IBLManager::CreatePrefilteredMap()
{
    _prefilteredMap = GET_SINGLE(Resources)->Get<Texture>(L"PrefilteredMap");
    shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"IBL_Prefilter");

    assert(_environmentMap != nullptr);
    assert(_prefilteredMap != nullptr);
    assert(material != nullptr);

    material->SetTexture(0, _environmentMap);

    ComPtr<ID3D12GraphicsCommandList> cmdList = GRAPHICS_CMD_LIST;

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        _prefilteredMap->GetTex2D().Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmdList->ResourceBarrier(1, &barrier);

    const uint32 baseSize = _prefilteredMap->GetWidth();
    const uint32 mipLevels = _prefilteredMap->GetMipLevels();
    const float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };

    for (uint32 mip = 0; mip < mipLevels; ++mip)
    {
        const uint32 mipSize = max(1u, baseSize >> mip);
        const float roughness = static_cast<float>(mip) / static_cast<float>(max(mipLevels - 1, 1u));

        D3D12_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(mipSize);
        viewport.Height = static_cast<float>(mipSize);
        viewport.MinDepth = 0.f;
        viewport.MaxDepth = 1.f;

        D3D12_RECT scissor = {};
        scissor.right = static_cast<LONG>(mipSize);
        scissor.bottom = static_cast<LONG>(mipSize);

        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissor);

        for (uint32 face = 0; face < 6; ++face)
        {
            uint32 directionFace = face;

            if (face == 2)
                directionFace = 3;
            else if (face == 3)
                directionFace = 2;

            IBLCubemapParams cubeParams = {};
            cubeParams.cubeFaceIndex = directionFace;
            cubeParams.roughness = roughness;

            GDEngine->GetConstantBuffer(CONSTANT_BUFFER_TYPE::IBL_CUBEMAP)->PushGraphicsData(&cubeParams, sizeof(cubeParams));

            D3D12_CPU_DESCRIPTOR_HANDLE rtv = _prefilteredMap->GetRTVHandle(face, mip);

            cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
            cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

            material->PushGraphicsData();

            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cmdList->DrawInstanced(3, 1, 0, 0);
        }
    }

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        _prefilteredMap->GetTex2D().Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmdList->ResourceBarrier(1, &barrier);
}

void IBLManager::CreateBRDFLUT()
{
    _brdfLUT = GET_SINGLE(Resources)->Get<Texture>(L"BRDFLUT");
    shared_ptr<Material> material = GET_SINGLE(Resources)->Get<Material>(L"IBL_BRDFLUT");

    assert(_brdfLUT != nullptr);
    assert(_brdfLUT->GetTex2D() != nullptr);
    assert(material != nullptr);

    ComPtr<ID3D12GraphicsCommandList> cmdList = GRAPHICS_CMD_LIST;

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        _brdfLUT->GetTex2D().Get(),
        D3D12_RESOURCE_STATE_COMMON,
        D3D12_RESOURCE_STATE_RENDER_TARGET);

    cmdList->ResourceBarrier(1, &barrier);

    constexpr uint32 lutSize = 512;

    D3D12_VIEWPORT viewport = {};
    viewport.Width = static_cast<float>(lutSize);
    viewport.Height = static_cast<float>(lutSize);
    viewport.MinDepth = 0.f;
    viewport.MaxDepth = 1.f;

    D3D12_RECT scissor = {};
    scissor.right = lutSize;
    scissor.bottom = lutSize;

    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    D3D12_CPU_DESCRIPTOR_HANDLE rtv = _brdfLUT->GetRTVHandle();

    const float clearColor[4] = { 0.f, 0.f, 0.f, 1.f };

    cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
    cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);

    material->PushGraphicsData();

    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);

    barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        _brdfLUT->GetTex2D().Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    cmdList->ResourceBarrier(1, &barrier);
}

