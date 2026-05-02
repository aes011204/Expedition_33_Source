#include "Material.h"
#include "Shader.h"

Material::Material(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : _device { device }
    , _context { context }
{
}

HRESULT Material::Initialize(const aiMaterial* aiMaterial, const char* modelFilePath)
{
    char               drive[MAX_PATH] = {};
    char               dir[MAX_PATH] = {};

    _splitpath_s(modelFilePath, drive, MAX_PATH, dir, MAX_PATH, nullptr, 0, nullptr, 0);


    for (size_t i = 0; i < AI_TEXTURE_TYPE_MAX; i++)
    {
        uint32       numTextures = { aiMaterial->GetTextureCount(static_cast<aiTextureType>(i)) };

        if (0 == numTextures)
            continue;

        _materials[i].reserve(numTextures);
        
        for (uint32 j = 0; j < numTextures; j++)
        {
            aiString          texturePath = {};           


            if (FAILED(aiMaterial->GetTexture(static_cast<aiTextureType>(i), j, &texturePath)))
                continue;

            char               fileName[MAX_PATH] = {};
            char               ext[MAX_PATH] = {};

            _splitpath_s(texturePath.C_Str(), nullptr, 0, nullptr, 0, fileName, MAX_PATH, ext, MAX_PATH);


            char               fullPath[MAX_PATH] = {};

            strcpy_s(fullPath, drive);
            strcat_s(fullPath, dir);
            strcat_s(fullPath, fileName);
            strcat_s(fullPath, ext);


            wchar_t              textureFilePath[MAX_PATH] = {};
            MultiByteToWideChar(CP_ACP, 0, fullPath, -1, textureFilePath, MAX_PATH);

            HRESULT         hr = {};
            ComPtr<ID3D11ShaderResourceView> srv = nullptr;

            if (false == strcmp(ext, (".dds")))
                hr = CreateDDSTextureFromFile(_device.Get(), textureFilePath, nullptr, srv.GetAddressOf());

            else if (false == strcmp(ext, (".tga")))
                hr = E_FAIL;

            else
                hr = CreateWICTextureFromFile(_device.Get(), textureFilePath, nullptr, srv.GetAddressOf());

            if (FAILED(hr))
                return E_FAIL;

            _materials[i].emplace_back(std::move(srv));
        }

     
    }

    return S_OK;
}

HRESULT Material::Bind_Material(Shader* shader, const char* constantName, aiTextureType materialType, uint32 textureIndex)
{
    if (textureIndex >= _materials[materialType].size() ||
        nullptr == _materials[materialType][textureIndex])
        return E_FAIL;

    shader->Bind_SRV(constantName, _materials[materialType][textureIndex].Get());

    return S_OK;
}

Shared<Material> Material::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const aiMaterial* aiMaterial, const char* modelFilePath)
{
    auto instance = make_shared<Material>(device, context);

    if (FAILED(instance->Initialize(aiMaterial, modelFilePath)))
    {
        MSG_BOX("Failed to Created : Material");
        return nullptr;
	}
    return instance;
}


void Material::Free()
{
    __super::Free();

    for (size_t i = 0; i < AI_TEXTURE_TYPE_MAX; i++)
    {
        _materials[i].clear();
    }

}



