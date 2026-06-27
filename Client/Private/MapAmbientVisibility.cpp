#include "MapAmbientVisibility.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "ShaderPassFinder.h"

namespace
{
    constexpr const wchar_t* kMapAmbientVisibilityModelComponentTag = L"Com_Model";
}

IMPLEMENT_REFLECTION(MapAmbientVisibility)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();
    info.displayName = "MapAmbientVisibility";
    info.category = "World";
    return true;
}

MapAmbientVisibility::MapAmbientVisibility(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : GameObject{ device, context }
{
}

MapAmbientVisibility::MapAmbientVisibility(const MapAmbientVisibility& prototype)
    : GameObject{ prototype },
      _visible{ prototype._visible },
      _cullEnabled{ prototype._cullEnabled },
      _ambientVisibilityAmount{ prototype._ambientVisibilityAmount },
      _sourceStaticMeshIndex{ prototype._sourceStaticMeshIndex },
      _sourceStaticMeshId{ prototype._sourceStaticMeshId },
      _sourceStaticMeshModelPath{ prototype._sourceStaticMeshModelPath },
      _sourceMapPath{ prototype._sourceMapPath }
{
}

HRESULT MapAmbientVisibility::Initialize_Prototype()
{
    return S_OK;
}

HRESULT MapAmbientVisibility::Initialize(void* arg)
{
    if (FAILED(__super::Initialize(arg)))
        return E_FAIL;

    Set_ShadowCasterType(ShadowCasterType::None);

    auto pDesc = static_cast<MAP_AMBIENT_VISIBILITY_DESC*>(arg);
    if (pDesc == nullptr)
        return S_OK;

    _sourceStaticMeshIndex = pDesc->sourceStaticMeshIndex;
    _visible = pDesc->visible;
    _cullEnabled = pDesc->cullEnabled;
    _ambientVisibilityAmount = pDesc->ambientVisibilityAmount;
    _sourceStaticMeshId = pDesc->name;
    _sourceStaticMeshModelPath = pDesc->modelPath;
    _sourceMapPath = pDesc->sourceMapPath;

    if (FAILED(Ready_Components(pDesc->modelPath, pDesc->model)))
        return E_FAIL;

    wstring objectName = pDesc->name;
    if (objectName.empty() && _model != nullptr)
    {
        const wstring& sourceModelPath = _model->Get_SourceModelPath();
        if (!sourceModelPath.empty())
            objectName = fs::path(sourceModelPath).stem().wstring();
    }
    if (objectName.empty() && !pDesc->modelPath.empty())
        objectName = fs::path(pDesc->modelPath).stem().wstring();
    if (!objectName.empty())
        Set_Name(objectName);

    _transformCom->Set_Position(pDesc->position);
    _transformCom->Set_RotationQuaternion(pDesc->rotation);
    _transformCom->Set_Scale(pDesc->scale);

    return S_OK;
}

void MapAmbientVisibility::Late_Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!_visible || _model == nullptr)
        return;

    Vec3 localMin{}, localMax{};
    if (_model->Try_Get_LocalBounds(localMin, localMax))
    {
        BoundingBox localBox{};
        BoundingBox::CreateFromPoints(localBox, XMLoadFloat3(&localMin), XMLoadFloat3(&localMax));

        BoundingBox worldBox{};
        localBox.Transform(worldBox, _model->Get_PreLocalTransformMatrix() * _transformCom->Get_WorldMatrix());

        if (_cullEnabled && !GAME->IsVisible(worldBox))
            return;
    }

    GAME->Add_RenderGroup(RenderGroup::AmbientVolume, GetSharedPtr<GameObject>());
}

