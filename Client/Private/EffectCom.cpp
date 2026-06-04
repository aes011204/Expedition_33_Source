#include "EffectCom.h"
#include "ClientInstance.h"
#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "ModelCom.h"
#include "PlayableParts.h"
#include "Player_WorldBase.h"
#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"
#include "TrailAnchor_Asset.h"
#include "TrailSampleProvider_ByPart.h"
#include "Weapon.h"
#include "ContainerObject.h"
#include "PartObject.h"

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
    return Component::Initialize_Prototype();
}

HRESULT EffectCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

    _switchDissolve = {}; // 디졸브 런타임은 clone마다 새 상태로 시작
    _previewRim = {};
    _attachedEffects.clear();
    _vanishComplete = false;
    _inspectorEffectPlayed = false;

    return S_OK;
}

void EffectCom::BeginPlay()
{
    Component::BeginPlay();
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
        if (attached.poolHandle.effectObject == nullptr)
            continue;

        attached.poolHandle.effectObject->Try_BeginPlay();
        attached.poolHandle.effectObject->Priority_Update(timeDelta);
        attached.poolHandle.effectObject->Update(timeDelta);
        attached.poolHandle.effectObject->Late_Update(timeDelta);
    }
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
    CHECK_NULL(shader , E_FAIL);

    DrawFeatureCB drawFeatureCB{};

    constexpr uint32 featureTextureCount =
        static_cast<uint32>(ShaderSRV::DrawFeatureTextureSlotNames.size());

    array<ShaderResourceView*, featureTextureCount> featureSrvs{};
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
            featureSrvs[DrawFeatureTexDissolveNoise] = noiseSRV;
            drawFeatureCB.textureSlotMask |= (1u << DrawFeatureTexDissolveNoise);
        }
    }

    if (_previewRim.enabled == true &&
        (supportFeatureMask & DrawFeatureFlagPreviewRim) != 0u)
    {
        drawFeatureCB.activeFeatureMask |= DrawFeatureFlagPreviewRim;
        drawFeatureCB.param2 = Vec4(_previewRim.color.x, _previewRim.color.y, _previewRim.color.z, _previewRim.intensity);
        drawFeatureCB.param3 = Vec4(_previewRim.power, _previewRim.hairIntensity, 0.f, 0.f);
    }

    return shader->Bind_DrawFeatures(drawFeatureCB, featureSrvs.data(), featureTextureCount);
}

HRESULT EffectCom::Bind_DrawFeatures(const Shared<GameObject>& owner, ShaderCom* shader, uint32 supportFeatureMask)
{
    // Render 쪽에 있던 owner->Get_Component<EffectCom>() 조회와 fallback reset을 여기로 이동
    CHECK_NULL(shader, E_FAIL);

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
    if (desc.effectName.empty() || desc.layerTag.empty())
        return E_FAIL;

    Vec3 spawnWorldPosition{};
    Vec3 spawnWorldRotation{};
    if (!Try_ResolveEffectWorldTransform(
            desc.target,
            desc.partTag,
            desc.socketName,
            desc.localOffset,
            desc.localRotation,
            spawnWorldPosition,
            spawnWorldRotation))
    {
        return E_FAIL;
    }

    EffectInstancePool* effectPool = CLIENT ? CLIENT->Get_EffectInstancePool() : nullptr;
    CHECK_NULL(effectPool, E_FAIL);

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = desc.effectName;
    acquireDesc.layerLevelIndex = desc.levelIndex;
    acquireDesc.layerTag = desc.layerTag;
    acquireDesc.worldPosition = spawnWorldPosition;
    acquireDesc.worldRotation = spawnWorldRotation;
    acquireDesc.scale = desc.scale;
    acquireDesc.trailSampleProvider = desc.trailSampleProvider;

    EffectInstancePool::AttachedHandle handle{};
    CHECK_FAILED(effectPool->Acquire(acquireDesc, handle), E_FAIL);
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

    return S_OK;
}

