#include "EffectCom.h"

#include "ClientInstance.h"
#include "ContainerObject.h"
#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "GameInstance.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "PlayableParts.h"
#include "Player_WorldBase.h"
#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"
#include "TrailAnchor_Asset.h"
#include "TrailSampleProvider_ByPart.h"
#include "Weapon.h"

IMPLEMENT_REFLECTION(EffectCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "EffectCom";
    info.displayName = "EffectCom";
    info.category = "Component";

    PROPERTY_WSTRING("Effect Name", _inspectorEffectName);

    return true;
}

EffectCom::EffectCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

EffectCom::~EffectCom()
{
    Stop_AllAttachedEffects();
}

HRESULT EffectCom::Initialize_Prototype()
{
    return __super::Initialize_Prototype();
}

HRESULT EffectCom::Initialize(void* arg)
{
    CHECK_FAILED(__super::Initialize(arg), E_FAIL);

    _switchDissolve = {}; // 디졸브 런타임은 clone마다 새 상태로 시작
    _previewRim = {};
    _attachedEffects.clear();
    _vanishComplete = false;
    _inspectorEffectPlayed = false;

    return S_OK;
}

void EffectCom::BeginPlay()
{
    __super::BeginPlay();
    Play_InspectorEffectIfEnabled();
}

void EffectCom::Update(float timeDelta)
{
    const bool wasDissolving = _switchDissolve.enabled;
    const bool wasVanish = !_switchDissolve.reveal;

    _switchDissolve.Update(timeDelta);

    // 사라지면, Visible 끄기
    if (_vanishComplete &&
        wasDissolving &&
        wasVanish &&
        !_switchDissolve.enabled)
    {
        if (const Shared<GameObject> owner = Get_Owner())
        {
            owner->Set_Visible(false);

            if (const Shared<ContainerObject> container = dynamic_pointer_cast<ContainerObject>(owner))
            {
                for (const auto& [partTag, partObject] : container->Get_PartObjects())
                {
                    UNREFERENCED_PARAMETER(partTag);

                    if (partObject == nullptr)
                        continue;

                    if (const Shared<EffectCom> partEffectCom = partObject->Get_Component<EffectCom>())
                        partEffectCom->Stop_AllAttachedEffects();
                }
            }
        }

        Stop_AllAttachedEffects();
        _vanishComplete = false;
    }

    Update_AttachedEffects();
}

void EffectCom::Tick_AttachedEffectInstances(float timeDelta)
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        if (nullptr == attached.poolHandle.effectObject)
            continue;

        attached.poolHandle.effectObject->Try_BeginPlay();
        attached.poolHandle.effectObject->Priority_Update(timeDelta);
        attached.poolHandle.effectObject->Update(timeDelta);
        attached.poolHandle.effectObject->Late_Update(timeDelta);
    }
}

void EffectCom::Capture_AttachedEffectPauseStates(vector<AttachedEffectPauseState>& outStates)
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        const Shared<EffectInstance> effect =
            dynamic_pointer_cast<EffectInstance>(attached.poolHandle.effectObject);
        if (effect == nullptr)
            continue;

        const bool alreadyCaptured = any_of(outStates.begin(), outStates.end(),
            [&effect](const AttachedEffectPauseState& state)
            {
                return state.effect == effect;
            });

        if (alreadyCaptured)
            continue;

        AttachedEffectPauseState state{};
        state.effect = effect;
        state.wasPaused = effect->Is_PlaybackPaused();

        effect->Set_PlaybackPaused(true); // 튜토리얼 UI가 열린 동안 attached trail/effect 시간을 고정한다.
        outStates.push_back(state);
    }
}

void EffectCom::Restore_AttachedEffectPauseStates(vector<AttachedEffectPauseState>& states)
{
    for (AttachedEffectPauseState& state : states)
    {
        if (state.effect == nullptr)
            continue;

        state.effect->Set_PlaybackPaused(state.wasPaused); // 튜토리얼 시작 전 effect pause 상태로 되돌린다.
    }

    states.clear();
}

