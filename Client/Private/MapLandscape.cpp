#include "MapLandscape.h"
#include "GameInstance.h"
#include "StaticCollisionData.h"
#include "Collision_Define.h"

IMPLEMENT_REFLECTION(MapLandscape)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "MapLandscape";
    info.category = "World";
    return true;
}

MapLandscape::MapLandscape(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

MapLandscape::MapLandscape(const MapLandscape& prototype)
    : GameObject{ prototype }
{
}

HRESULT MapLandscape::Initialize_Prototype()
{
    return S_OK;
}

HRESULT MapLandscape::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
    {
        return E_FAIL;
    }

    if (FAILED(Add_Component(0, L"Shader_VtxNorTex", _shader)))
    {
        return E_FAIL;
    }

    const LANDSCAPE_DESC* desc = static_cast<const LANDSCAPE_DESC*>(arg);
    if (!desc)
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

    D3D11_TEXTURE2D_DESC ndesc{};
    ndesc.Width = ndesc.Height = 1;
    ndesc.MipLevels = ndesc.ArraySize = 1;
    ndesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    ndesc.SampleDesc.Count = 1;
    ndesc.Usage = D3D11_USAGE_IMMUTABLE;
    ndesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    uint8_t flatNormal[4] = { 128, 128, 255, 255 };
    D3D11_SUBRESOURCE_DATA ndata = {};
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

    _collisionCacheKey = desc->hmapPath;

    if (FAILED(Load_HMap(desc->hmapPath, desc->texRoot)))
    {
        return E_FAIL;
    }

    if (FAILED(Build_CollisionData()))
    {
        return E_FAIL;
    }

    uint8_t flatRough[4] = { 255, 255, 255, 255 };
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

    if (_collisionData != nullptr)
    {
        _physxStaticHandle = GAME->Register_PhysXStaticCollision(
            *_collisionData,
            _transformCom->Get_WorldMatrix(),
            Collision_Preset::Enviroment);
    }

    return S_OK;
}

void MapLandscape::Late_Update(float timeDelta)
{
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT MapLandscape::Render()
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

    const Vec4* camPos4 = GAME->Get_CamPosition();
    Vec3 camPos = Vec3(camPos4->x, camPos4->y, camPos4->z);
    Vec3 localCam = camPos - _transformCom->Get_Position();

    if (FAILED(_vibuffer->Render_LOD(localCam, _lodDist)))
    {
        return E_FAIL;
    }

    return S_OK;
}

