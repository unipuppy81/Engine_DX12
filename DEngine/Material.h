#pragma once
#include "Object.h"

class Shader;
class Texture;

enum
{
	MATERIAL_ARG_COUNT = 4,
};

struct MaterialParams
{
	void SetInt(uint8 index, int32 value) { intParams[index] = value; }
	void SetFloat(uint8 index, float value) { floatParams[index] = value; }
	void SetTexOn(uint8 index, int32 value) { texOnParams[index] = value; }
	void SetVec2(uint8 index, Vec2 value) { vec2Params[index] = value; }
	void SetVec4(uint8 index, Vec4 value) { vec4Params[index] = value; }
	void SetMatrix(uint8 index, Matrix& value) { matrixParams[index] = value; }

	array<int32, MATERIAL_ARG_COUNT> intParams;
	array<float, MATERIAL_ARG_COUNT> floatParams;
	array<int32, MATERIAL_ARG_COUNT> texOnParams;
	array<Vec2, MATERIAL_ARG_COUNT> vec2Params;
	array<Vec4, MATERIAL_ARG_COUNT> vec4Params;
	array<Matrix, MATERIAL_ARG_COUNT> matrixParams;
};

struct PBRMaterialParams
{
	Vec4 baseColor = Vec4(1.f, 1.f, 1.f, 1.f);

	float metallic = 0.f;
	float roughness = 0.5f;
	float ao = 1.f;
	float padding = 0.f;
};

class Material : public Object
{
public:
	Material();
	virtual ~Material();

	shared_ptr<Shader> GetShader() { return _shader; }

	void SetShader(shared_ptr<Shader> shader) { _shader = shader; }
	void SetInt(uint8 index, int32 value) { _params.SetInt(index, value); }
	void SetFloat(uint8 index, float value) { _params.SetFloat(index, value); }
	void SetTexture(uint8 index, shared_ptr<Texture> texture)
	{
		_textures[index] = texture;
		_params.SetTexOn(index, texture == nullptr ? 0 : 1);
	}

	void SetVec2(uint8 index, Vec2 value) { _params.SetVec2(index, value); }
	void SetVec4(uint8 index, Vec4 value) { _params.SetVec4(index, value); }
	void SetMatrix(uint8 index, Matrix& value) { _params.SetMatrix(index, value); }

	// PBR
	void SetPBRBaseColor(const Vec4& color) { _pbrParams.baseColor = color; }
	void SetPBRMetallic(float value) { _pbrParams.metallic = value; }
	void SetPBRRoughness(float value) { _pbrParams.roughness = value; }
	void SetPBRAO(float value) { _pbrParams.ao = value; }

	void PushGraphicsData();
	void PushComputeData();
	void Dispatch(uint32 x, uint32 y, uint32 z);

	shared_ptr<Material> Clone();

private:
	shared_ptr<Shader>	_shader;
	MaterialParams		_params;
	PBRMaterialParams	_pbrParams;
	CubeCaptureParams	_cubeCapParams;

	array<shared_ptr<Texture>, MATERIAL_ARG_COUNT> _textures;

};

