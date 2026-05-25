#include "EffectCom.h"
#include "EffectAssetRuntimeLoader.h"
#include "EffectInstance.h"
#include "GameInstance.h"
#include "PlayableParts.h"
#include "Player_WorldBase.h"
#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"

IMPLEMENT_REFLECTION(EffectCom)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.className = "EffectCom";
    info.displayName = "EffectCom";
    info.category = "Component";
    
    return true;
}

EffectCom::EffectCom(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Component(device, context)
{
}

HRESULT EffectCom::Initialize_Prototype()
{
    return Component::Initialize_Prototype();
}

HRESULT EffectCom::Initialize(void* arg)
{
    CHECK_FAILED(Component::Initialize(arg), E_FAIL);

    _switchDissolve = {}; // 디졸브 런타임은 clone마다 새 상태로 시작
    _attachedEffects.clear();

    return S_OK;
}

void EffectCom::BeginPlay()
{
    Component::BeginPlay();
}

void EffectCom::Update(float timeDelta)
{
    _switchDissolve.Update(timeDelta);

    Update_AttachedEffects();
}

void EffectCom::Start_SwitchDissolve(float durationSec, const string& noiseName)
{
    const uint32 noiseHandle = GAME->Get_Handle(noiseName);

    _switchDissolve.Start_Reveal(durationSec, noiseHandle, 0.08f, 0.02f, Vec3(0.10f, 0.10f, 0.10f));
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
        drawFeatureCB.activeFeatureMask = DrawFeatureFlagDissolve;
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

HRESULT EffectCom::Play_AttachedEffect(uint32 levelIndex, const wstring& effectName, const wstring& partTag, const string& socketName,
                                       const wstring& layerTag, const Vec3& localOffset)
{
    Shared<const EffectDefinition> definition = EffectAssetRuntimeLoader::Load_DefinitionByName(effectName);
    CHECK_NULL(definition, E_FAIL);

    Vec3 spawnPosition{};
    if (!Try_GetSocketWorldPosition(partTag, socketName, spawnPosition))
        return E_FAIL;

    EffectInstanceDesc desc{};
    desc.definition = definition;
    desc.position = spawnPosition + localOffset;

    Shared<GameObject> effectObject{};
    CHECK_FAILED(
        GAME->Add_GameObject(
            levelIndex, // 이거 바꿔야하는데. 일단 지금 GamePlay로 Prototype이 생성될듯
            L"GameObject_EffectInstance",
            GAME->Current_LevelIndex(),
            layerTag,
            effectObject,
            &desc),
        E_FAIL);

    AttachedEffect attached{};
    attached.effectObject = effectObject;
    attached.partTag = partTag;
    attached.socketName = socketName;
    attached.localOffset = localOffset;
    _attachedEffects.push_back(attached);

    return S_OK;
}

void EffectCom::Update_AttachedEffects()
{
    for (AttachedEffect& attached : _attachedEffects)
    {
        if (attached.effectObject == nullptr)
            continue;

        Vec3 socketPosition{};
        if (!Try_GetSocketWorldPosition(attached.partTag, attached.socketName, socketPosition))
            continue;

        const Shared<TransformCom> effectTransform = attached.effectObject->Get_Transform();
        CHECK_NULL(effectTransform);

        effectTransform->Set_WorldPosition(socketPosition + attached.localOffset);
    }
}

bool EffectCom::Try_GetSocketWorldPosition(const wstring& partTag, const string& socketName,
    Vec3& outWorldPosition) const
{
    const Shared<GameObject> ownerObject = Get_Owner();
    const Shared<Player_WorldBase> ownerPlayer = dynamic_pointer_cast<Player_WorldBase>(ownerObject);
    if (ownerPlayer == nullptr)
        return false;

    PartObject* partObject = ownerPlayer->Get_PartObject(partTag);
    PlayableParts* playablePart = dynamic_cast<PlayableParts*>(partObject);
    if (playablePart == nullptr)
        return false;

    const Matrix* socketMatrix = playablePart->Get_SocketBoneMatrixPtr(socketName);
    if (socketMatrix == nullptr)
        return false;

    const Shared<TransformCom> partTransform = playablePart->Get_Transform();
    const Shared<TransformCom> ownerTransform = ownerPlayer->Get_Transform();
    if (partTransform == nullptr || ownerTransform == nullptr)
        return false;

    const Matrix socketWorldMatrix =
        *socketMatrix * partTransform->Get_WorldMatrix() * ownerTransform->Get_WorldMatrix();

    outWorldPosition = Vec3(socketWorldMatrix._41, socketWorldMatrix._42, socketWorldMatrix._43);
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
    _attachedEffects.clear();

    Component::Free();
}