HRESULT MapLandscape::Load_HMap(const wstring& hmapPath, const wstring& texRoot)
{
    FILE* f = {};
    if (_wfopen_s(&f, hmapPath.c_str(), L"rb") != 0 || !f)
    {
        return E_FAIL;
    }

    uint32 countX = {}, countZ = {};
    float sizeX = {}, sizeZ = {};
    fread(&countX, sizeof(uint32), 1, f);
    fread(&countZ, sizeof(uint32), 1, f);
    fread(&sizeX, sizeof(float), 1, f);
    fread(&sizeZ, sizeof(float), 1, f);

    Vec3 pos{};
    fread(&pos, sizeof(Vec3), 1, f);
    _transformCom->Set_Position(pos);

    VIBuffer_Landscape::LAND_DESC landDesc{};
    landDesc.countX = countX;
    landDesc.countZ = countZ;
    landDesc.sizeX = sizeX;
    landDesc.sizeZ = sizeZ;

    _vibuffer = VIBuffer_Landscape::Create(_device, _context, landDesc);
    if (!_vibuffer)
    {
        fclose(f);
        return E_FAIL;
    }

    const uint32 vertCount = (countX + 1) * (countZ + 1);
    auto& heights = _vibuffer->Get_Heights();
    heights.resize(vertCount);
    fread(heights.data(), sizeof(float), vertCount, f);
    _vibuffer->Flush();

    uint32 layerCount = 0;
    if (fread(&layerCount, sizeof(uint32), 1, f) == 1 && layerCount > 0)
    {
        _layerCount = layerCount;
        _vibuffer->Set_LayerCount(layerCount);

        for (uint32 i = 0; i < layerCount; ++i)
        {
            uint32 nameLen = 0;
            fread(&nameLen, sizeof(uint32), 1, f);
            wstring name(nameLen, L'\0');
            fread(name.data(), sizeof(WCHAR), nameLen, f);

            uint32 normalLen = 0;
            fread(&normalLen, sizeof(uint32), 1, f);
            wstring normalName(normalLen, L'\0');
            if (normalLen > 0)
            {
                fread(normalName.data(), sizeof(WCHAR), normalLen, f);
            }

            uint32 roughLen = 0;
            fread(&roughLen, sizeof(uint32), 1, f);
            wstring roughName(roughLen, L'\0');
            if (roughLen > 0)
            {
                fread(roughName.data(), sizeof(WCHAR), roughLen, f);
            }

            float tiling = 3.f;
            Vec4 tint = { 1.f, 1.f, 1.f, 1.f };
            Vec4 control = { 1, 1, 0, 0 };
            fread(&tiling, sizeof(float), 1, f);
            fread(&tint, sizeof(Vec4), 1, f);
            fread(&control, sizeof(Vec4), 1, f);
            Vec4 albedo = { 1.f, 1.f, 1.f, 0.f };
            fread(&albedo, sizeof(Vec4), 1, f);

            auto& weights = _vibuffer->Get_WeightMap(i);
            fread(weights.data(), sizeof(float), vertCount, f);

            wstring albedoPath = (fs::path(texRoot) / name).wstring();
            wstring normalPath = (normalLen > 0) ? (fs::path(texRoot) / normalName).wstring() : L"";
            wstring roughPath = (roughLen > 0) ? (fs::path(texRoot) / roughName).wstring() : L"";

            if (FAILED(Add_LayerTexture(albedoPath, i, normalPath, roughPath)))
            {
                fclose(f);
                return E_FAIL;
            }

            _tiling[i] = tiling;
            _groundTint[i] = tint;
            _layerControl[i] = control;
            _albedoControl[i] = albedo;

            Flush_WeightMap(i);
        }
    }

    fclose(f);
    return S_OK;
}

HRESULT MapLandscape::Build_CollisionData()
{
    if (_vibuffer == nullptr)
    {
        return E_FAIL;
    }

    const uint32 countX = _vibuffer->Get_CountX();
    const uint32 countZ = _vibuffer->Get_CountZ();
    const uint32 numVX = countX + 1;
    const uint32 numVZ = countZ + 1;

    const vector<float>& heights = _vibuffer->Get_Heights();
    if (heights.size() != static_cast<size_t>(numVX * numVZ))
    {
        return E_FAIL;
    }

    auto collisionData = make_shared<StaticCollisionData>();
    collisionData->cacheKey = _collisionCacheKey;
    collisionData->preLocalTransform = Matrix::Identity;

    ModelCollisionMesh mesh{};
    mesh.positions.reserve(static_cast<size_t>(numVX * numVZ));
    mesh.indices.reserve(static_cast<size_t>(countX * countZ * 6));

    for (uint32 z = 0; z < numVZ; ++z)
    {
        for (uint32 x = 0; x < numVX; ++x)
        {
            const uint32 index = z * numVX + x;
            mesh.positions.push_back(Vec3(
                static_cast<float>(x) * _vibuffer->Get_SizeX(),
                heights[index],
                static_cast<float>(z) * _vibuffer->Get_SizeZ()));
        }
    }

    for (uint32 z = 0; z < countZ; ++z)
    {
        for (uint32 x = 0; x < countX; ++x)
        {
            const uint32 tl = z * numVX + x;
            const uint32 tr = tl + 1;
            const uint32 bl = (z + 1) * numVX + x;
            const uint32 br = bl + 1;

            mesh.indices.push_back(bl);
            mesh.indices.push_back(br);
            mesh.indices.push_back(tr);

            mesh.indices.push_back(bl);
            mesh.indices.push_back(tr);
            mesh.indices.push_back(tl);
        }
    }

    collisionData->triangleMeshes.push_back(std::move(mesh));
    _collisionData = collisionData;

    return S_OK;
}

