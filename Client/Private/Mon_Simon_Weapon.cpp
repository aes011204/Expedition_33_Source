#include "pch.h"
#include "Mon_Simon_Weapon.h"
#include "Asset_Manager.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "Material.h"
#include "MaterialBuilder.h"
#include "Material_Types.h"
#include "ModelCom.h"
#include "ShaderCom.h"

namespace
{
    constexpr uint32 kSimonWeaponEchoPassIndex = 15;
    constexpr const char* kSimonBladeMaterialName = "MI_Simon_Blade_Retake";
    const Vec4 kSimonBladeGhostColor = Vec4(0.70f, 1.0f, 0.92f, 1.f);
    constexpr float kSimonWeaponEchoScale = 0.95f;

    class SimonWeaponEchoTrailSampleProvider final : public IEffectTrailSampleProvider
    {
    public:
        SimonWeaponEchoTrailSampleProvider(
            const Weak<Mon_Simon_Weapon>& weapon,
            uint32 laneIndex,
            const TrailAnchorAsset& trailAnchorAsset)
            : _weapon(weapon)
            , _laneIndex(laneIndex)
            , _trailAnchorAsset(trailAnchorAsset)
        {
        }

        bool Try_GetTrailSample(EffectTrailSample& outSample) const override
        {
            const Shared<Mon_Simon_Weapon> weapon = _weapon.lock();
            if (weapon == nullptr)
                return false;

            return weapon->Try_GetWeaponEchoTrailSample(_laneIndex, _trailAnchorAsset, outSample);
        }

    private:
        Weak<Mon_Simon_Weapon> _weapon{};
        uint32 _laneIndex = 0;
        TrailAnchorAsset _trailAnchorAsset{};
    };
}

IMPLEMENT_REFLECTION(Mon_Simon_Weapon)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.category = "Weapon";
    info.displayName = "Simon_Weapon";

    PROPERTY_WSTRING("트레일 이펙트", _trailEffectName);

    return true;
}

