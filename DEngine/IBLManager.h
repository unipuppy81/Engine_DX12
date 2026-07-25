#pragma once

class Texture;
class Material;

class IBLManager
{	
	DECLARE_SINGLE(IBLManager);

public:
	void Init();

private:
	void CreateEnvironmentCube();
	void CreateIrradianceMap();
	void CreatePrefilteredMap();
	void CreateBRDFLUT();


private:
	shared_ptr<Texture> _environmentMap;
	shared_ptr<Texture> _irradianceMap;
	shared_ptr<Texture> _prefilteredMap;
	shared_ptr<Texture> _brdfLUT;
};