void EffectCom::Start_SwitchDissolve(float durationSec, const string& noiseName)
{
    const uint32 noiseHandle = GAME->Get_Handle(noiseName);

    _vanishComplete = false;
    _switchDissolve.Start_Reveal(durationSec, noiseHandle, 0.08f, 0.02f, Vec3(0.10f, 0.10f, 0.10f));
}

void EffectCom::Start_VanishDissolve(float durationSec, const string& noiseName, bool vanishComplete)
{
    const uint32 noiseHandle = GAME->Get_Handle(noiseName);

    _vanishComplete = vanishComplete;
    _switchDissolve.Start_Vanish(durationSec, noiseHandle, 0.08f, 0.02f, Vec3(0.10f, 0.10f, 0.10f));
}

void EffectCom::Set_PreviewRim(const Vec3& color, float intensity, float power, float hairIntensity)
{
    _previewRim.Start(color, intensity, power, hairIntensity);
}

void EffectCom::Clear_PreviewRim()
{
    _previewRim.Stop();
}

void EffectCom::Stop_AllAttachedEffects()
{
    for (AttachedEffect& attached : _attachedEffects)
        Release_AttachedEffect(attached);

    Prune_ReleasedAttachedEffects();
}

HRESULT EffectCom::Bind_DrawFeatures(ShaderCom* shader, uint32 supportFeatureMask)
{
    CHECK_NULL_THROTTLED(shader, 60, E_FAIL);

    DrawFeatureCB drawFeatureCB{};

    constexpr uint32 featureTextureCount =
        static_cast<uint32>(ShaderSRV::DrawFeatureTextureSlotNames.size());

    array<ShaderResourceView*, featureTextureCount> featureSRVs{};
    drawFeatureCB.supportFeatureMask = supportFeatureMask;

    if (_switchDissolve.enabled == true &&
        (supportFeatureMask & DrawFeatureFlagDissolve) != 0u)
    {
        drawFeatureCB.activeFeatureMask |= DrawFeatureFlagDissolve;
        drawFeatureCB.param0.x = _switchDissolve.progress;
        drawFeatureCB.param0.y = _switchDissolve.edgeWidth;
        drawFeatureCB.param0.z = _switchDissolve.edgeSoftness;
        drawFeatureCB.param0.w = _switchDissolve.reveal ? 1.f : 0.f;
        drawFeatureCB.param1 = Vec4(_switchDissolve.edgeColor.x, _switchDissolve.edgeColor.y, _switchDissolve.edgeColor.z, 0.45f);

        if (ShaderResourceView* noiseSRV = GAME->Get_SRV(_switchDissolve.noiseHandle))
        {
            featureSRVs[DrawFeatureTexDissolveNoise] = noiseSRV;
            drawFeatureCB.textureSlotMask |= 1u << DrawFeatureTexDissolveNoise;
        }
    }

    if (_previewRim.enabled == true &&
        (supportFeatureMask & DrawFeatureFlagPreviewRim) != 0u)
    {
        drawFeatureCB.activeFeatureMask |= DrawFeatureFlagPreviewRim;
        drawFeatureCB.param2 = Vec4(_previewRim.color.x, _previewRim.color.y, _previewRim.color.z, _previewRim.intensity);
        drawFeatureCB.param3 = Vec4(_previewRim.power, _previewRim.hairIntensity, 0.f, 0.f);
    }

    return shader->Bind_DrawFeatures(drawFeatureCB, featureSRVs.data(), featureTextureCount);
}