void EffectCom::Stop_Effect(const wstring& effectName, EffectPlayTarget target, const wstring& partTag,
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

void EffectCom::Play_InspectorEffectIfEnabled()
{
    if (!_inspectorAutoPlayEffect || _inspectorEffectPlayed || _inspectorEffectName.empty())
        return;

    if (SUCCEEDED(Play_InspectorEffect()))
        _inspectorEffectPlayed = true;
}

bool EffectCom::Try_ResolveEffectWorldTransform(EffectPlayTarget target, const wstring& partTag,
    const string& socketName, const Vec3& localOffset, const Vec3& localRotation,
    Vec3& outWorldPosition, Vec3& outWorldRotation) const
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
        XMConvertToRadians(localRotation.z));

    // local offset/rotation은 owner root 또는 socket matrix 기준으로 합성한다.
    const Matrix localMatrix =
        Matrix::CreateFromYawPitchRoll(localRotationRad.y, localRotationRad.x, localRotationRad.z) *
        Matrix::CreateTranslation(localOffset);

    Matrix finalWorldMatrix = localMatrix * baseWorldMatrix;

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
        XMConvertToDegrees(eulerRad.z));
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
        scale);
}

HRESULT EffectCom::Play_TrailEffect(const wstring& effectName, const wstring& partTag, const wstring& layerTag)
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    CHECK_NULL(ownerContainer, E_FAIL);

    Weapon* weaponPart = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(partTag));
    CHECK_NULL(weaponPart, E_FAIL);

    const Shared<ModelCom> weaponModel = weaponPart->Get_Model();
    CHECK_NULL(weaponModel, E_FAIL);

    const wstring& sourceModelPath = weaponModel->Get_SourceModelPath();
    if (sourceModelPath.empty())
        return E_FAIL;

    const string modelGuid = GAME->Ensure_AssetGUID(sourceModelPath, "Model");
    if (modelGuid.empty())
        return E_FAIL;

    AttachedEffect attached{};
    attached.effectName = effectName;
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
        attached.trailAnchorAsset);
    CHECK_NULL(attached.trailSampleProvider, E_FAIL);

    EffectInstancePool* effectPool = CLIENT->Get_EffectInstancePool();
    CHECK_NULL(effectPool, E_FAIL);

    EffectInstancePool::AcquireDesc acquireDesc{};
    acquireDesc.effectName = effectName;
    acquireDesc.layerTag = layerTag;
    acquireDesc.worldPosition = initialSample.baseWorldPosition;
    acquireDesc.trailSampleProvider = attached.trailSampleProvider;

    CHECK_FAILED(effectPool->Acquire(acquireDesc, attached.poolHandle), E_FAIL);

    _attachedEffects.push_back(attached);

    return S_OK;
}

void EffectCom::Stop_TrailEffect(const wstring& effectName, const wstring& partTag)
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        if (!attached.isTrail)
            continue;
        if (attached.effectName != effectName)
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
                targetWorldRotation))
        {
            continue;
        }

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

bool EffectCom::Try_GetSocketWorldMatrix(const wstring& partTag, const string& socketName,
                                           Matrix& outWorldMatrix) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<ContainerObject> ownerContainer = dynamic_pointer_cast<ContainerObject>(ownerObject);
    if (ownerContainer == nullptr)
        return false;

    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
    if (partObject == nullptr)
        return false;

    if (auto* playablePart = dynamic_cast<PlayableParts*>(partObject))
    {
        const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
        if (socketMatrix == nullptr)
            return false;

        const Shared<TransformCom> partTransform = playablePart->Get_Transform();
        const Shared<TransformCom> ownerTransform = ownerContainer->Get_Transform();
        if (partTransform == nullptr || ownerTransform == nullptr)
            return false;

        const Matrix socketWorldMatrix =
            *socketMatrix * partTransform->Get_WorldMatrix() * ownerTransform->Get_WorldMatrix();

        outWorldMatrix = socketWorldMatrix;
        return true;
    }

    if (auto* weaponPart = dynamic_cast<Weapon*>(partObject))
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

    PartObject* partObject = ownerContainer->Get_PartObject(partTag);
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

    Weapon* weaponPart = dynamic_cast<Weapon*>(ownerContainer->Get_PartObject(partTag));
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

    Component::Free();
}
