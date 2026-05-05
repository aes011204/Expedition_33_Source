#pragma once

#include "Base.h"
#include <filesystem>

NS_BEGIN(Engine)

class Material final : public Base
{
public:
	struct TextureSlot
	{
		uint32 typeIndex{};
		uint32 textureIndex{};
		ComPtr<ID3D11ShaderResourceView> shaderResourceView{ nullptr };
	};

public:
	Material(const ComPtr<Device>& device, const ComPtr<Context>& context);
	~Material() override = default;

public:
	HRESULT Initialize(const char* modelPath, const ModelMaterialData& materialData, const vector<ModelTextureSlot>& slots, uint32 maxTextureTypeSize);
	HRESULT Bind_Material(class ShaderCom* shader, const char* constantName, uint32 materialType, uint32 textureIndex);

private:
	std::wstring Find_TextureFile(const std::filesystem::path& searchRoot, const std::string& fileName);

private:
	uint32							_maxTextureTypeSize{};
	ComPtr<Device>					_device = { nullptr };
	ComPtr<Context>					_context = { nullptr };
	vector<vector<TextureSlot>>		_materials;

public:
	static Shared<Material> Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const char* modelPath, const ModelMaterialData& materialData, const vector<ModelTextureSlot>& slots, uint32 maxTextureTypeSize);
	void Free() override;
};

NS_END
