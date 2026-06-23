#include "Mon_Dualliste_Sword_L.h"

#include "Bounding_OBB.h"
#include "ColliderCom.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "ShaderCom.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(Mon_Dualliste_Sword_L)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "Weapon";
    info.displayName = "Dualliste_Sword_L";

    PROPERTY_WSTRING("트레일 이펙트", _trailEffectName);

    return true;
}

Mon_Dualliste_Sword_L::Mon_Dualliste_Sword_L(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Dualliste_Sword_L::Mon_Dualliste_Sword_L(const Mon_Dualliste_Sword_L& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Dualliste_Sword_L::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Dualliste_Sword_L::Initialize(void* arg)
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

void Mon_Dualliste_Sword_L::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Dualliste_Sword_L::Update(float timeDelta)
{
    Matrix worldMatrix = _offsetMatrix * _transformCom->Get_WorldMatrix();

    if (_socketMatrix != nullptr)
        worldMatrix *= *_socketMatrix;

    Update_CombinedWorldMatrix(worldMatrix);

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);


    if (_attackLCom)
        _attackLCom->Update_Collider(Get_CombinedWorldMatrix());
    GAME->Add_Collider(_attackLCom, Get_CombinedWorldMatrix());
}

void Mon_Dualliste_Sword_L::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

void Mon_Dualliste_Sword_L::OnBeginOverlap(Shared<ColliderCom> self, Shared<ColliderCom> other)
{
    Monster_Part::OnBeginOverlap(self, other);

    const Shared<GameObject> owner = Get_Owner();
    if (owner == nullptr)
        return;

    owner->OnBeginOverlap(self, other);
}

HRESULT Mon_Dualliste_Sword_L::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Dualliste_Sword_L::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

json Mon_Dualliste_Sword_L::To_Json() const
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

void Mon_Dualliste_Sword_L::From_Json(const json& data)
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

const WeaponSourceHistoryAnchorDesc* Mon_Dualliste_Sword_L::Find_SourceHistoryAnchor(const string& anchorName) const
{
    for (const WeaponSourceHistoryAnchorDesc& anchor : _sourceHistoryAnchors)
    {
        if (anchor.name == anchorName)
            return &anchor;
    }

    return nullptr;
}

HRESULT Mon_Dualliste_Sword_L::Ready_Components(const MONSTER_PART_DESC& desc)
{
    Bounding_OBB::BoundingOBBDesc attackObbDesc{};
    attackObbDesc.center = Vec3(0.f, 1.0f, 1.0f);
    attackObbDesc.extents = Vec3(0.55f, 0.65f, 0.95f);

    CHECK_FAILED(Add_Component(ETOI(LevelType::Static), TEXT("Com_Collider_OBB"), _attackLCom, &attackObbDesc), E_FAIL);
    CHECK_FAILED(Rename_ComponentTag(TEXT("Com_Collider_OBB"), TEXT("Com_SL_MonsterSword_L")), E_FAIL);

    _attackLCom->Set_CollisionPreset(Collision_Preset::SL_MonsterAttack);
    _attackLCom->Set_IsActive(false);
    _attackLCom->Clear_Overlap();

    return Monster_Part::Ready_Components(desc);
}

uint32 Mon_Dualliste_Sword_L::Decide_PassIndex(Shared<Material> mat)
{
    const int opaquePass = 0;
    UNREFERENCED_PARAMETER(mat);
    return opaquePass;
}

HRESULT Mon_Dualliste_Sword_L::Ready_DefaultComponents()
{
    SWORD_DESC defaultDesc{};
    defaultDesc.modelType = ModelType::NonAnim;
    defaultDesc.modelTag = L"Model_Dualliste_Sword_L";
    defaultDesc.socketMatrix = _socketMatrix;

    return Ready_Components(defaultDesc);
}

HRESULT Mon_Dualliste_Sword_L::Ensure_RuntimeComponentsFromJson(const json& data)
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

Shared<Mon_Dualliste_Sword_L> Mon_Dualliste_Sword_L::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Dualliste_Sword_L>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Dualliste_Sword_L::Clone(void* arg)
{
    auto clone = make_shared<Mon_Dualliste_Sword_L>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Dualliste_Sword_L::Free()
{
    Monster_Part::Free();
}

NS_END
