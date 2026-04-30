#include "Material.h"
#include "Shader.h"

Material::Material(ComPtr<Device> device, ComPtr<Context> context)
    : _device { device }
    , _context { context }
{
}

HRESULT Material::Initialize(const aiMaterial* pAIMaterial, const char* pModelFilePath)
{
    char               szDrive[MAX_PATH] = {};
    char               szDir[MAX_PATH] = {};

    _splitpath_s(pModelFilePath, szDrive, MAX_PATH, szDir, MAX_PATH, nullptr, 0, nullptr, 0);


    for (size_t i = 0; i < AI_TEXTURE_TYPE_MAX; i++)
    {
        uint32       iNumTextures = { pAIMaterial->GetTextureCount(static_cast<aiTextureType>(i)) };

        if (0 == iNumTextures)
            continue;

        _materials[i].reserve(iNumTextures);
        
        for (size_t j = 0; j < iNumTextures; j++)
        {
            aiString          strTexturePath = {};           


            if (FAILED(pAIMaterial->GetTexture(static_cast<aiTextureType>(i), j, &strTexturePath)))
                continue;

            char               szFileName[MAX_PATH] = {};
            char               szEXT[MAX_PATH] = {};

            _splitpath_s(strTexturePath.C_Str(), nullptr, 0, nullptr, 0, szFileName, MAX_PATH, szEXT, MAX_PATH);


            char               szFullPath[MAX_PATH] = {};

            strcpy_s(szFullPath, szDrive);
            strcat_s(szFullPath, szDir);
            strcat_s(szFullPath, szFileName);
            strcat_s(szFullPath, szEXT);


            wchar_t              szTextureFilePath[MAX_PATH] = {};
            MultiByteToWideChar(CP_ACP, 0, szFullPath, strlen(szFullPath), szTextureFilePath, MAX_PATH);

            HRESULT         hr = {};
            ID3D11ShaderResourceView* pSRV = { nullptr };

            if (false == strcmp(szEXT, (".dds")))
                hr = CreateDDSTextureFromFile(_device.Get(), szTextureFilePath, nullptr, &pSRV);

            else if (false == strcmp(szEXT, (".tga")))
                hr = E_FAIL;

            else
                hr = CreateWICTextureFromFile(_device.Get(), szTextureFilePath, nullptr, &pSRV);

            if (FAILED(hr))
                return E_FAIL;

            _materials[i].push_back(pSRV);
        }

     
    }

    return S_OK;
}

HRESULT Material::Bind_Material(Shader* pShader, const char* pConstantName, aiTextureType eMaterialType, uint32 iTextureIndex)
{
    if (nullptr == _materials[eMaterialType][iTextureIndex] ||
        iTextureIndex >= _materials[eMaterialType].size())
        return E_FAIL;

    pShader->Bind_SRV(pConstantName, _materials[eMaterialType][iTextureIndex]);

    return S_OK;
}

Shared<Material> Material::Create(ComPtr<Device> device, ComPtr<Context> context, const aiMaterial* pAIMaterial, const char* pModelFilePath)
{
    Shared<Material> pInstance = MakeSharedFromRaw(new Material(device, context));

    if (FAILED(pInstance->Initialize(pAIMaterial, pModelFilePath)))
    {
        MSG_BOX("Failed to Created : Material");
        Safe_Release(pInstance);
    }
    return pInstance;
}


void Material::Free()
{
    __super::Free();

    for (size_t i = 0; i < AI_TEXTURE_TYPE_MAX; i++)
    {
        for (auto& pSRV : _materials[i])
            Safe_Release(pSRV);
        _materials[i].clear();
    }

}