Mon_Simon_Weapon::Mon_Simon_Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Simon_Weapon::Mon_Simon_Weapon(const Mon_Simon_Weapon& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Simon_Weapon::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Simon_Weapon::Initialize(void* arg)
{
    auto* desc = static_cast<WEAPON_DESC*>(arg);

    _socketMatrix = (desc != nullptr) ? desc->socketMatrix : nullptr;
    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (desc != nullptr)
    {
        if (FAILED(Monster_Part::Initialize(arg)))
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

void Mon_Simon_Weapon::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Simon_Weapon::Update(float timeDelta)
{
    if (_isDetached)
    {
        _combinedWorldMatrix = _detachedWorldMatrix;
        Update_WeaponEchoPlayback(timeDelta);
        if (_partEffectCom)
            _partEffectCom->Update(timeDelta);
        return;
    }

    Matrix worldMatrix = Matrix::CreateScale(10.f) * _offsetMatrix * _transformCom->Get_WorldMatrix();

    if (_useSocketBinding && _socketMatrix != nullptr)
        worldMatrix *= *_socketMatrix;

    Update_CombinedWorldMatrix(worldMatrix);
    Update_WeaponEchoPlayback(timeDelta);
    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void Mon_Simon_Weapon::Detach_PreserveWorld()
{
    _detachedWorldMatrix = Get_CombinedWorldMatrix();
    _useSocketBinding = false;
    _isDetached = true;
}

void Mon_Simon_Weapon::Attach_ToBindings()
{
    _isDetached = false;
    _useSocketBinding = true;
}

void Mon_Simon_Weapon::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);

    if (Get_Owner() == nullptr && _shader != nullptr && _model != nullptr && Is_Visible())
    {
        GAME->Add_RenderGroup(RenderGroup::NonBlend, GetSharedPtr<GameObject>());

        if (Casts_DynamicShadow())
            GAME->Add_RenderGroup(RenderGroup::Shadow, GetSharedPtr<GameObject>());
    }

    Queue_WeaponEchoRender();
}

HRESULT Mon_Simon_Weapon::Render()
{
    const HRESULT result = Monster_Part::Render();
    if (FAILED(result))
        return result;

    if (GAME->Get_CurrentRenderGroup() == RenderGroup::NonBlend)
        return Render_WeaponEchoPlayback();

    return result;
}

HRESULT Mon_Simon_Weapon::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

json Mon_Simon_Weapon::To_Json() const
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

void Mon_Simon_Weapon::From_Json(const json& data)
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

const WeaponSourceHistoryAnchorDesc* Mon_Simon_Weapon::Find_SourceHistoryAnchor(const string& anchorName) const
{
    for (const WeaponSourceHistoryAnchorDesc& anchor : _sourceHistoryAnchors)
    {
        if (anchor.name == anchorName)
            return &anchor;
    }

    return nullptr;
}

uint32 Mon_Simon_Weapon::Decide_PassIndex(Shared<Material> mat)
{
    return mat->Get_PassIndex();
}

HRESULT Mon_Simon_Weapon::Ready_DefaultComponents()
{
    WEAPON_DESC defaultDesc{};
    defaultDesc.modelType = ModelType::NonAnim;
    defaultDesc.modelTag = L"Model_Simon_Weapon";
    defaultDesc.socketMatrix = _socketMatrix;

    return Ready_Components(defaultDesc);
}

HRESULT Mon_Simon_Weapon::Ensure_RuntimeComponentsFromJson(const json& data)
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

void Mon_Simon_Weapon::Begin_WeaponEchoPlayback(
    float delaySec,
    uint32 echoCount,
    ModelCom* sourceModel,
    const string& sourceAnimationName,
    float sourceTimeSec,
    const WeaponEchoMaterialOverrideDesc* materialOverrides,
    uint32 materialOverrideCount)
{
    Stop_WeaponEchoTrail();

    for (WeaponEchoLane& lane : _weaponEchoLanes)
        Reset_WeaponEchoLane(lane);

    Update_WeaponEchoSource(sourceModel, sourceAnimationName, sourceTimeSec);

    const uint32 clampedEchoCount = std::clamp(echoCount, 0u, kMaxWeaponEchoLaneCount);
    for (uint32 laneIndex = 0; laneIndex < clampedEchoCount; ++laneIndex)
    {
        WeaponEchoLane& lane = _weaponEchoLanes[laneIndex];

        lane.active = true;
        lane.recordingEnded = false;
        lane.delaySec = max(0.f, delaySec) * static_cast<float>(laneIndex + 1);
        lane.playbackSec = _weaponEchoLastKnownSourceTimeSec;
        lane.endElapsedSec = 0.f;

        if (materialOverrides != nullptr && laneIndex < materialOverrideCount)
            lane.materialOverride = materialOverrides[laneIndex];
    }

    if (_weaponEchoPendingTrailValid)
    {
        for (uint32 laneIndex = 0; laneIndex < kMaxWeaponEchoLaneCount; ++laneIndex)
        {
            WeaponEchoLane& lane = _weaponEchoLanes[laneIndex];
            if (!lane.active)
                continue;

            Start_WeaponEchoTrail(
                lane,
                laneIndex,
                _weaponEchoPendingTrailEffectName,
                _weaponEchoPendingTrailLayerTag,
                _weaponEchoPendingTrailOffset);
        }
    }
}

void Mon_Simon_Weapon::Record_WeaponEchoSample(float elapsedSec)
{
    for (WeaponEchoLane& lane : _weaponEchoLanes)
    {
        if (!lane.active)
            continue;

        Push_WeaponEchoSample(lane, elapsedSec);
    }
}

void Mon_Simon_Weapon::End_WeaponEchoPlayback()
{
    for (WeaponEchoLane& lane : _weaponEchoLanes)
    {
        if (!lane.active)
            continue;

        lane.recordingEnded = true;
        if (!lane.samples.empty())
            lane.endElapsedSec = lane.samples.back().elapsedSec;
    }
}

void Mon_Simon_Weapon::Update_WeaponEchoSource(ModelCom* sourceModel, const string& sourceAnimationName, float sourceTimeSec)
{
    _weaponEchoSourceModel = sourceModel;
    _weaponEchoSourceAnimationName = sourceAnimationName;
    _weaponEchoLastKnownSourceTimeSec = max(0.f, sourceTimeSec);
    _weaponEchoHasSourceTime = sourceModel != nullptr && !sourceAnimationName.empty();
}

bool Mon_Simon_Weapon::Has_WeaponEchoPlayback() const
{
    for (const WeaponEchoLane& lane : _weaponEchoLanes)
    {
        if (Has_WeaponEchoPlayback(lane))
            return true;
    }

    return false;
}

bool Mon_Simon_Weapon::Has_WeaponEchoPlayback(const WeaponEchoLane& lane) const
{
    if (!lane.active || lane.samples.empty())
        return false;

    const float playbackStartSec = lane.samples.front().elapsedSec + lane.delaySec;
    if (!lane.recordingEnded)
        return lane.playbackSec >= playbackStartSec;

    return lane.playbackSec <= lane.delaySec + lane.endElapsedSec;
}

void Mon_Simon_Weapon::Queue_WeaponEchoRender()
{
    if (!Has_WeaponEchoPlayback())
        return;

    if (_shader == nullptr || _model == nullptr)
        return;

    if (!Is_Visible())
        return;

    if (const Shared<GameObject> owner = Get_Owner())
    {
        if (!owner->Is_Visible())
            return;
    }
}

void Mon_Simon_Weapon::Update_WeaponEchoPlayback(float timeDelta)
{
    if (_weaponEchoPendingTrailValid)
    {
        _weaponEchoPendingTrailAgeSec += max(0.f, timeDelta);
        if (_weaponEchoPendingTrailAgeSec > 0.5f)
        {
            _weaponEchoPendingTrailValid = false;
            _weaponEchoPendingTrailEffectName.clear();
            _weaponEchoPendingTrailLayerTag.clear();
            _weaponEchoPendingTrailOffset = {};
            _weaponEchoPendingTrailAgeSec = 0.f;
        }
    }

    bool sourceTimeUpdated = false;
    float playbackTimeSec = _weaponEchoLastKnownSourceTimeSec;
    if (_weaponEchoSourceModel != nullptr && !_weaponEchoSourceAnimationName.empty())
    {
        const uint32 currentAnimIndex = _weaponEchoSourceModel->Get_CurrentAnimationIndex();
        const char* currentAnimName = _weaponEchoSourceModel->Get_AnimationName(currentAnimIndex);
        if (currentAnimName != nullptr && _weaponEchoSourceAnimationName == currentAnimName)
        {
            playbackTimeSec = max(0.f, _weaponEchoSourceModel->Get_CurrentAnimationTimeSec());
            _weaponEchoLastKnownSourceTimeSec = playbackTimeSec;
            _weaponEchoHasSourceTime = true;
            sourceTimeUpdated = true;
        }
    }

    if (!sourceTimeUpdated)
    {
        playbackTimeSec = _weaponEchoLastKnownSourceTimeSec + max(0.f, timeDelta);
        _weaponEchoLastKnownSourceTimeSec = playbackTimeSec;
        _weaponEchoHasSourceTime = false;
    }

    for (WeaponEchoLane& lane : _weaponEchoLanes)
    {
        if (!lane.active)
            continue;

        lane.playbackSec = playbackTimeSec;

        if (lane.recordingEnded && lane.playbackSec > lane.delaySec + lane.endElapsedSec)
        {
            Stop_WeaponEchoTrail(lane);
            Reset_WeaponEchoLane(lane);
        }
    }
}

void Mon_Simon_Weapon::Reset_WeaponEchoLane(WeaponEchoLane& lane)
{
    lane.samples.clear();
    lane.active = false;
    lane.recordingEnded = false;
    lane.delaySec = 0.2f;
    lane.playbackSec = 0.f;
    lane.endElapsedSec = 0.f;
    lane.trailAnchorAsset = {};
    lane.trailSampleProvider.reset();
    lane.trailHandle = {};
    lane.trailOffset = {};
    lane.trailStarted = false;
    lane.materialOverride = {};
    lane.materialOverrideCache.clear();
}

void Mon_Simon_Weapon::Push_WeaponEchoSample(WeaponEchoLane& lane, float elapsedSec)
{
    WeaponEchoSample sample{};
    sample.elapsedSec = max(0.f, elapsedSec);
    sample.worldMatrix = _combinedWorldMatrix;

    if (!lane.samples.empty() && sample.elapsedSec + 1e-4f < lane.samples.back().elapsedSec)
        lane.samples.clear();

    if (!lane.samples.empty() && fabsf(sample.elapsedSec - lane.samples.back().elapsedSec) <= 1e-4f)
    {
        lane.samples.back() = sample;
        lane.endElapsedSec = sample.elapsedSec;
        return;
    }

    lane.samples.push_back(sample);
    lane.endElapsedSec = sample.elapsedSec;
}

bool Mon_Simon_Weapon::Try_SampleWeaponEchoMatrix(const WeaponEchoLane& lane, float elapsedSec, Matrix& outWorldMatrix) const
{
    if (lane.samples.empty())
        return false;

    if (elapsedSec <= lane.samples.front().elapsedSec)
    {
        outWorldMatrix = lane.samples.front().worldMatrix;
        return true;
    }

    if (elapsedSec >= lane.samples.back().elapsedSec)
    {
        outWorldMatrix = lane.samples.back().worldMatrix;
        return true;
    }

    for (size_t i = 1; i < lane.samples.size(); ++i)
    {
        const WeaponEchoSample& right = lane.samples[i];
        if (elapsedSec > right.elapsedSec)
            continue;

        const WeaponEchoSample& left = lane.samples[i - 1];
        const float duration = right.elapsedSec - left.elapsedSec;
        const float ratio = duration > 0.0001f
            ? std::clamp((elapsedSec - left.elapsedSec) / duration, 0.f, 1.f)
            : 1.f;

        Vec3 leftScale = Vec3::One;
        Quat leftRotation = Quat::Identity;
        Vec3 leftTranslation = Vec3::Zero;
        Vec3 rightScale = Vec3::One;
        Quat rightRotation = Quat::Identity;
        Vec3 rightTranslation = Vec3::Zero;
        Matrix leftWorldMatrix = left.worldMatrix;
        Matrix rightWorldMatrix = right.worldMatrix;

        if (!leftWorldMatrix.Decompose(leftScale, leftRotation, leftTranslation) ||
            !rightWorldMatrix.Decompose(rightScale, rightRotation, rightTranslation))
        {
            outWorldMatrix = left.worldMatrix;
            return true;
        }

        const Vec3 blendedScale = Vec3::Lerp(leftScale, rightScale, ratio);
        const Quat blendedRotation = Quat::Slerp(leftRotation, rightRotation, ratio);
        const Vec3 blendedTranslation = Vec3::Lerp(leftTranslation, rightTranslation, ratio);

        outWorldMatrix =
            Matrix::CreateScale(blendedScale) *
            Matrix::CreateFromQuaternion(blendedRotation) *
            Matrix::CreateTranslation(blendedTranslation);

        return true;
    }

    return false;
}

bool Mon_Simon_Weapon::Try_GetWeaponEchoTrailSample(
    uint32 laneIndex,
    const TrailAnchorAsset& trailAnchorAsset,
    EffectTrailSample& outSample) const
{
    if (laneIndex >= kMaxWeaponEchoLaneCount)
        return false;

    const WeaponEchoLane& lane = _weaponEchoLanes[laneIndex];
    if (!Has_WeaponEchoPlayback(lane))
        return false;

    if (_model == nullptr)
        return false;

    const float echoElapsedSec = lane.playbackSec - lane.delaySec;
    if (echoElapsedSec < 0.f)
        return false;

    Matrix echoWorldMatrix = Matrix::Identity;
    if (!Try_SampleWeaponEchoMatrix(lane, echoElapsedSec, echoWorldMatrix))
        return false;

    const TrailAnchorAsset sampleAnchorAsset =
        ANS_Trail::Make_OffsetTrailAnchorAsset(
            trailAnchorAsset,
            lane.trailOffset.tipExtendCurve,
            lane.trailOffset.baseLocalOffsetCurve,
            lane.trailOffset.tipLocalOffsetCurve,
            lane.trailOffset.phase);

    auto resolveAnchor =
        [this, &echoWorldMatrix](const TrailAnchorPointDesc& anchor, Vec3& outWorldPosition) -> bool
        {
            if (anchor.boneName.empty())
                return false;

            const int32 boneIndex = _model->Get_BoneIndex(anchor.boneName);
            if (boneIndex < 0)
                return false;

            const Matrix* boneMatrix = _model->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
            if (boneMatrix == nullptr)
                return false;

            const Matrix anchorWorldMatrix = *boneMatrix * echoWorldMatrix;
            outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
            return true;
        };

    return
        resolveAnchor(sampleAnchorAsset.base, outSample.baseWorldPosition) &&
        resolveAnchor(sampleAnchorAsset.tip, outSample.tipWorldPosition);
}

void Mon_Simon_Weapon::Begin_WeaponEchoTrail(
    const wstring& effectName,
    const wstring& layerTag,
    const EffectCom::TrailOffsetRuntimeDesc& offsetDesc)
{
    _weaponEchoPendingTrailValid = true;
    _weaponEchoPendingTrailEffectName = effectName;
    _weaponEchoPendingTrailLayerTag = layerTag;
    _weaponEchoPendingTrailOffset = offsetDesc;
    _weaponEchoPendingTrailOffset.phase = std::clamp(_weaponEchoPendingTrailOffset.phase, 0.f, 1.f);
    _weaponEchoPendingTrailAgeSec = 0.f;

    for (uint32 laneIndex = 0; laneIndex < kMaxWeaponEchoLaneCount; ++laneIndex)
    {
        WeaponEchoLane& lane = _weaponEchoLanes[laneIndex];
        if (!lane.active)
            continue;

        if (lane.trailStarted)
        {
            lane.trailOffset = offsetDesc;
            lane.trailOffset.phase = std::clamp(lane.trailOffset.phase, 0.f, 1.f);
            continue;
        }

        Start_WeaponEchoTrail(lane, laneIndex, effectName, layerTag, offsetDesc);
    }
}

void Mon_Simon_Weapon::Update_WeaponEchoTrailOffset(const EffectCom::TrailOffsetRuntimeDesc& offsetDesc)
{
    if (_weaponEchoPendingTrailValid)
    {
        _weaponEchoPendingTrailOffset = offsetDesc;
        _weaponEchoPendingTrailOffset.phase = std::clamp(_weaponEchoPendingTrailOffset.phase, 0.f, 1.f);
    }

    for (WeaponEchoLane& lane : _weaponEchoLanes)
    {
        if (!lane.active)
            continue;

        lane.trailOffset = offsetDesc;
        lane.trailOffset.phase = std::clamp(lane.trailOffset.phase, 0.f, 1.f);
    }
}

void Mon_Simon_Weapon::Start_WeaponEchoTrail(
    WeaponEchoLane& lane,
    uint32 laneIndex,
    const wstring& effectName,
    const wstring& layerTag,
    const EffectCom::TrailOffsetRuntimeDesc& offsetDesc)
{
    if (lane.trailStarted)
        return;

    if (effectName.empty() || layerTag.empty())
        return;

    if (_partEffectCom == nullptr || _model == nullptr)
        return;

    lane.trailOffset = offsetDesc;
    lane.trailOffset.phase = std::clamp(lane.trailOffset.phase, 0.f, 1.f);

    const wstring& sourceModelPath = _model->Get_SourceModelPath();
    if (sourceModelPath.empty())
        return;

    const string modelGuid = GAME->Ensure_AssetGUID(sourceModelPath, "Model");
    if (modelGuid.empty())
        return;

    lane.trailAnchorAsset = {};
    const fs::path anchorPath = TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(modelGuid);
    if (!TrailAnchor_Parser::Load_FromFile(anchorPath.wstring(), lane.trailAnchorAsset))
        return;

    lane.trailSampleProvider = make_shared<SimonWeaponEchoTrailSampleProvider>(
        GetSharedPtr<Mon_Simon_Weapon>(),
        laneIndex,
        lane.trailAnchorAsset);

    EffectCom::EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = effectName;
    desc.target = EffectCom::EffectPlayTarget::OwnerRoot;
    desc.layerTag = layerTag;
    desc.requiresManualRelease = true;
    desc.trailSampleProvider = lane.trailSampleProvider;

    if (FAILED(_partEffectCom->Play_Effect(desc, &lane.trailHandle)))
    {
        lane.trailSampleProvider.reset();
        lane.trailHandle = {};
        return;
    }

    lane.trailStarted = true;
}

void Mon_Simon_Weapon::Stop_WeaponEchoTrail()
{
    for (WeaponEchoLane& lane : _weaponEchoLanes)
        Stop_WeaponEchoTrail(lane);
}

void Mon_Simon_Weapon::Stop_WeaponEchoTrail(WeaponEchoLane& lane)
{
    if (_partEffectCom != nullptr && lane.trailHandle.effectObject != nullptr)
        _partEffectCom->Stop_Effect(lane.trailHandle);

    lane.trailHandle = {};
    lane.trailSampleProvider.reset();
    lane.trailAnchorAsset = {};
    lane.trailOffset = {};
    lane.trailStarted = false;
}

Shared<Material> Mon_Simon_Weapon::Resolve_WeaponEchoRenderMaterial(
    WeaponEchoLane& lane,
    uint32 materialIndex,
    const Shared<Material>& sourceMaterial)
{
    if (sourceMaterial == nullptr)
        return nullptr;

    if (!lane.materialOverride.enabled || lane.materialOverride.materialBundleKey.empty())
        return sourceMaterial;

    const auto it = lane.materialOverrideCache.find(materialIndex);
    if (it != lane.materialOverrideCache.end())
        return it->second != nullptr ? it->second : sourceMaterial;

    Shared<Material> renderMaterial = sourceMaterial->Clone();
    if (renderMaterial == nullptr)
    {
        lane.materialOverrideCache.emplace(materialIndex, nullptr);
        return sourceMaterial;
    }

    Asset_Manager* assetManager = GAME->Get_AssetManager();
    MaterialBuilder* materialBuilder = assetManager != nullptr ? assetManager->Get_MaterialBuilder() : nullptr;
    if (materialBuilder == nullptr ||
        !materialBuilder->Try_ApplyBundleMaterialToMaterial(lane.materialOverride.materialBundleKey, materialIndex, *renderMaterial))
    {
        lane.materialOverrideCache.emplace(materialIndex, nullptr);
        return sourceMaterial;
    }

    lane.materialOverrideCache.emplace(materialIndex, renderMaterial);
    return renderMaterial;
}

HRESULT Mon_Simon_Weapon::Render_WeaponEchoPlayback()
{
    if (!Has_WeaponEchoPlayback())
        return S_OK;

    if (FAILED(GAME->Bind_CameraCB(_shader)))
        return E_FAIL;

    if (FAILED(_shader->Reset_DrawFeatures()))
        return E_FAIL;

    const uint32 numMeshes = static_cast<uint32>(_model->Get_NumMeshes());

    for (WeaponEchoLane& lane : _weaponEchoLanes)
    {
        if (!Has_WeaponEchoPlayback(lane))
            continue;

        const float echoElapsedSec = lane.playbackSec - lane.delaySec;
        if (echoElapsedSec < 0.f)
            continue;

        Matrix echoWorldMatrix = Matrix::Identity;
        if (!Try_SampleWeaponEchoMatrix(lane, echoElapsedSec, echoWorldMatrix))
            continue;

        Vec3 echoScale = Vec3::One;
        Quat echoRotation = Quat::Identity;
        Vec3 echoTranslation = Vec3::Zero;
        if (echoWorldMatrix.Decompose(echoScale, echoRotation, echoTranslation))
        {
            echoScale *= kSimonWeaponEchoScale;
            echoWorldMatrix =
                Matrix::CreateScale(echoScale) *
                Matrix::CreateFromQuaternion(echoRotation) *
                Matrix::CreateTranslation(echoTranslation);
        }

        Vec4 ghostColor = kSimonBladeGhostColor;

        if (FAILED(_shader->Bind_ObjectCB(echoWorldMatrix)))
            return E_FAIL;

        for (uint32 i = 0; i < numMeshes; ++i)
        {
            const uint32 materialIndex = _model->Get_MeshMaterialIndex(i);
            const Shared<Material> material = _model->Get_Material(materialIndex);
            if (material == nullptr)
                continue;

            const Shared<Material> renderMaterial =
                Resolve_WeaponEchoRenderMaterial(lane, materialIndex, material);
            if (renderMaterial == nullptr)
                continue;

            const uint32 isBladeMaterial = renderMaterial->Get_MaterialName() == kSimonBladeMaterialName ? 1u : 0u;
            const uint32 useMaterialOverride = renderMaterial.get() != material.get() ? 1u : 0u;

            if (FAILED(_shader->Bind_RawValue("g_WeaponGhostColor", &ghostColor, sizeof(Vec4))))
                return E_FAIL;

            if (FAILED(_shader->Bind_RawValue("g_WeaponGhostBladeMaterial", &isBladeMaterial, sizeof(uint32))))
                return E_FAIL;

            if (FAILED(_shader->Bind_RawValue("g_WeaponGhostUseMaterialOverride", &useMaterialOverride, sizeof(uint32))))
                return E_FAIL;

            if (FAILED(_shader->Bind_MaterialTextures(*renderMaterial)))
                continue;

            if (FAILED(_shader->Bind_CBufferData(renderMaterial->Get_CB())))
                return E_FAIL;

            if (FAILED(_shader->Begin(kSimonWeaponEchoPassIndex)))
                return E_FAIL;

            _model->Render(i);
        }
    }

    return S_OK;
}

Shared<Mon_Simon_Weapon> Mon_Simon_Weapon::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Simon_Weapon>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Simon_Weapon::Clone(void* arg)
{
    auto clone = make_shared<Mon_Simon_Weapon>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Simon_Weapon::Free()
{
    Stop_WeaponEchoTrail();

    Monster_Part::Free();
}
