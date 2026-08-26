#include "pch.h"
#include "Material.h"
#include "DEngine.h"

Material::Material() : Object(OBJECT_TYPE::MATERIAL)
{

}

Material::~Material()
{

}

void Material::PushGraphicsData()
{
	DX_LOG(L"CMDLISTTEST MATERIAL CMD = " << GRAPHICS_CMD_LIST);
	// CBV 업로드
	// 기존 MaterialParams -> b2
	CONST_BUFFER(CONSTANT_BUFFER_TYPE::MATERIAL)->PushGraphicsData(&_params, sizeof(_params));
	// PBRMaterialParams -> b3
	CONST_BUFFER(CONSTANT_BUFFER_TYPE::MATERIAL_PBR)->PushGraphicsData(&_pbrParams, sizeof(_pbrParams));

	// SRV 업로드
	for (size_t i = 0; i < _textures.size(); i++)
	{
		if (_textures[i] == nullptr)
			continue;

		SRV_REGISTER reg = SRV_REGISTER(static_cast<int8>(SRV_REGISTER::t0) + i);
		GDEngine->GetGraphicsDescHeap()->SetSRV(_textures[i]->GetSRVHandle(), reg);
	}

	// 파이프라인 세팅
	_shader->Update();
}

void Material::PushComputeData()
{
	// CBV 업로드
	CONST_BUFFER(CONSTANT_BUFFER_TYPE::MATERIAL)->PushComputeData(&_params, sizeof(_params));

	// SRV 업로드
	for (size_t i = 0; i < _textures.size(); i++)
	{
		if (_textures[i] == nullptr)
			continue;

		SRV_REGISTER reg = SRV_REGISTER(static_cast<int8>(SRV_REGISTER::t0) + i);
		GDEngine->GetComputeDescHeap()->SetSRV(_textures[i]->GetSRVHandle(), reg);
	}

	// 파이프라인 세팅
	_shader->Update();
}

void Material::Dispatch(uint32 x, uint32 y, uint32 z)
{
	auto cmdList = COMPUTE_CMD_LIST.Get();

	// CBV + SRV + SetPipelineState
	PushComputeData();

	// SetDescriptorHeaps + SetComputeRootDescriptorTable
	GDEngine->GetComputeDescHeap()->CommitTable();

	cmdList->Dispatch(x, y, z);

	GDEngine->GetComputeCmdQueue()->FlushComputeCommandQueue();
}


shared_ptr<Material> Material::Clone()
{
	shared_ptr<Material> material = make_shared<Material>();

	material->SetShader(_shader);
	material->_params = _params;
	material->_pbrParams = _pbrParams;
	material->_textures = _textures;

	return material;
}