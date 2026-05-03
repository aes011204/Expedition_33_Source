#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class Material final : public Base
{
public:
	Material(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual ~Material() = default;

public:
	HRESULT Initialize(const aiMaterial* aiMaterial, const char* modelFilePath);
	HRESULT Bind_Material(class ShaderCom* shader, const char* constantName, aiTextureType materialType, uint32 numTextures);

private:
	ComPtr<Device> _device = { nullptr };
	ComPtr<Context> _context = { nullptr };


	vector<ComPtr<ID3D11ShaderResourceView>>	_materials[AI_TEXTURE_TYPE_MAX];


public:
	static Shared<Material> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const aiMaterial* aiMaterial, const char* modelFilePath);
	virtual void Free() override;
};

NS_END