HRESULT EffectCom::Bind_DrawFeatures(const Shared<GameObject>& owner, ShaderCom* shader, uint32 supportFeatureMask)
{
    // Render 쪽에 있던 owner->Get_Component<EffectCom>() 조회와 fallback reset을 여기로 이동
    CHECK_NULL_THROTTLED(shader, 60, E_FAIL);

    Shared<EffectCom> effectCom{};

    if (owner)
        effectCom = owner->Get_Component<EffectCom>();

    if (effectCom)
        return effectCom->Bind_DrawFeatures(shader, supportFeatureMask);

    return shader->Reset_DrawFeatures();
}

Shared<EffectCom> EffectCom::Find_FromNotifyOwner(GameObject* notifyOwner)
{
    Shared<GameObject> current = notifyOwner ? notifyOwner->GetSharedPtr<GameObject>() : nullptr;

    while (current)
    {
        if (Shared<EffectCom> effectCom = current->Get_Component<EffectCom>())
            return effectCom;

        current = current->Get_Owner();
    }

    return nullptr;
}

EffectCom::InspectorEffectSettings EffectCom::Get_InspectorEffectSettings() const
{
    InspectorEffectSettings settings{};
    settings.autoPlay = _inspectorAutoPlayEffect;
    settings.effectName = _inspectorEffectName;
    settings.target = static_cast<EffectPlayTarget>(_inspectorEffectTarget);
    settings.partTag = _inspectorPartTag;
    settings.socketName = _inspectorSocketName;
    settings.layerTag = _inspectorLayerTag;
    settings.localOffset = _inspectorLocalOffset;
    settings.localRotation = _inspectorLocalRotation;
    settings.scale = _inspectorScale;
    settings.followTarget = _inspectorFollowTarget;
    settings.requiresManualRelease = _inspectorRequiresManualRelease;
    return settings;
}

void EffectCom::Set_InspectorEffectSettings(const InspectorEffectSettings& settings)
{
    _inspectorAutoPlayEffect = settings.autoPlay;
    _inspectorEffectName = settings.effectName;
    _inspectorEffectTarget = static_cast<int>(settings.target);
    _inspectorPartTag = settings.partTag;
    _inspectorSocketName = settings.socketName;
    _inspectorLayerTag = settings.layerTag;
    _inspectorLocalOffset = settings.localOffset;
    _inspectorLocalRotation = settings.localRotation;
    _inspectorScale = settings.scale;
    _inspectorFollowTarget = settings.followTarget;
    _inspectorRequiresManualRelease = settings.requiresManualRelease;
    _inspectorEffectPlayed = false;
}

HRESULT EffectCom::Play_InspectorEffect()
{
    if (_inspectorEffectName.empty())
        return E_FAIL;

    EffectPlayDesc desc{};
    desc.levelIndex = GAME->Current_LevelIndex();
    desc.effectName = _inspectorEffectName;
    desc.target = static_cast<EffectPlayTarget>(_inspectorEffectTarget);
    desc.partTag = _inspectorPartTag;
    desc.socketName = _inspectorSocketName;
    desc.layerTag = _inspectorLayerTag;
    desc.localOffset = _inspectorLocalOffset;
    desc.localRotation = _inspectorLocalRotation;
    desc.scale = _inspectorScale;
    desc.followTarget = _inspectorFollowTarget;
    desc.requiresManualRelease = _inspectorRequiresManualRelease;

    return Play_Effect(desc);
}

HRESULT EffectCom::Play_Effect(const EffectPlayDesc& desc)
{
    return Play_Effect(desc, nullptr);
}

