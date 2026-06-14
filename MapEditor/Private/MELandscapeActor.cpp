#include "MELandscapeActor.h"
#include "GameInstance.h"

namespace
{
    fs::path ResolveLandscapeTextureLoadPath(const fs::path& texturePath)
    {
        fs::path normalizedPath = texturePath.lexically_normal();
        const fs::path ext = normalizedPath.extension();

        if (ext == ".dds" || ext == ".DDS")
            return normalizedPath;

        fs::path ddsPath = normalizedPath;
        ddsPath.replace_extension(".dds");
        if (fs::exists(ddsPath) && fs::is_regular_file(ddsPath))
            return ddsPath.lexically_normal();

        fs::path upperDdsPath = normalizedPath;
        upperDdsPath.replace_extension(".DDS");
        if (fs::exists(upperDdsPath) && fs::is_regular_file(upperDdsPath))
            return upperDdsPath.lexically_normal();

        return normalizedPath;
    }
}

MELandscapeActor::MELandscapeActor(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

HRESULT MELandscapeActor::Initialize(const VIBuffer_Landscape::LAND_DESC& desc)
{
    if (FAILED(__super::Initialize(nullptr)))
    {
        return E_FAIL;
    }

    if (FAILED(Add_Component(0, L"Shader_VtxNorTex", _shader)))
    {
        return E_FAIL;
    }

    _vibuffer = VIBuffer_Landscape::Create(_device, _context, desc);
    if (!_vibuffer)
    {
        return E_FAIL;
    }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height = 1;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    float zero = 0.f;
    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = &zero;
    initData.SysMemPitch = sizeof(float);

    if (FAILED(_device->CreateTexture2D(&td, &initData, _dummyWeightTex.GetAddressOf())))
    {
        return E_FAIL;
    }
    if (FAILED(_device->CreateShaderResourceView(_dummyWeightTex.Get(), nullptr, _dummyWeightSRV.GetAddressOf())))
    {
        return E_FAIL;
    }

    D3D11_TEXTURE2D_DESC ndesc = {};
    ndesc.Width = ndesc.Height = 1;
    ndesc.MipLevels = ndesc.ArraySize = 1;
    ndesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ndesc.SampleDesc.Count = 1;
    ndesc.Usage = D3D11_USAGE_IMMUTABLE;
    ndesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    uint8_t flatNormal[4] = { 128, 128, 255, 255 };
    D3D11_SUBRESOURCE_DATA ndata{};
    ndata.pSysMem = flatNormal;
    ndata.SysMemPitch = 4;

    if (FAILED(_device->CreateTexture2D(&ndesc, &ndata, _dummyNormalTex.GetAddressOf())))
    {
        return E_FAIL;
    }

    if (FAILED(_device->CreateShaderResourceView(_dummyNormalTex.Get(), nullptr, _dummyNormalSRV.GetAddressOf())))
    {
        return E_FAIL;
    }

    uint8_t flatRough[4] = { 128, 128, 128, 255 };
    D3D11_SUBRESOURCE_DATA rdata = {};
    rdata.pSysMem = flatRough;
    rdata.SysMemPitch = 4;

    if (FAILED(_device->CreateTexture2D(&ndesc, &rdata, _dummyRoughnessTex.GetAddressOf())))
    {
        return E_FAIL;
    }
    if (FAILED(_device->CreateShaderResourceView(_dummyRoughnessTex.Get(), nullptr, _dummyRoughnessSRV.GetAddressOf())))
    {
        return E_FAIL;
    }

    return S_OK;
}

void MELandscapeActor::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MELandscapeActor::Render()
{
    if (FAILED(Bind_ShaderResources()))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Begin(_layerCount > 0 ? 1 : 0)))
    {
        return E_FAIL;
    }

    if (FAILED(_vibuffer->Bind_Resources()))
    {
        return E_FAIL;
    }

    //Vec3 localCamPos = _cameraPos - _transformCom->Get_Position();
    //localCamPos.y = 0.f;
    //if (FAILED(_vibuffer->Render_LOD(localCamPos, _lodDist)))
    //{
    //    return E_FAIL;
    //}

    if (FAILED(_vibuffer->Render_NoLOD()))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MELandscapeActor::Bind_ShaderResources()
{
    if (FAILED(_transformCom->Bind_ShaderResource(_shader.get(), "g_WorldMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::View, _shader.get(), "g_ViewMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(GAME->Bind_TransformMatrix(D3DTS::Proj, _shader.get(), "g_ProjMatrix")))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_RawValue("g_Far", &_cameraFar, sizeof(float))))
    {
        return E_FAIL;
    }


    for (uint32 i = 0; i < _layerCount; ++i)
    {
        if (FAILED(_shader->Bind_SRV(layerNames[i], _layerSRVs[i].Get())))
        {
            return E_FAIL;
        }
        if (FAILED(_shader->Bind_SRV(weightNames[i], _weightSRVs[i].Get())))
        {
            return E_FAIL;
        }
    }

    for (uint32 i = _layerCount; i < MAX_LAYERS; ++i)
    {
        _shader->Bind_SRV(layerNames[i], _layerSRVs[0].Get());
        _shader->Bind_SRV(weightNames[i], _dummyWeightSRV.Get());
    }

    // Normal / Roughness
    for (uint32 i = 0; i < _layerCount; ++i)
    {
        _shader->Bind_SRV(normalNames[i], _normalSRVs[i] ? _normalSRVs[i].Get() : _dummyNormalSRV.Get());
        _shader->Bind_SRV(roughnessNames[i], _roughnessSRVs[i] ? _roughnessSRVs[i].Get() : _dummyRoughnessSRV.Get());
    }
    for (uint32 i = _layerCount; i < MAX_LAYERS; ++i)
    {
        _shader->Bind_SRV(normalNames[i], _dummyNormalSRV.Get());
        _shader->Bind_SRV(roughnessNames[i], _dummyRoughnessSRV.Get());
    }

    // LayerParams cbuffer
    Vec4 tilingArr[MAX_LAYERS];
    Vec4 tintArr[MAX_LAYERS];
    Vec4 controlArr[MAX_LAYERS];
    for (uint32 i = 0; i < MAX_LAYERS; ++i)
    {
        tilingArr[i] = Vec4(_tiling[i].x, _tiling[i].y, 0.f, 0.f);
        tintArr[i] = _groundTint[i];
        controlArr[i] = _layerControl[i];
    }

    if (FAILED(_shader->Bind_RawValue("g_Tiling", tilingArr, sizeof(tilingArr))))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_RawValue("g_GroundTint", tintArr, sizeof(tintArr))))
    {
        return E_FAIL;
    }

    if (FAILED(_shader->Bind_RawValue("g_LayerControl", controlArr, sizeof(controlArr))))
    {
        return E_FAIL;
    }

    Vec4 albedoArr[MAX_LAYERS];
    for (uint32 i = 0; i < MAX_LAYERS; ++i)
    {
        albedoArr[i] = _albedoControl[i];
    }

    if (FAILED(_shader->Bind_RawValue("g_AlbedoControl", albedoArr, sizeof(albedoArr))))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MELandscapeActor::Add_LayerTexture(const wstring& albedoPath, const wstring& normalPath, const wstring& roughnessPath)
{
    if (_layerCount >= MAX_LAYERS)
    {
        return E_FAIL;
    }

    const fs::path resolvedAlbedoPath = ResolveLandscapeTextureLoadPath(fs::path(albedoPath));

    WCHAR ext[MAX_PATH] = {};
    _wsplitpath_s(resolvedAlbedoPath.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, ext, MAX_PATH);

    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = (lstrcmp(ext, L".dds") == 0)
        ? CreateDDSTextureFromFile(_device.Get(), resolvedAlbedoPath.c_str(), nullptr, srv.GetAddressOf())
        : CreateWICTextureFromFile(_device.Get(), resolvedAlbedoPath.c_str(), nullptr, srv.GetAddressOf());

    if (FAILED(hr))
    {
        return E_FAIL;
    }

    _layerSRVs[_layerCount] = srv;

    if (!normalPath.empty())
    {
        const fs::path resolvedNormalPath = ResolveLandscapeTextureLoadPath(fs::path(normalPath));
        WCHAR nExt[MAX_PATH] = {};
        _wsplitpath_s(resolvedNormalPath.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, nExt, MAX_PATH);
        ComPtr<ID3D11ShaderResourceView> nsrv;
        hr = (lstrcmp(nExt, L".dds") == 0)
            ? CreateDDSTextureFromFile(_device.Get(), resolvedNormalPath.c_str(), nullptr, nsrv.GetAddressOf())
            : CreateWICTextureFromFile(_device.Get(), resolvedNormalPath.c_str(), nullptr, nsrv.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            _normalSRVs[_layerCount] = nsrv;
        }
        _normalPaths[_layerCount] = resolvedNormalPath.filename().wstring();
    }

    if (!roughnessPath.empty())
    {
        const fs::path resolvedRoughnessPath = ResolveLandscapeTextureLoadPath(fs::path(roughnessPath));
        WCHAR rExt[MAX_PATH] = {};
        _wsplitpath_s(resolvedRoughnessPath.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, rExt, MAX_PATH);
        ComPtr<ID3D11ShaderResourceView> rsrv;
        hr = (lstrcmp(rExt, L".dds") == 0)
            ? CreateDDSTextureFromFile(_device.Get(), resolvedRoughnessPath.c_str(), nullptr, rsrv.GetAddressOf())
            : CreateWICTextureFromFile(_device.Get(), resolvedRoughnessPath.c_str(), nullptr, rsrv.GetAddressOf());

        if (SUCCEEDED(hr))
        {
            _roughnessSRVs[_layerCount] = rsrv;
        }
        _roughnessPaths[_layerCount] = resolvedRoughnessPath.filename().wstring();
    }

    uint32 w = _vibuffer->Get_CountX() + 1;
    uint32 h = _vibuffer->Get_CountZ() + 1;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w;
    td.Height = h;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(_device->CreateTexture2D(&td, nullptr, _weightTextures[_layerCount].GetAddressOf())))
    {
        return E_FAIL;
    }

    if (FAILED(_device->CreateShaderResourceView(_weightTextures[_layerCount].Get(), nullptr, _weightSRVs[_layerCount].GetAddressOf())))
    {
        return E_FAIL;
    }

    _vibuffer->Set_LayerCount(_layerCount + 1);
    _layerPaths[_layerCount] = resolvedAlbedoPath.filename().wstring();
    ++_layerCount;

    Flush_WeightMap(_layerCount - 1);
    return S_OK;
}

