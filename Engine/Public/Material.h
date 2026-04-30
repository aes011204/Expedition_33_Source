#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Material final : public Base
{
public:
	Material(ComPtr<Device> device, ComPtr<Context> context);
	virtual ~Material() { Free(); }

public:
	HRESULT Initialize(const aiMaterial* pAIMaterial, const char* pModelFilePath);
	HRESULT Bind_Material(class Shader* pShader, const char* pConstantName, aiTextureType eMaterialType, uint32 iNumTextures);

private:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };


	vector<ID3D11ShaderResourceView*>			_materials[AI_TEXTURE_TYPE_MAX];


public:
	static Shared<Material> Create(ComPtr<Device> device, ComPtr<Context> context, const aiMaterial* pAIMaterial, const char* pModelFilePath);
	virtual void Free() override;
};

NS_END