HRESULT EffectCom::Play_Effect(const EffectPlayDesc& desc, EffectInstancePool::AttachedHandle* outHandle)
{
    if (desc.effectName.empty() || desc.layerTag.empty())
    {
        LOG_WARN(
            "[EffectCom] Play_Effect failed: empty effectName or layerTag. effect={}, layer={}",
            String::ToString(desc.effectName),
            String::ToString(desc.layerTag));
        return E_FAIL;
    }

    Vec3 spawnWorldPosition{};
    Vec3 spawnWorldRotation{};
    if (!Try_ResolveEffectWorldTransform(
        desc.target,
        desc.partTag,
        desc.socketName,
        desc.localOffset,
        desc.localRotation,
        spawnWorldPosition,
        spawnWorldRotation
    ))
        return E_FAIL;

    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    if (effectPool == nullptr)
    {
        LOG_WARN(
            "[EffectCom] Play_Effect failed: effect pool is null. effect={}, layer={}",
            String::ToString(desc.effectName),
            String::ToString(desc.layerTag));
        return E_FAIL;
    }

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = desc.effectName;
    acquireDesc.layerLevelIndex = desc.levelIndex;
    acquireDesc.layerTag = desc.layerTag;
    acquireDesc.worldPosition = spawnWorldPosition;
    acquireDesc.worldRotation = spawnWorldRotation;
    acquireDesc.scale = desc.scale;
    acquireDesc.trailSampleProvider = desc.trailSampleProvider;

    EffectInstancePool::AttachedHandle handle{};
    const HRESULT acquireResult = effectPool->Acquire(acquireDesc, handle);
    if (FAILED(acquireResult))
    {
        LOG_WARN(
            "[EffectCom] Play_Effect failed: pool acquire. hr={}, effect={}, level={}, layer={}, pos=({:.3f},{:.3f},{:.3f})",
            acquireResult,
            String::ToString(desc.effectName),
            desc.levelIndex,
            String::ToString(desc.layerTag),
            spawnWorldPosition.x,
            spawnWorldPosition.y,
            spawnWorldPosition.z);
        return E_FAIL;
    }
    if (desc.requiresManualRelease)
        handle.requiresManualRelease = true;

    AttachedEffect attached{};
    attached.poolHandle = handle;
    attached.effectName = desc.effectName;
    attached.target = desc.target;
    attached.partTag = desc.partTag;
    attached.socketName = desc.socketName;
    attached.layerTag = desc.layerTag;
    attached.localOffset = desc.localOffset;
    attached.localRotation = desc.localRotation;
    attached.scale = desc.scale;
    attached.followTarget = desc.followTarget;
    attached.isTrail = false;
    attached.trailSampleProvider = desc.trailSampleProvider.lock();
    _attachedEffects.push_back(attached);

    if (outHandle != nullptr)
        *outHandle = handle;

    if (attached.poolHandle.effectObject && attached.poolHandle.effectObject->Get_Transform())
    {
        const Vec3 pos = attached.poolHandle.effectObject->Get_Transform()->Get_WorldPosition();
        LOG_WARN("Attached effect spawned at: {}, {}, {}", pos.x, pos.y, pos.z);
    }

    return S_OK;
}

void EffectCom::Stop_Effect(
    const wstring& effectName,
    EffectPlayTarget target,
    const wstring& partTag,
    const string& socketName)
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.isTrail)
            continue;
        if (attached.effectName != effectName)
            continue;
        if (attached.target != target)
            continue;
        if (attached.partTag != partTag)
            continue;
        if (attached.socketName != socketName)
            continue;

        Release_AttachedEffect(attached);
    }

    Prune_ReleasedAttachedEffects();
}

void EffectCom::Stop_Effect(const EffectInstancePool::AttachedHandle& handle)
{
    if (handle.effectObject == nullptr)
        return;

    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.poolHandle.effectObject != handle.effectObject)
            continue;

        Release_AttachedEffect(attached);
        break;
    }

    Prune_ReleasedAttachedEffects();
}

void EffectCom::Play_InspectorEffectIfEnabled()
{
    if (!_inspectorAutoPlayEffect || _inspectorEffectPlayed || _inspectorEffectName.empty())
        return;

    if (SUCCEEDED(Play_InspectorEffect()))
        _inspectorEffectPlayed = true;
}