void MELandscapeActor::Flush_WeightMap(uint32 layerIdx)
{
    if (layerIdx >= _layerCount || !_weightTextures[layerIdx])
    {
        return;
    }

    const auto& weights = _vibuffer->Get_WeightMap(layerIdx);
    uint32 w = _vibuffer->Get_CountX() + 1;
    uint32 h = _vibuffer->Get_CountZ() + 1;

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    if (FAILED(_context->Map(_weightTextures[layerIdx].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        return;
    }

    float* dst = static_cast<float*>(mapped.pData);
    uint32 rowPitch = mapped.RowPitch / sizeof(float);
    for (uint32 z = 0; z < h; ++z)
    {
        for (uint32 x = 0; x < w; ++x)
        {
            dst[z * rowPitch + x] = weights[z * w + x];
        }
    }

    _context->Unmap(_weightTextures[layerIdx].Get(), 0);
}

void MELandscapeActor::Clear_Layers()
{
    for (uint32 i = 0; i < MAX_LAYERS; ++i)
    {
        _layerSRVs[i].Reset();
        _weightTextures[i].Reset();
        _weightSRVs[i].Reset();
        _layerPaths[i].clear();

        _normalSRVs[i].Reset();
        _roughnessSRVs[i].Reset();
        _normalPaths[i].clear();
        _roughnessPaths[i].clear();
    }
    _layerCount = 0;
    _vibuffer->Clear_Layers();
}

void MELandscapeActor::Remove_LayerTexture(uint32 idx)
{
    if (idx >= _layerCount || _layerCount <= 1)
    {
        return;
    }

    for (uint32 i = idx; i < _layerCount - 1; ++i)
    {
        _layerSRVs[i] = _layerSRVs[i + 1];
        _weightTextures[i] = _weightTextures[i + 1];
        _weightSRVs[i] = _weightSRVs[i + 1];
        _layerPaths[i] = _layerPaths[i + 1];
        _tiling[i] = _tiling[i + 1];
        _groundTint[i] = _groundTint[i + 1];
        _layerControl[i] = _layerControl[i + 1];
        _albedoControl[i] = _albedoControl[i + 1];

        _normalSRVs[i] = _normalSRVs[i + 1];
        _roughnessSRVs[i] = _roughnessSRVs[i + 1];
        _normalPaths[i] = _normalPaths[i + 1];
        _roughnessPaths[i] = _roughnessPaths[i + 1];
    }

    uint32 last = _layerCount - 1;
    _layerSRVs[last].Reset();
    _weightTextures[last].Reset();
    _weightSRVs[last].Reset();
    _layerPaths[last].clear();
    _tiling[last] = Vec2(1.f, 1.f);
    _groundTint[last] = Vec4(1.f, 1.f, 1.f, 1.f);
    _layerControl[last] = Vec4(1.f, 1.f, 0.f, 0.f);
    _albedoControl[last] = Vec4(1.f, 1.f, 1.f, 0.f);
    _normalSRVs[last].Reset();
    _roughnessSRVs[last].Reset();
    _normalPaths[last].clear();
    _roughnessPaths[last].clear();

    --_layerCount;

    _vibuffer->Remove_Layer(idx);

    Flush_WeightMap(0);
}

void MELandscapeActor::Set_BaseLayer(uint32 idx)
{
    if (idx == 0 || idx >= _layerCount)
        return;

    swap(_layerSRVs[0], _layerSRVs[idx]);
    swap(_layerPaths[0], _layerPaths[idx]);
    swap(_weightTextures[0], _weightTextures[idx]);
    swap(_weightSRVs[0], _weightSRVs[idx]);
    swap(_tiling[0], _tiling[idx]);
    swap(_groundTint[0], _groundTint[idx]);
    swap(_layerControl[0], _layerControl[idx]);
    swap(_albedoControl[0], _albedoControl[idx]);
    swap(_normalSRVs[0], _normalSRVs[idx]);
    swap(_roughnessSRVs[0], _roughnessSRVs[idx]);
    swap(_normalPaths[0], _normalPaths[idx]);
    swap(_roughnessPaths[0], _roughnessPaths[idx]);

    _vibuffer->Swap_Layer(0, idx);
}

Shared<MELandscapeActor> MELandscapeActor::Create(const ComPtr<Device>& device, const ComPtr<Context>& context, const VIBuffer_Landscape::LAND_DESC& desc)
{
    auto instance = make_shared<MELandscapeActor>(device, context);
    if (FAILED(instance->Initialize(desc)))
    {
        return nullptr;
    }

    return instance;
}

void MELandscapeActor::Free()
{
    GameObject::Free();
}
