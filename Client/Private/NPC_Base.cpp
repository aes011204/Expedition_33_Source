#include "NPC_Base.h"
#include "ClientInstance.h"
#include "ColliderCom.h"
#include "Delegate.h"
#include "GameInstance.h"
#include "Bounding_OBB.h"
#include "DialogueTypes.h"
#include "LevelTransitionFade.h"
#include "Monster_Part.h"
#include "Trigger_Interaction.h"

NPC_Base::NPC_Base(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Character_WorldBase(device, context)
{
}

NPC_Base::NPC_Base(const NPC_Base& prototype)
    : Character_WorldBase(prototype)
{
}

HRESULT NPC_Base::Initialize_Prototype()
{
    return Character_WorldBase::Initialize_Prototype();
}

void NPC_Base::Register_CommonReflectionProperties(Engine::ClassReflectionInfo& info)
{
    info.properties.push_back(Engine::PropertyInfo{
        "_defaultAnimationKey", "Default Animation", "_defaultAnimationKey",
        Engine::PropertyType::String, offsetof(NPC_Base, _defaultAnimationKey) });
    info.properties.push_back(Engine::PropertyInfo{
        "_useInteractionTrigger", "Use Interaction Trigger", "_useInteractionTrigger",
        Engine::PropertyType::Bool, offsetof(NPC_Base, _useInteractionTrigger) });
    info.properties.push_back(Engine::PropertyInfo{
        "_useBackInteractionTrigger", "Back Interaction Trigger", "_useBackInteractionTrigger",
        Engine::PropertyType::Bool, offsetof(NPC_Base, _useBackInteractionTrigger) });
    info.properties.push_back(Engine::PropertyInfo{
        "_interactionTriggerDistance", "Interaction Trigger Distance", "_interactionTriggerDistance",
        Engine::PropertyType::Float, offsetof(NPC_Base, _interactionTriggerDistance) });
    info.properties.push_back(Engine::PropertyInfo{
        "_facePlayerOnDialogueStart", "Face Player On Dialogue Start", "_facePlayerOnDialogueStart",
        Engine::PropertyType::Bool, offsetof(NPC_Base, _facePlayerOnDialogueStart) });
}

HRESULT NPC_Base::Initialize(void* arg)
{
    CHECK_FAILED(Character_WorldBase::Initialize(arg), E_FAIL);
    CHECK_FAILED(Ready_Components(), E_FAIL);
    CHECK_FAILED(Ready_PartObjects(), E_FAIL);
    Apply_DefaultAnimation();

    return S_OK;
}

void NPC_Base::BeginPlay()
{
    Character_WorldBase::BeginPlay();

    if (!_editorStopHandle.IsValid())
        _editorStopHandle = GAME->Get_DelegateHub().OnEditorStopRuntime.Add(this, &NPC_Base::On_EditorStopRuntime);

    if (!_useInteractionTrigger)
        return;

    Shared<GameObject> gameObject = nullptr;
    Trigger_Interaction::TRIGGER_INTERACTION_DESC triggerDesc{};
    triggerDesc.ownerNpc = dynamic_pointer_cast<NPC_Base>(GetSharedPtr<GameObject>());
    const float triggerDistance = abs(_interactionTriggerDistance);
    const float triggerForwardSign = _useBackInteractionTrigger ? -1.f : 1.f;
    triggerDesc.localOffset = Vec3(0.f, 0.f, triggerDistance * triggerForwardSign);

    if (FAILED(GAME->Add_GameObject(
      ETOI(LevelType::Static),
      L"GameObject_Trigger_Interaction",
      GAME->Current_LevelIndex(),
      L"Layer_Trigger",
      gameObject,
      &triggerDesc)))
    {
        return;
    }

    _sharedTrigger = dynamic_pointer_cast<Trigger_Interaction>(gameObject);
}

void NPC_Base::Update(float timeDelta)
{
    if (CLIENT->Is_WorldControlSuspended())
        return;

    if (auto* bodyPart = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body")))
        bodyPart->Play_Animation(timeDelta);

    Character_WorldBase::Update(timeDelta);
}

void NPC_Base::Late_Update(float timeDelta)
{
    if (CLIENT->Is_WorldControlSuspended())
    {
        Set_Visible(false);
        return;
    }

    Character_WorldBase::Late_Update(timeDelta);
    Update_Culling(Vec3(1.5f, 4.f, 1.5f));
}

HRESULT NPC_Base::Render()
{
    return Character_WorldBase::Render();
}

HRESULT NPC_Base::Render_Shadow()
{
    return Character_WorldBase::Render_Shadow();
}

json NPC_Base::To_Json() const
{
    json data = Character_WorldBase::To_Json();
    json& customProperties = data["custom_properties"];
    customProperties["patrol_points"] = json::array();

    for (const Vec3& patrolPoint : _PatrolPoints)
    {
        customProperties["patrol_points"].push_back(json::array({
            patrolPoint.x,
            patrolPoint.y,
            patrolPoint.z,
        }));
    }

    return data;
}

void NPC_Base::From_Json(const json& data)
{
    Character_WorldBase::From_Json(data);

    _isCinematicDriven = false;
    _PatrolPoints.clear();
    _defaultPatrolPoints.clear();

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(true);
        _colliderCom->Clear_Overlap();
    }

    if (data.contains("custom_properties") && data["custom_properties"].is_object())
    {
        const json& customProperties = data["custom_properties"];
        if (customProperties.contains("patrol_points") && customProperties["patrol_points"].is_array())
        {
            for (const json& patrolPointNode : customProperties["patrol_points"])
            {
                if (!patrolPointNode.is_array() || patrolPointNode.size() != 3)
                    continue;

                _PatrolPoints.emplace_back(
                    patrolPointNode[0].get<float>(),
                    patrolPointNode[1].get<float>(),
                    patrolPointNode[2].get<float>());
            }
        }
    }

    Apply_DefaultAnimation();
}

void NPC_Base::On_EditorStopRuntime() 
{
    Restore_AfterBattleExit();
}

void NPC_Base::BuildDialogueRequest(const string& npcPrefabName)
{
    DialogueRequest request {};

    request.dialogueKey = npcPrefabName;
    request.speaker = GetSharedPtr<GameObject>();
    request.speakerPrefabName = npcPrefabName;
  
    CLIENT->StartDialogue(request);
}

const vector<Vec3>& NPC_Base::Get_EffectivePatrolPoints()
{
    if (!_PatrolPoints.empty())
        return _PatrolPoints;

    if (_defaultPatrolPoints.empty())
        Build_DefaultPatrolPoints();

    return _defaultPatrolPoints;
}

void NPC_Base::Set_DialogueAnimation(const string& animationKey)
{
    if(animationKey.empty())
        return;
 
    auto* bodyPart = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body"));
    if (bodyPart == nullptr)
        return;

    bodyPart->Set_Animation(animationKey, 0.3f, true);
}

void NPC_Base::Apply_DefaultAnimation()
{
    Set_DialogueAnimation(_defaultAnimationKey);
}

string NPC_Base::Get_CurrentAnimationKey() const
{
    auto* bodyPart = dynamic_cast<Monster_Part*>(Get_PartObject(L"Part_Body"));
    if (bodyPart == nullptr)
        return {};

    Shared<ModelCom> model = bodyPart->Get_Model();
    if (model == nullptr)
        return {};

    const char* animationName = model->Get_AnimationName(model->Get_CurrentAnimationIndex());
    if (animationName == nullptr)
        return {};

    return animationName;
}

float NPC_Base::Get_DialogueVisualOffsetY() const
{
    if (_transformCom == nullptr)
        return 0.f;

    if (!_hasDialogueVisualBaseLocalPosition)
        return 0.f;

    return _transformCom->Get_Position().y - _dialogueVisualBaseLocalPosition.y;
}

void NPC_Base::Reset_ToDefaultAnimation()
{
    Apply_DefaultAnimation();
}

void NPC_Base::Set_DialogueVisualOffsetY(float offsetY)
{
    if (_transformCom == nullptr)
        return;

    if (!_hasDialogueVisualBaseLocalPosition)
    {
        _dialogueVisualBaseLocalPosition = _transformCom->Get_Position();
        _hasDialogueVisualBaseLocalPosition = true;
    }

    Vec3 offsetPosition = _dialogueVisualBaseLocalPosition;
    offsetPosition.y += offsetY;
    _transformCom->Set_Position(offsetPosition);
}

void NPC_Base::Reset_DialogueVisualOffset()
{
    if (_transformCom == nullptr)
        return;

    if (!_hasDialogueVisualBaseLocalPosition)
    {
        _dialogueVisualBaseLocalPosition = _transformCom->Get_Position();
        _hasDialogueVisualBaseLocalPosition = true;
    }

    _transformCom->Set_Position(_dialogueVisualBaseLocalPosition);
}

void NPC_Base::Restore_AfterBattleExit()
{
    Set_Visible(true);
    Reset_DialogueVisualOffset();

    if (_colliderCom)
    {
        _colliderCom->Set_IsActive(true);
        _colliderCom->Clear_Overlap();
    }
}

HRESULT NPC_Base::Ready_Components()
{
    Bounding_OBB::BoundingOBBDesc obbDesc{};
    obbDesc.center = Vec3(0.f, 0.6f, 0.f);
    obbDesc.extents = Vec3(0.7f, 1.f, 0.7f);

    CHECK_FAILED(Ready_Collider(L"Com_Collider_OBB", &obbDesc, Collision_Preset::NPC_WorldBody), E_FAIL);

    return S_OK;
}

HRESULT NPC_Base::Ready_PartObjects()
{
    return S_OK;
}

void NPC_Base::Build_DefaultPatrolPoints()
{
    _defaultPatrolPoints.clear();

    if (_transformCom == nullptr)
        return;

    const Vec3 origin = _transformCom->Get_Position();
    static const Vec3 offsets[] =
    {
        {  0.f, 0.f,  0.f },
        {  3.f, 0.f, -2.f },
        {  5.f, 0.f,  0.f },
        {  3.f, 0.f,  2.f },
        {  0.f, 0.f,  0.f },
        { -3.f, 0.f, -2.f },
        { -5.f, 0.f,  0.f },
        { -3.f, 0.f,  2.f },
        {  0.f, 0.f,  0.f },
    };

    _defaultPatrolPoints.reserve(_countof(offsets));
    for (const Vec3& offset : offsets)
        _defaultPatrolPoints.push_back(origin + offset);
}

void NPC_Base::Free()
{
    if (_editorStopHandle.IsValid())
    {
        GAME->Get_DelegateHub().OnEditorStopRuntime.Remove(_editorStopHandle);
        _editorStopHandle.Reset();
    }

    Character_WorldBase::Free();
}