bool EffectCom::Try_ResolveEffectWorldTransform(
    EffectPlayTarget target,
    const wstring& partTag,
    const string& socketName,
    const Vec3& localOffset,
    const Vec3& localRotation,
    Vec3& outWorldPosition,
    Vec3& outWorldRotation) const
{
    Matrix baseWorldMatrix = Matrix::Identity;

    if (target == EffectPlayTarget::OwnerRoot)
    {
        if (!Try_GetOwnerRootWorldMatrix(baseWorldMatrix))
            return false;
    }
    else if (target == EffectPlayTarget::PartSocket)
    {
        if (!Try_GetSocketWorldMatrix(partTag, socketName, baseWorldMatrix))
            return false;
    }
    else
    {
        if (!Try_GetPartRootWorldMatrix(partTag, baseWorldMatrix))
            return false;
    }

    const Vec3 localRotationRad(
        XMConvertToRadians(localRotation.x),
        XMConvertToRadians(localRotation.y),
        XMConvertToRadians(localRotation.z)
    );

    // local offset/rotation은 owner root 또는 socket matrix 기준으로 합성한다.
    const Matrix localMatrix =
        Matrix::CreateFromYawPitchRoll(localRotationRad.y, localRotationRad.x, localRotationRad.z) *
        Matrix::CreateTranslation(localOffset);

    Matrix basisWorldMatrix = baseWorldMatrix;
    if (target != EffectPlayTarget::OwnerRoot)
    {
        auto normalize_axis = [](Vec3 axis, const Vec3& fallback) -> Vec3
        {
            if (axis.LengthSquared() <= 0.000001f)
                return fallback;

            axis.Normalize();
            return axis;
        };

        const Vec3 basisRight = normalize_axis(
            Vec3(baseWorldMatrix._11, baseWorldMatrix._12, baseWorldMatrix._13),
            Vec3::Right
        );
        const Vec3 basisUp = normalize_axis(
            Vec3(baseWorldMatrix._21, baseWorldMatrix._22, baseWorldMatrix._23),
            Vec3::Up
        );
        const Vec3 basisLook = normalize_axis(
            Vec3(baseWorldMatrix._31, baseWorldMatrix._32, baseWorldMatrix._33),
            Vec3::Forward
        );

        basisWorldMatrix = Matrix::Identity;
        basisWorldMatrix._11 = basisRight.x;
        basisWorldMatrix._12 = basisRight.y;
        basisWorldMatrix._13 = basisRight.z;
        basisWorldMatrix._21 = basisUp.x;
        basisWorldMatrix._22 = basisUp.y;
        basisWorldMatrix._23 = basisUp.z;
        basisWorldMatrix._31 = basisLook.x;
        basisWorldMatrix._32 = basisLook.y;
        basisWorldMatrix._33 = basisLook.z;
        basisWorldMatrix._41 = baseWorldMatrix._41;
        basisWorldMatrix._42 = baseWorldMatrix._42;
        basisWorldMatrix._43 = baseWorldMatrix._43;
    }

    Matrix finalWorldMatrix = localMatrix * basisWorldMatrix;

    Vec3 decomposedScale = Vec3::One;
    Quat decomposedRotation = Quat::Identity;
    Vec3 decomposedTranslation = Vec3::Zero;
    if (!finalWorldMatrix.Decompose(decomposedScale, decomposedRotation, decomposedTranslation))
        return false;

    const Vec3 eulerRad = decomposedRotation.ToEuler();

    outWorldPosition = decomposedTranslation;
    outWorldRotation = Vec3(
        XMConvertToDegrees(eulerRad.x),
        XMConvertToDegrees(eulerRad.y),
        XMConvertToDegrees(eulerRad.z)
    );
    return true;
}

