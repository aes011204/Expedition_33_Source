#include "RuntimeCheckpointService.h"

#include "ClientInstance.h"
#include "CostumeManager.h"
#include "GameInstance.h"
#include "GameObject.h"
#include "MovementCom.h"
#include "PlayerControllerCom.h"
#include "PlayerInventory.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "SkillTreeCom.h"
#include "TransformCom.h"
#include "Unlock_Manager.h"

NS_BEGIN(Client)

bool RuntimeCheckpointService::Capture_Current(RuntimeCheckpointIO::Checkpoint& outCheckpoint)
{
    RuntimeCheckpointIO::Checkpoint checkpoint{};
    checkpoint.levelType = static_cast<LevelType>(GAME->Current_LevelIndex());
    checkpoint.activeSlot = CLIENT->Get_ActiveHeroSlot();

    if (const Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory())
        checkpoint.inventoryJson = inventory->Create_SaveJson();

    if (const CostumeManager* costumeManager = CLIENT->Get_CostumeManager())
        checkpoint.costumeJson = costumeManager->Create_CurrentCostumeSaveJson();

    if (const Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager())
        checkpoint.unlockManagerJson = unlockManager->Create_SaveJson();

    uint32 slotIndex = 0;
    for (const HeroPartyEntry& entry : CLIENT->Get_HeroEntries())
    {
        if (entry.hero == nullptr)
        {
            ++slotIndex;
            continue;
        }

        Shared<TransformCom> transform = entry.hero->Get_Transform();
        if (transform == nullptr)
        {
            ++slotIndex;
            continue;
        }

        RuntimeCheckpointIO::HeroState heroState{};
        heroState.heroId = entry.heroId;
        heroState.slot = static_cast<HeroPartySlot>(slotIndex);
        heroState.position = transform->Get_WorldPosition();
        heroState.rotation = transform->Get_WorldRotationEuler();

        if (Shared<PlayerStatCom> stat = entry.hero->Get_Component<PlayerStatCom>())
            heroState.statJson = stat->Create_SaveJson();

        if (Shared<SkillTreeCom> skillTree = entry.hero->Get_Component<SkillTreeCom>())
            heroState.skillTreeJson = skillTree->Create_SaveJson();

        checkpoint.heroes.emplace_back(std::move(heroState));
        ++slotIndex;
    }

    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (!Should_CaptureWorldObject(layerTag, gameObject))
                return;

            RuntimeCheckpointIO::WorldObject object{};
            object.layerTag = layerTag;
            object.objectJson = gameObject->To_Json();
            checkpoint.worldObjects.emplace_back(std::move(object));
        });

    outCheckpoint = std::move(checkpoint);
    return true;
}

bool RuntimeCheckpointService::Apply_ToCurrentLevel(const RuntimeCheckpointIO::Checkpoint& checkpoint)
{
    if (checkpoint.levelType != static_cast<LevelType>(GAME->Current_LevelIndex()))
    {
        LOG_WARN("[RuntimeCheckpointService] level mismatch. current={}, checkpoint={}",
            GAME->Current_LevelIndex(), magic_enum::enum_name(checkpoint.levelType));
        return false;
    }

    if (Shared<PlayerInventory> inventory = CLIENT->Get_PlayerInventory())
        inventory->Apply_SaveJson(checkpoint.inventoryJson);

    CostumeManager* costumeManager = CLIENT->Get_CostumeManager();
    if (costumeManager != nullptr)
        costumeManager->Apply_CurrentCostumeSaveJson(checkpoint.costumeJson);

    if (Unlock_Manager* unlockManager = CLIENT->Get_UnlockManager())
        unlockManager->Apply_SaveJson(checkpoint.unlockManagerJson, false);

    for (const RuntimeCheckpointIO::HeroState& heroState : checkpoint.heroes)
    {
        Shared<Player_WorldBase> hero = CLIENT->Find_Hero(heroState.heroId);
        if (hero == nullptr)
            hero = CLIENT->Get_Hero(heroState.slot);
        if (hero == nullptr)
            continue;

        if (Shared<TransformCom> transform = hero->Get_Transform())
        {
            transform->Set_WorldPosition(heroState.position);
            transform->Set_WorldRotationEuler(heroState.rotation);
        }

        Sync_RestoredMovement(hero);

        if (Shared<PlayerStatCom> stat = hero->Get_Component<PlayerStatCom>())
            stat->Apply_SaveJson(heroState.statJson);

        if (Shared<SkillTreeCom> skillTree = hero->Get_Component<SkillTreeCom>())
            skillTree->Apply_SaveJson(heroState.skillTreeJson);

        if (costumeManager != nullptr)
        {
            if (const CostumePreset* preset = costumeManager->Get_CurrentCostume(heroState.heroId))
                hero->Apply_CostumePreset(*preset);
        }
    }

    CLIENT->Set_ActiveHeroSlotForCheckpoint(checkpoint.activeSlot);

    const uint32 levelIndex = GAME->Current_LevelIndex();
    vector<Shared<GameObject>> objectsToRemove{};
    GAME->Visit_CurrentLevelObjects(
        [&](const wstring& layerTag, const Shared<GameObject>& gameObject)
        {
            if (Should_CaptureWorldObject(layerTag, gameObject))
                objectsToRemove.emplace_back(gameObject);
        });

    for (const Shared<GameObject>& gameObject : objectsToRemove)
        GAME->Remove_GameObject(levelIndex, gameObject);

    GAME->Flush_RemovedGameObjects();

    for (const RuntimeCheckpointIO::WorldObject& object : checkpoint.worldObjects)
    {
        Shared<GameObject> gameObject = GAME->Instantiate_GameObjectJson(object.objectJson);
        if (gameObject == nullptr)
            continue;

        gameObject->Set_RuntimeAuthoringLoaded(true); // 체크포인트 복원 후에도 다음 저장 대상임을 유지한다.
        Sync_RestoredMovement(gameObject);

        (void)GAME->Add_GameObject(levelIndex, object.layerTag, gameObject);
    }

    CLIENT->Reset_AllHeroesToIdle();
    return true;
}

bool RuntimeCheckpointService::Should_CaptureWorldObject(const wstring& layerTag, const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return false;

    if (!gameObject->Is_RuntimeAuthoringLoaded())
        return false;

    if (gameObject->Get_ObjectType() == ObjectType::FreeCamera ||
        gameObject->Get_ObjectType() == ObjectType::TargetCamera)
    {
        return false;
    }

    return layerTag != L"Layer_Player" &&
        layerTag != L"Layer_MapStaticMesh" &&
        layerTag != L"Layer_Camera" &&
        layerTag != L"Layer_UI" &&
        layerTag != L"Layer_Effect";
}

void RuntimeCheckpointService::Sync_RestoredMovement(const Shared<GameObject>& gameObject)
{
    if (gameObject == nullptr)
        return;

    if (Shared<MovementCom> movement = gameObject->Get_Component<MovementCom>())
    {
        movement->Sync_RuntimePositionToTransform();
        movement->Clear_FollowMoveCommand();
        movement->Apply_Input({});
        movement->Stop_AllMovement();
    }

    if (Shared<PlayerControllerCom> controller = gameObject->Get_Component<PlayerControllerCom>())
        controller->Clear_FollowControl();
}

NS_END