HRESULT MapAmbientVisibility::Render()
{
    if (_shader == nullptr || _model == nullptr)
        return S_OK;

    if (FAILED(Bind_ShaderResources()))
        return E_FAIL;

    const float ambientVisibilityAmount = std::clamp(_ambientVisibilityAmount, 0.f, 1.f);
    if (FAILED(_shader->Bind_RawValue("g_AmbientVisibilityAmount", &ambientVisibilityAmount, sizeof(float))))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(VtxMeshPassIndex::AmbientVisibility)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

HRESULT MapAmbientVisibility::RenderPickingID(uint32 id)
{
    if (!_model || !_shader || _transformCom == nullptr)
        return S_OK;

    if (FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    if (FAILED(_shader->Bind_RawValue("g_iObjectID", &id, sizeof(uint32))))
        return E_FAIL;

    if (FAILED(_shader->Reset_DrawFeatures()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());
    for (uint32 i = 0; i < numMeshes; ++i)
    {
        if (FAILED(_shader->Begin(VtxMeshPassIndex::IDPass)))
            return E_FAIL;

        _model->Render(i);
    }

    return S_OK;
}

bool MapAmbientVisibility::Try_Get_ShadowWorldBounds(BoundingBox& outWorldBounds) const
{
    return false;
}

HRESULT MapAmbientVisibility::Ready_Components(const wstring& modelPath, Shared<ModelCom> sharedModel)
{
    if (FAILED(__super::Add_Component(ETOI(LevelType::Static), TEXT("Shader_VtxMesh"), _shader)))
        return E_FAIL;

    if (sharedModel)
    {
        _model = make_shared<ModelCom>(*sharedModel);
        if (_model == nullptr || FAILED(_model->Initialize(nullptr)))
            return E_FAIL;
    }
    else
    {
        string pathA = String::ToString(modelPath);
        _model = ModelCom::Create(_device, _context, ModelType::NonAnim, pathA.c_str(), Matrix::CreateScale(0.001f));
        if (!_model)
            return E_FAIL;
    }

    Register_ModelComponent();

    return S_OK;
}

void MapAmbientVisibility::Register_ModelComponent()
{
    if (_model == nullptr)
        return;

    _model->Set_Owner(GetSharedPtr<GameObject>());

    if (_components.find(kMapAmbientVisibilityModelComponentTag) == _components.end())
        _components.emplace(kMapAmbientVisibilityModelComponentTag, static_pointer_cast<Component>(_model));
}

HRESULT MapAmbientVisibility::Bind_ShaderResources()
{
    if (FAILED(_shader->Bind_ObjectCB(_transformCom->Get_WorldMatrix())))
        return E_FAIL;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    return S_OK;
}

json MapAmbientVisibility::To_Json() const
{
    json root = __super::To_Json();
    root["Visible"] = _visible;
    root["CullEnabled"] = _cullEnabled;
    root["AmbientVisibilityAmount"] = _ambientVisibilityAmount;

    bool hasModelComponentJson = false;
    if (root.contains("components") && root["components"].is_array())
    {
        for (const json& componentJson : root["components"])
        {
            if (!componentJson.is_object())
                continue;

            if (componentJson.value("prototype_tag", string{}) == String::ToString(kMapAmbientVisibilityModelComponentTag))
            {
                hasModelComponentJson = true;
                break;
            }
        }
    }

    if (!hasModelComponentJson && _model != nullptr)
    {
        json modelJson = _model->To_Json();
        modelJson["prototype_tag"] = String::ToString(kMapAmbientVisibilityModelComponentTag);

        if (!root.contains("components") || !root["components"].is_array())
            root["components"] = json::array();

        root["components"].emplace_back(modelJson);
    }

    return root;
}

void MapAmbientVisibility::From_Json(const json& data)
{
    _visible = data.value("Visible", true);
    _cullEnabled = data.value("CullEnabled", true);
    _ambientVisibilityAmount = data.value("AmbientVisibilityAmount", 0.15f);

    __super::From_Json(data);

    if (_model != nullptr)
    {
        const wstring& sourceModelPath = _model->Get_SourceModelPath();
        if (!sourceModelPath.empty())
            Set_Name(fs::path(sourceModelPath).stem().wstring());
    }
}

Shared<MapAmbientVisibility> MapAmbientVisibility::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<MapAmbientVisibility>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        MSG_BOX("Failed to Created : MapAmbientVisibility");
        return nullptr;
    }

    return instance;
}

Shared<GameObject> MapAmbientVisibility::Clone(void* arg)
{
    auto clone = make_shared<MapAmbientVisibility>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void MapAmbientVisibility::Free()
{
    __super::Free();
}