HRESULT EffectCom::Play_AttachedEffect(
    uint32 levelIndex,
    const wstring& effectName,
    const wstring& partTag,
    const string& socketName,
    const wstring& layerTag,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale,
    const Weak<IEffectTrailSampleProvider>& trailSampleProvider)
{
    EffectPlayDesc desc{};
    desc.levelIndex = levelIndex;
    desc.effectName = effectName;
    desc.target = EffectPlayTarget::PartSocket;
    desc.partTag = partTag;
    desc.socketName = socketName;
    desc.layerTag = layerTag;
    desc.localOffset = localOffset;
    desc.localRotation = localRotation;
    desc.scale = scale;
    desc.followTarget = true; // attached effect는 EffectCom Update에서 계속 socket transform을 따라간다.
    desc.trailSampleProvider = trailSampleProvider;

    return Play_Effect(desc);
}

HRESULT EffectCom::Play_AttachedPartEffect(
    uint32 levelIndex,
    const wstring& effectName,
    const wstring& partTag,
    const string& socketName,
    const wstring& layerTag,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale)
{
    EffectPlayDesc desc{};
    desc.levelIndex = levelIndex;
    desc.effectName = effectName;
    desc.target = socketName.empty() ? EffectPlayTarget::PartRoot : EffectPlayTarget::PartSocket;
    desc.partTag = partTag;
    desc.socketName = socketName;
    desc.layerTag = layerTag;
    desc.localOffset = localOffset;
    desc.localRotation = localRotation;
    desc.scale = scale;
    desc.followTarget = true; // socketName이 비어 있으면 part root, 있으면 socket을 계속 따라간다.

    return Play_Effect(desc);
}

HRESULT EffectCom::Play_AttachedPartEffect(
    uint32 levelIndex,
    const wstring& effectName,
    const wstring& partTag,
    const wstring& layerTag,
    const Vec3& localOffset,
    const Vec3& localRotation,
    const Vec3& scale)
{
    return Play_AttachedPartEffect(
        levelIndex,
        effectName,
        partTag,
        "",
        layerTag,
        localOffset,
        localRotation,
        scale
    );
}

HRESULT EffectCom::Play_TrailEffect(const wstring& effectName, const wstring& partTag, const wstring& layerTag)
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    CHECK_NULL(ownerContainer, E_FAIL);

    const Weapon* weaponPart = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(partTag));
    CHECK_NULL(weaponPart, E_FAIL);

    wstring resolvedEffectName{};
    if (!Try_ResolveTrailEffectName(effectName, partTag, resolvedEffectName))
        return E_FAIL;

    const Shared<ModelCom> weaponModel = weaponPart->Get_Model();
    CHECK_NULL(weaponModel, E_FAIL);

    const wstring& sourceModelPath = weaponModel->Get_SourceModelPath();
    if (sourceModelPath.empty())
        return E_FAIL;

    const string modelGuid = GAME->Ensure_AssetGUID(sourceModelPath, "Model");
    if (modelGuid.empty())
        return E_FAIL;

    AttachedEffect attached{};
    attached.effectName = resolvedEffectName; // Stop에서도 같은 resolved 이름으로 찾아야 한다.
    attached.partTag = partTag;
    attached.layerTag = layerTag;
    attached.isTrail = true;

    const fs::path anchorPath = TrailAnchor_Parser::Get_ModelTrailAnchorFilePath(modelGuid);
    if (!TrailAnchor_Parser::Load_FromFile(anchorPath.wstring(), attached.trailAnchorAsset))
        return E_FAIL;

    EffectTrailSample initialSample{};
    if (!Try_BuildTrailSample(attached.partTag, attached.trailAnchorAsset, initialSample))
        return E_FAIL;

    attached.trailSampleProvider = make_shared<TrailSampleProvider_ByPart>(
        static_pointer_cast<EffectCom>(GetSharedPtr<EffectCom>()),
        partTag,
        attached.trailAnchorAsset
    );
    CHECK_NULL(attached.trailSampleProvider, E_FAIL);

    EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool();
    CHECK_NULL(effectPool, E_FAIL);

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = resolvedEffectName;
    acquireDesc.layerTag = layerTag;
    acquireDesc.worldPosition = initialSample.baseWorldPosition;
    acquireDesc.trailSampleProvider = attached.trailSampleProvider;

    CHECK_FAILED(effectPool->Acquire(acquireDesc, attached.poolHandle), E_FAIL);

    _attachedEffects.push_back(attached);

    return S_OK;
}

