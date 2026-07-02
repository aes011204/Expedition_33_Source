#include "Mon_PotatoBag_Weapon.h"

#include "EffectCom.h"
#include "ModelCom.h"
#include "ShaderCom.h"

NS_BEGIN(Client)

IMPLEMENT_REFLECTION(Mon_PotatoBag_Weapon)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "Weapon";
    info.displayName = "PotatoBag_Weapon";

    PROPERTY_WSTRING("트레일 이펙트", _trailEffectName);

    return true;
}

Mon_PotatoBag_Weapon::Mon_PotatoBag_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_PotatoBag_Weapon::Mon_PotatoBag_Weapon(const Mon_PotatoBag_Weapon& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_PotatoBag_Weapon::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_PotatoBag_Weapon::Initialize(void* arg)
{
    auto* desc = static_cast<WEAPON_DESC*>(arg);
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

void Mon_PotatoBag_Weapon::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_PotatoBag_Weapon::Update(float timeDelta)
{
    Matrix worldMatrix = Matrix::CreateScale(10.f) * _offsetMatrix * _transformCom->Get_WorldMatrix();

    if (_socketMatrix != nullptr)
        worldMatrix *= *_socketMatrix;

    Update_CombinedWorldMatrix(worldMatrix);

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void Mon_PotatoBag_Weapon::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);

    if (Get_Owner() == nullptr && _shader != nullptr && _model != nullptr && Is_Visible())
    {
        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

        if (Casts_DynamicShadow())
            GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
    }
}

HRESULT Mon_PotatoBag_Weapon::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_PotatoBag_Weapon::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

json Mon_PotatoBag_Weapon::To_Json() const
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

void Mon_PotatoBag_Weapon::From_Json(const json& data)
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

const WeaponSourceHistoryAnchorDesc* Mon_PotatoBag_Weapon::Find_SourceHistoryAnchor(const string& anchorName) const
{
    for (const WeaponSourceHistoryAnchorDesc& anchor : _sourceHistoryAnchors)
    {
        if (anchor.name == anchorName)
            return &anchor;
    }

    return nullptr;
}

uint32 Mon_PotatoBag_Weapon::Decide_PassIndex(Shared<Material> mat)
{
    return mat->Get_PassIndex();
}

HRESULT Mon_PotatoBag_Weapon::Ready_DefaultComponents()
{
    WEAPON_DESC defaultDesc{};
    defaultDesc.modelType = ModelType::NonAnim;
    defaultDesc.modelTag = L"Model_PotatoBag_Weapon";
    defaultDesc.socketMatrix = _socketMatrix;

    return Ready_Components(defaultDesc);
}

HRESULT Mon_PotatoBag_Weapon::Ensure_RuntimeComponentsFromJson(const json& data)
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

Shared<Mon_PotatoBag_Weapon> Mon_PotatoBag_Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_PotatoBag_Weapon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_PotatoBag_Weapon::Clone(void* arg)
{
    auto clone = make_shared<Mon_PotatoBag_Weapon>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_PotatoBag_Weapon::Free()
{
    Monster_Part::Free();
}

NS_END