HRESULT MapLandscape::Add_LayerTexture(const wstring& fullTexPath, uint32 idx, const wstring& normalPath, const wstring& roughnessPath)
{
    WCHAR ext[MAX_PATH] = {};
    _wsplitpath_s(fullTexPath.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, ext, MAX_PATH);

    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = (lstrcmp(ext, L".dds") == 0)
        ? CreateDDSTextureFromFile(_device.Get(), fullTexPath.c_str(), nullptr, srv.GetAddressOf())
        : CreateWICTextureFromFile(_device.Get(), fullTexPath.c_str(), nullptr, srv.GetAddressOf());

    if (FAILED(hr))
    {
        return E_FAIL;
    }

    _layerSRVs[idx] = srv;

    if (!normalPath.empty())
    {
        WCHAR nExt[MAX_PATH] = {};
        _wsplitpath_s(normalPath.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, nExt, MAX_PATH);
        ComPtr<ID3D11ShaderResourceView> nsrv;
        HRESULT hr = (lstrcmp(nExt, L".dds") == 0)
            ? CreateDDSTextureFromFile(_device.Get(), normalPath.c_str(), nullptr, nsrv.GetAddressOf())
            : CreateWICTextureFromFile(_device.Get(), normalPath.c_str(), nullptr, nsrv.GetAddressOf());

        if (SUCCEEDED(hr))
        {
            _normalSRVs[idx] = nsrv;
        }
    }

    if (!roughnessPath.empty())
    {
        WCHAR rExt[MAX_PATH] = {};
        _wsplitpath_s(roughnessPath.c_str(), nullptr, 0, nullptr, 0, nullptr, 0, rExt, MAX_PATH);
        ComPtr<ID3D11ShaderResourceView> rsrv;
        HRESULT hr = (lstrcmp(rExt, L".dds") == 0)
            ? CreateDDSTextureFromFile(_device.Get(), roughnessPath.c_str(), nullptr, rsrv.GetAddressOf())
            : CreateWICTextureFromFile(_device.Get(), roughnessPath.c_str(), nullptr, rsrv.GetAddressOf());
        if (SUCCEEDED(hr))
        {
            _roughnessSRVs[idx] = rsrv;
        }
    }

    uint32 w = _vibuffer->Get_CountX() + 1;
    uint32 h = _vibuffer->Get_CountZ() + 1;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = w;
    td.Height = h;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R32_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (FAILED(_device->CreateTexture2D(&td, nullptr, _weightTextures[idx].GetAddressOf())))
    {
        return E_FAIL;
    }
    if (FAILED(_device->CreateShaderResourceView(_weightTextures[idx].Get(), nullptr, _weightSRVs[idx].GetAddressOf())))
    {
        return E_FAIL;
    }

    return S_OK;
}

void MapLandscape::Flush_WeightMap(uint32 idx)
{
    if (idx >= _layerCount || !_weightTextures[idx])
    {
        return;
    }

    const auto& weights = _vibuffer->Get_WeightMap(idx);
    uint32 w = _vibuffer->Get_CountX() + 1;
    uint32 h = _vibuffer->Get_CountZ() + 1;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(_context->Map(_weightTextures[idx].Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
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
    _context->Unmap(_weightTextures[idx].Get(), 0);
}

HRESULT MapLandscape::Bind_ShaderResources()
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
    if (FAILED(GAME->Bind_CameraFar(_shader, "g_Far")))
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

    Vec4 tilingArr[MAX_LAYERS];
    Vec4 tintArr[MAX_LAYERS];
    Vec4 controlArr[MAX_LAYERS];
    for (uint32 i = 0; i < MAX_LAYERS; ++i)
    {
        tilingArr[i] = Vec4(_tiling[i], 0.f, 0.f, 0.f);
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

Shared<MapLandscape> MapLandscape::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MapLandscape>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
    {
        return nullptr;
    }
    return instance;
}

Shared<GameObject> MapLandscape::Clone(void* pArg)
{
    auto clone = make_shared<MapLandscape>(*this);
    if (FAILED(clone->Initialize(pArg)))
    {
        return nullptr;
    }
    return clone;
}

void MapLandscape::Free()
{
    if (_physxStaticHandle.Is_Valid())
    {
        GAME->Unregister_PhysXStaticActor(_physxStaticHandle);
        _physxStaticHandle = {};
    }

    _collisionData.reset();

    GameObject::Free();
}
