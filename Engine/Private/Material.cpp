#include "Material.h"
#include "ShaderCom.h"

Material::Material(const ComPtr<Device>& device, const ComPtr<Context>& context)
	: _device{ device }, _context{ context } {}

HRESULT Material::Initialize(const char* modelPath, const ModelMaterialData& materialData, const vector<ModelTextureSlot>& slots, uint32 maxTextureTypeSize)
{
	_maxTextureTypeSize = maxTextureTypeSize;
	_materials.resize(_maxTextureTypeSize);

	fs::path rootPath = fs::path(modelPath).parent_path();

	for (const auto& slot : slots)
	{
		if (slot.typeIndex >= _maxTextureTypeSize)
			continue;

		TextureSlot texSlot{};
		texSlot.typeIndex = slot.typeIndex;
		texSlot.textureIndex = slot.textureIndex;

		string texFileName = slot.path;

		if (!texFileName.empty())
		{
			wstring wFoundPath = Find_TextureFile(rootPath, texFileName);

			if (!wFoundPath.empty())
			{
				ID3D11ShaderResourceView* srv = nullptr;
				fs::path ext = fs::path(wFoundPath).extension();

				HRESULT hr = (ext == ".dds" || ext == ".DDS")
					? DirectX::CreateDDSTextureFromFile(_device.Get(), wFoundPath.c_str(), nullptr, &srv)
					: DirectX::CreateWICTextureFromFile(_device.Get(), wFoundPath.c_str(), nullptr, &srv);

				if (SUCCEEDED(hr))
					texSlot.shaderResourceView.Attach(srv);
			}
		}

		_materials[slot.typeIndex].push_back(texSlot);
	}

	return S_OK;
}

wstring Material::Find_TextureFile(const fs::path& searchRoot, const string& fileName)
{
	/* 1차: 같은 디렉토리에서 직접 확인 */
	fs::path directPath = searchRoot / fileName;
	if (fs::exists(directPath))
		return directPath.lexically_normal().wstring();

	/* 2차: 형제 + 하위 폴더를 재귀 탐색하여 파일명 일치 검색 */
	for (const auto& entry : fs::recursive_directory_iterator(searchRoot))
	{
		if (entry.is_regular_file() && entry.path().filename().string() == fileName)
			return entry.path().lexically_normal().wstring();
	}

	return L"";
}

HRESULT Material::Bind_Material(ShaderCom* shader, const char* constantName, uint32 materialType, uint32 textureIndex)
{
	if (materialType >= _maxTextureTypeSize || textureIndex >= _materials[materialType].size())
		return E_FAIL;

	if (!_materials[materialType][textureIndex].shaderResourceView)
		return E_FAIL;

	return shader->Bind_SRV(constantName, _materials[materialType][textureIndex].shaderResourceView.Get());
}

Shared<Material> Material::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const char* modelPath, const ModelMaterialData& materialData, const vector<ModelTextureSlot>& slots, uint32 maxTextureTypeSize)
{
	auto instance = make_shared<Material>(device, context);

	if (FAILED(instance->Initialize(modelPath, materialData, slots, maxTextureTypeSize)))
	{
		LOG_ERROR("[MODEL] Failed to Created : Material");
		return nullptr;
	}

	return instance;
}

void Material::Free()
{
	__super::Free();

	_materials.clear();
}
