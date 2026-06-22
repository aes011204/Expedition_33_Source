#include "Mon_Dualliste_Sword_R.h"

#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "ShaderCom.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(Mon_Dualliste_Sword_R)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "Weapon";
    info.displayName = "Dualliste_Sword_R";

    PROPERTY_WSTRING("트레일 이펙트", _trailEffectName);

    return true;
}

Mon_Dualliste_Sword_R::Mon_Dualliste_Sword_R(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Dualliste_Sword_R::Mon_Dualliste_Sword_R(const Mon_Dualliste_Sword_R& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Dualliste_Sword_R::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Dualliste_Sword_R::Initialize(void* arg)
{
    auto* desc = static_cast<SWORD_DESC*>(arg);
    _socketMatrix = (desc != nullptr) ? desc->socketMatrix : nullptr;

    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (desc != nullptr)
    {
        if (FAILED(Monster_Part::Initialize(desc)))
            return E_FAIL;
    }
    else
    {
        if (FAILED(PartObject::Initialize(nullptr)))
            return E_FAIL;

        if (FAILED(Ready_DefaultComponents()))
            return E_FAIL;

        Set_ShadowCasterType(ShadowCasterType::Dynamic);
    }

    return S_OK;
}

void Mon_Dualliste_Sword_R::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Dualliste_Sword_R::Update(float timeDelta)
{
    Matrix worldMatrix = _offsetMatrix * _transformCom->Get_WorldMatrix();

    if (_socketMatrix != nullptr)
        worldMatrix *= *_socketMatrix;

    Update_CombinedWorldMatrix(worldMatrix);

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);


    if (_attackRCom)
        _attackRCom->Update_Collider(Get_CombinedWorldMatrix());

    GAME->Add_Collider(_attackRCom, Get_CombinedWorldMatrix());

}

void Mon_Dualliste_Sword_R::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

void Mon_Dualliste_Sword_R::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_Part::OnBeginOverlap(self, other);

    const Shared<GameObject> owner = Get_Owner();
    if (owner == nullptr)
        return;

    owner->OnBeginOverlap(self, other);
}

HRESULT Mon_Dualliste_Sword_R::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Dualliste_Sword_R::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

json Mon_Dualliste_Sword_R::To_Json() const
{
    json data = Monster_Part::To_Json();
    json& customProperties = data["custom_properties"];

    customProperties["_sourceHistoryAnchors"] = json::array();
    for (const WeaponSourceHistoryAnchorDesc& anchor : _sourceHistoryAnchors)
    {
        customProperties["_sourceHistoryAnchors"].push_back(
            {
                { "name", anchor.name },
                { "boneName", anchor.boneName },
                { "localPosition", { anchor.localPosition.x, anchor.localPosition.y, anchor.localPosition.z } },
            }
        );
    }

    return data;
}

void Mon_Dualliste_Sword_R::From_Json(const json& data)
{
    if (FAILED(Ensure_RuntimeComponentsFromJson(data)))
        return;

    Monster_Part::From_Json(data);

    _sourceHistoryAnchors.clear();
    if (data.contains("custom_properties") && data["custom_properties"].is_object())
    {
        const json& customProperties = data["custom_properties"];
        if (customProperties.contains("_sourceHistoryAnchors") && customProperties["_sourceHistoryAnchors"].is_array())
        {
            for (const json& anchorJson : customProperties["_sourceHistoryAnchors"])
            {
                if (!anchorJson.is_object())
                    continue;

                WeaponSourceHistoryAnchorDesc anchor{};
                anchor.name = anchorJson.value("name", string{});
                anchor.boneName = anchorJson.value("boneName", string{});

                if (anchorJson.contains("localPosition") &&
                    anchorJson["localPosition"].is_array() &&
                    anchorJson["localPosition"].size() >= 3)
                {
                    anchor.localPosition.x = anchorJson["localPosition"][0].get<float>();
                    anchor.localPosition.y = anchorJson["localPosition"][1].get<float>();
                    anchor.localPosition.z = anchorJson["localPosition"][2].get<float>();
                }

                if (!anchor.name.empty() && !anchor.boneName.empty())
                    _sourceHistoryAnchors.push_back(std::move(anchor));
            }
        }
    }
}

const WeaponSourceHistoryAnchorDesc* Mon_Dualliste_Sword_R::Find_SourceHistoryAnchor(const string& anchorName) const
{
    for (const WeaponSourceHistoryAnchorDesc& anchor : _sourceHistoryAnchors)
    {
        if (anchor.name == anchorName)
            return &anchor;
    }

    return nullptr;
}

uint32 Mon_Dualliste_Sword_R::Decide_PassIndex(Shared<Material> mat)
{
    const int opaquePass = 0;
    UNREFERENCED_PARAMETER(mat);
    return opaquePass;
}

HRESULT Mon_Dualliste_Sword_R::Ready_DefaultComponents()
{
    SWORD_DESC defaultDesc{};
    defaultDesc.modelType = ModelType::NonAnim;
    defaultDesc.modelTag = L"Model_Dualliste_Sword_R";
    defaultDesc.socketMatrix = _socketMatrix;

    return Ready_Components(defaultDesc);
}

HRESULT Mon_Dualliste_Sword_R::Ensure_RuntimeComponentsFromJson(const json& data)
{
    if (!data.contains("components") || !data["components"].is_array())
        return S_OK;

    auto findComponentTag =
        [this](const Shared<Component>& targetComponent) -> wstring
        {
            if (targetComponent == nullptr)
                return {};

            wstring foundTag{};
            Visit_Components(
                [&foundTag, &targetComponent](const wstring& tag, const Shared<Component>& component)
                {
                    if (foundTag.empty() && component == targetComponent)
                        foundTag = tag;
                });

            return foundTag;
        };

    for (const json& componentJson : data["components"])
    {
        if (!componentJson.is_object())
            continue;

        const string prototypeTagString = componentJson.value("prototype_tag", string{});
        if (prototypeTagString.empty())
            continue;

        const wstring prototypeTag = String::ToWString(prototypeTagString);

        if (prototypeTag == L"Shader_VtxMesh")
        {
            if (_components.find(prototypeTag) != _components.end())
                continue;

            if (_shader != nullptr)
            {
                const wstring currentShaderTag = findComponentTag(static_pointer_cast<Component>(_shader));
                if (!currentShaderTag.empty())
                    CHECK_FAILED(Rename_ComponentTag(currentShaderTag, prototypeTag), E_FAIL);
            }
            else
            {
                CHECK_FAILED(Add_Component(ETOI(LevelType::Static), prototypeTag.c_str(), _shader), E_FAIL);
            }

            continue;
        }

        if (componentJson.contains("model_type"))
        {
            if (_components.find(prototypeTag) != _components.end())
                continue;

            if (_model != nullptr)
            {
                const wstring currentModelTag = findComponentTag(static_pointer_cast<Component>(_model));
                if (!currentModelTag.empty())
                    CHECK_FAILED(Rename_ComponentTag(currentModelTag, prototypeTag), E_FAIL);
            }
            else
            {
                CHECK_FAILED(Add_Component(ETOI(LevelType::Static), prototypeTag.c_str(), _model), E_FAIL);
            }

            continue;
        }
    }

    return S_OK;
}

Shared<Mon_Dualliste_Sword_R> Mon_Dualliste_Sword_R::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Dualliste_Sword_R>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Dualliste_Sword_R::Clone(void* arg)
{
    auto clone = make_shared<Mon_Dualliste_Sword_R>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Dualliste_Sword_R::Free()
{
    Monster_Part::Free();
}

NS_END
