#include "Animation_PreviewObject.h"
#include "Client_Defines.h"
#include "GameInstance.h"
#include "Material_Types.h"
#include "ModelCom.h"
#include "ShaderCom.h"
#include <filesystem>
#include <fstream>

Animation_PreviewObject::Animation_PreviewObject(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject(device, context)
{
}

Animation_PreviewObject::Animation_PreviewObject(const Animation_PreviewObject& prototype)
    : GameObject(prototype)
    , _staticShaderCom(prototype._staticShaderCom)
    , _animShaderCom(prototype._animShaderCom)
    , _modelCom(prototype._modelCom)
{
}

HRESULT Animation_PreviewObject::Initialize_Prototype()
{
    return GameObject::Initialize_Prototype();
}

HRESULT Animation_PreviewObject::Initialize(void* arg)
{
    if (FAILED(GameObject::Initialize(arg)))
        return E_FAIL;

    if (FAILED(Ready_Components()))
        return E_FAIL;

    return S_OK;
}

void Animation_PreviewObject::Update(float timeDelta)
{
    GameObject::Update(timeDelta);

    if (_modelCom && _modelCom->Is_Skeletal())
        _modelCom->Play_Animation(timeDelta);
}

void Animation_PreviewObject::Late_Update(float timeDelta)
{
    GameObject::Late_Update(timeDelta);
    GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());
}

HRESULT Animation_PreviewObject::Render()
{
    if (_modelCom == nullptr || _staticShaderCom == nullptr || _animShaderCom == nullptr)
        return S_OK;

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_modelCom->Get_NumMeshes());

    if (_modelCom->Is_Skeletal())
    {
        for (uint32 i = 0; i < numMeshes; ++i)
        {
            if (FAILED(_modelCom->Bind_Material(_animShaderCom.get(), "g_DiffuseTexture", i, MaterialTextureSlot::BaseColor)))
                continue;

            _modelCom->Bind_Material(_animShaderCom.get(), "g_NormalTexture", i, MaterialTextureSlot::Normal);
            _modelCom->Bind_Material(_animShaderCom.get(), "g_EmissiveTexture", i, MaterialTextureSlot::Emissive);

            const Material::TextureSlot* baseColorSlot =
                _modelCom->Find_MaterialSlot(i, { MaterialTextureSlot::BaseColor });
            const Vec2 uvScale = baseColorSlot ? baseColorSlot->uvScale : Vec2(1.f, 1.f);
            const Vec2 uvOffset = baseColorSlot ? baseColorSlot->uvOffset : Vec2(0.f, 0.f);
            const float uvRotation = baseColorSlot ? baseColorSlot->uvRotation : 0.f;
            const int useOpacityMask = 0;

            _animShaderCom->Bind_RawValue("g_UVScale", &uvScale, sizeof(Vec2));
            _animShaderCom->Bind_RawValue("g_UVOffset", &uvOffset, sizeof(Vec2));
            _animShaderCom->Bind_RawValue("g_UVRotate", &uvRotation, sizeof(float));
            _animShaderCom->Bind_RawValue("g_UseOpacityMask", &useOpacityMask, sizeof(int));

            _modelCom->Bind_BoneMatrices(_animShaderCom.get(), "g_BoneMatrices", i);

            if (FAILED(_animShaderCom->Begin(0)))
                return E_FAIL;

            _modelCom->Render(i);
        }
    }
    else
    {
        for (uint32 i = 0; i < numMeshes; ++i)
        {
            if (FAILED(_modelCom->Bind_Material(_staticShaderCom.get(), "g_DiffuseTexture", i, MaterialTextureSlot::BaseColor)))
                continue;

            if (FAILED(_staticShaderCom->Begin(0)))
                return E_FAIL;

            _modelCom->Render(i);
        }
    }

    return S_OK;
}

bool Animation_PreviewObject::Set_ModelPath(const wstring& modelPath)
{
    _modelCom.reset();
    _components.erase(TEXT("Com_Model"));

    Shared<ModelCom> newModelCom{};
    const wstring prototypeTag = Find_ModelPrototypeTag(modelPath);
    if (!prototypeTag.empty())
    {
        Shared<Base> cloned = GAME->Clone_Prototype(Prototype::Comopnent, ETOI(LevelType::GamePlay), prototypeTag);
        newModelCom = dynamic_pointer_cast<ModelCom>(cloned);
    }

    if (newModelCom == nullptr)
    {
        const string modelPathUtf8 = String::ToString(modelPath);
        newModelCom = ModelCom::Create(_device, _context, ModelType::Anim, modelPathUtf8.c_str());
    }

    if (newModelCom == nullptr)
        return false;

    newModelCom->Set_AnimNotifyExecutionEnabled(false);

    _modelCom = newModelCom;
    _components.emplace(TEXT("Com_Model"), _modelCom);

    if (_transformCom != nullptr)
    {
        _transformCom->Set_Scale(Vec3::One);
        _transformCom->Set_RotationEuler(Vec3::Zero);
    }

    return true;
}

Shared<ModelCom> Animation_PreviewObject::Get_ModelComponent() const
{
    return _modelCom;
}