void EffectCom::Stop_TrailEffect(const wstring& effectName, const wstring& partTag)
{
    wstring resolvedEffectName{};
    if (!Try_ResolveTrailEffectName(effectName, partTag, resolvedEffectName))
        return;

    for (AttachedEffect& attached : _attachedEffects)
    {
        if (!attached.isTrail)
            continue;
        if (attached.effectName != resolvedEffectName)
            continue;
        if (attached.partTag != partTag)
            continue;

        Release_AttachedEffect(attached);
    }

    Prune_ReleasedAttachedEffects();
}

bool EffectCom::Try_GetTrailSampleForPart(
    const wstring& partTag,
    const TrailAnchorAsset& trailAnchorAsset,
    EffectTrailSample& outSample) const
{
    return Try_BuildTrailSample(partTag, trailAnchorAsset, outSample);
}

bool EffectCom::Try_ResolveTrailEffectName(
    const wstring& effectNameOverride,
    const wstring& partTag,
    wstring& outEffectName) const
{
    outEffectName.clear();

    if (!effectNameOverride.empty())
    {
        outEffectName = effectNameOverride;
        return true;
    }

    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    const Weapon* weaponPart = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(partTag));
    if (weaponPart == nullptr)
        return false;

    outEffectName = weaponPart->Get_TrailEffectName(); // notify가 비어 있으면 무기 프리펩 기본 Trail을 소비한다.
    return !outEffectName.empty();
}

void EffectCom::Update_AttachedEffects()
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.poolHandle.effectObject == nullptr)
            continue;

        if (Should_ReleaseAttachedEffect(attached))
        {
            Release_AttachedEffect(attached);
            continue;
        }

        if (attached.isTrail)
        {
            EffectTrailSample trailSample{};
            if (!Try_BuildTrailSample(attached.partTag, attached.trailAnchorAsset, trailSample))
                continue;

            const Shared<TransformCom> effectTransform = attached.poolHandle.effectObject->Get_Transform();
            CHECK_NULL(effectTransform);

            effectTransform->Set_WorldPosition(trailSample.baseWorldPosition);
            continue;
        }

        if (!attached.followTarget)
            continue; // AN 단발 이펙트는 시작 위치만 맞추고 더 이상 이동시키지 않는다.

        Vec3 targetWorldPosition{};
        Vec3 targetWorldRotation{};
        if (!Try_ResolveEffectWorldTransform(
            attached.target,
            attached.partTag,
            attached.socketName,
            attached.localOffset,
            attached.localRotation,
            targetWorldPosition,
            targetWorldRotation
        ))
            continue;

        const Shared<TransformCom> effectTransform = attached.poolHandle.effectObject->Get_Transform();
        CHECK_NULL(effectTransform);

        effectTransform->Set_WorldPosition(targetWorldPosition);
        effectTransform->Set_WorldRotationEuler(targetWorldRotation);
    }

    Prune_ReleasedAttachedEffects();
}

void EffectCom::Release_AttachedEffect(AttachedEffect& attached)
{
    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    if (nullptr == effectPool)
        return;

    if (nullptr == attached.poolHandle.effectObject)
        return;

    effectPool->Release(attached.poolHandle);
    attached.poolHandle = {};
}

bool EffectCom::Should_ReleaseAttachedEffect(const AttachedEffect& attached) const
{
    if (nullptr == attached.poolHandle.effectObject)
        return false;

    if (attached.poolHandle.requiresManualRelease)
        return false;

    const Shared<EffectInstance> effectInstance =
        dynamic_pointer_cast<EffectInstance>(attached.poolHandle.effectObject);
    if (nullptr == effectInstance)
        return false;

    return effectInstance->Is_Finished();
}