wstring Animation_PreviewObject::Resolve_PreviewShaderPath(const wchar_t* fileName)
{
    const fs::path assetRoot = fs::path(GAME->Get_AssetRoot());
    return (assetRoot.parent_path() / L"ShaderFiles" / fileName).lexically_normal().wstring();
}

wstring Animation_PreviewObject::Resolve_ModelTablePath(const string& path)
{
    if (path.empty())
        return {};

    const fs::path inputPath = fs::path(String::ToWString(path));
    if (inputPath.is_absolute())
        return inputPath.lexically_normal().wstring();

    const fs::path assetRoot = fs::path(GAME->Get_AssetRoot());
    const fs::path clientBinPath = assetRoot.parent_path();
    wstring normalized = inputPath.wstring();

    for (wchar_t& ch : normalized)
    {
        if (ch == L'/')
            ch = L'\\';
        else
            ch = static_cast<wchar_t>(towlower(ch));
    }

    const wstring resourcesPrefix = L"..\\bin\\resources\\";
    if (normalized.size() >= resourcesPrefix.size() &&
        0 == normalized.compare(0, resourcesPrefix.size(), resourcesPrefix))
    {
        return (assetRoot / fs::path(normalized.substr(resourcesPrefix.size()))).lexically_normal().wstring();
    }

    return (clientBinPath / inputPath).lexically_normal().wstring();
}

wstring Animation_PreviewObject::Find_ModelPrototypeTag(const wstring& modelPath)
{
    const fs::path tablePath = fs::path(GAME->Get_AssetRoot()) / L"Data" / L"json" / L"DT_Model.json";
    std::ifstream file(tablePath);
    if (!file.is_open())
        return {};

    json root{};
    file >> root;

    if (!root.contains("DT_Model") || !root["DT_Model"].is_array())
        return {};

    const wstring targetPath = fs::absolute(fs::path(modelPath)).lexically_normal().wstring();
    for (const json& item : root["DT_Model"])
    {
        const string rawPath = item.value("Path", string{});
        const string id = item.value("Id", string{});
        if (rawPath.empty() || id.empty())
            continue;

        if (Resolve_ModelTablePath(rawPath) == targetPath)
            return String::ToWString(id);
    }

    return {};
}

HRESULT Animation_PreviewObject::Ready_Components()
{
    _staticShaderCom = ShaderCom::Create(
        _device,
        _context,
        Resolve_PreviewShaderPath(L"Shader_VtxMesh.hlsl").c_str(),
        VTXMESH::Elements,
        VTXMESH::numElements,
        ShaderType::VtxMesh);
    if (_staticShaderCom == nullptr)
        return E_FAIL;

    _animShaderCom = ShaderCom::Create(
        _device,
        _context,
        Resolve_PreviewShaderPath(L"Shader_VtxAnimMesh.hlsl").c_str(),
        VTXANIMMESH::Elements,
        VTXANIMMESH::numElements,
        ShaderType::VtxAnimMesh);
    if (_animShaderCom == nullptr)
        return E_FAIL;

    return S_OK;
}

HRESULT Animation_PreviewObject::Bind_ShaderResources()
{
    if (_modelCom == nullptr)
        return E_FAIL;

    Shared<ShaderCom> target = _modelCom->Is_Skeletal() ? _animShaderCom : _staticShaderCom;

    if (FAILED(_transformCom->Bind_ShaderResource(target.get(), "g_WorldMatrix")))
        return E_FAIL;

    if (FAILED(target->Bind_Matrix("g_ViewMatrix", GAME->Get_Transform(D3DTS::View))))
        return E_FAIL;

    if (FAILED(target->Bind_Matrix("g_ProjMatrix", GAME->Get_Transform(D3DTS::Proj))))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraFar(target, "g_fFar")))
        return E_FAIL;

    if (FAILED(GAME->Bind_CamPosition(target.get(), "g_vCamPosition")))
        return E_FAIL;

    const LIGHT_DESC* lightDesc = GAME->Get_LightDesc(0);
    if (lightDesc != nullptr)
    {
        target->Bind_RawValue("g_vLightDir", &lightDesc->direction, sizeof(Vec4));
        target->Bind_RawValue("g_vLightDiffuse", &lightDesc->diffuse, sizeof(Vec4));
        target->Bind_RawValue("g_vLightAmbient", &lightDesc->ambient, sizeof(Vec4));
        target->Bind_RawValue("g_vLightSpecular", &lightDesc->specular, sizeof(Vec4));
    }

    return S_OK;
}

Shared<Animation_PreviewObject> Animation_PreviewObject::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    Shared<Animation_PreviewObject> instance = make_shared<Animation_PreviewObject>(device, context);
    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    if (FAILED(instance->Initialize(nullptr)))
        return nullptr;

    return instance;
}

Shared<GameObject> Animation_PreviewObject::Clone(void* arg)
{
    Shared<Animation_PreviewObject> instance = make_shared<Animation_PreviewObject>(*this);
    if (FAILED(instance->Initialize(arg)))
        return nullptr;

    return instance;
}

void Animation_PreviewObject::Free()
{
    GameObject::Free();
}