void EffectCom::Prune_ReleasedAttachedEffects()
{
    erase_if(
        _attachedEffects,
        [](const AttachedEffect& attached)
        {
            return attached.poolHandle.effectObject == nullptr;
        }
    );
}

bool EffectCom::Try_GetOwnerRootWorldMatrix(Matrix& outWorldMatrix) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    if (ownerObject == nullptr)
        return false;

    if (const Shared<PartObject> ownerPart = dynamic_pointer_cast<PartObject>(ownerObject))
    {
        outWorldMatrix = ownerPart->Get_CombinedWorldMatrix();
        return true;
    }

    const Shared<TransformCom> ownerTransform = ownerObject->Get_Transform();
    if (ownerTransform == nullptr)
        return false;

    outWorldMatrix = ownerTransform->Get_WorldMatrix();
    return true;
}

bool EffectCom::Try_GetSocketWorldMatrix(
    const wstring& partTag,
    const string& socketName,
    Matrix& outWorldMatrix) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    if (const auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * playablePart->Get_CombinedWorldMatrix();

        outWorldMatrix = socketWorldMatrix;
        return true;
    }

    if (const auto* weaponPart = dynamic_cast<Weapon*>(partObject))
    {
        const Matrix* socketMatrix = weaponPart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Matrix socketWorldMatrix = *socketMatrix * weaponPart->Get_CombinedWorldMatrix();
        outWorldMatrix = socketWorldMatrix;
        return true;
    }

    return false;
}

bool EffectCom::Try_GetPartRootWorldMatrix(const wstring& partTag, Matrix& outWorldMatrix) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    const PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    outWorldMatrix = partObject->Get_CombinedWorldMatrix();
    return true;
}

bool EffectCom::Try_BuildTrailSample(
    const wstring& partTag,
    const TrailAnchorAsset& trailAnchorAsset,
    EffectTrailSample& outSample) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    const Weapon* weaponPart = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(partTag));
    if (weaponPart == nullptr)
        return false;

    Vec3 baseWorld{};
    Vec3 tipWorld{};
    if (!Try_ResolveTrailAnchorWorldPosition(*weaponPart, trailAnchorAsset.base, baseWorld))
        return false;
    if (!Try_ResolveTrailAnchorWorldPosition(*weaponPart, trailAnchorAsset.tip, tipWorld))
        return false;

    outSample.baseWorldPosition = baseWorld;
    outSample.tipWorldPosition = tipWorld;

    return true;
}

bool EffectCom::Try_ResolveTrailAnchorWorldPosition(
    const Weapon& weaponPart,
    const TrailAnchorPointDesc& anchor,
    Vec3& outWorldPosition) const
{
    const Shared<ModelCom> weaponModel = weaponPart.Get_Model();
    if (weaponModel == nullptr || anchor.boneName.empty())
        return false;

    const int32 boneIndex = weaponModel->Get_BoneIndex(anchor.boneName);
    if (boneIndex < 0)
        return false;

    const Matrix* boneMatrix = weaponModel->Get_BoneMatrixPtr(static_cast<uint32>(boneIndex));
    if (boneMatrix == nullptr)
        return false;

    const Matrix anchorWorldMatrix = *boneMatrix * weaponPart.Get_CombinedWorldMatrix();
    outWorldPosition = Vec3::Transform(anchor.localPosition, anchorWorldMatrix);
    return true;
}

Shared<EffectCom> EffectCom::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<EffectCom>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
    {
        LOG_ERROR("Failed to Created : EffectCom");
        return nullptr;
    }

    return instance;
}

Shared<Component> EffectCom::Clone(void* arg)
{
    auto clone = make_shared<EffectCom>(*this);

    if (FAILED(clone->Initialize(arg)))
    {
        MSG_BOX("Failed to Cloned : EffectCom");
        return nullptr;
    }

    return clone;
}

void EffectCom::Free()
{
    Stop_AllAttachedEffects();

    __super::Free();
}
