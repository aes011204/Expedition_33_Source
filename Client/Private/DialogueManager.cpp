#include "DialogueManager.h"
#include <DialogueTypes.h>
#include "ClientInstance.h"
#include "DialogueCamera.h"
#include "GameplayUI.h"
#include "Item.h"
#include "LevelChange_Portal.h"
#include "UI_Dialogue.h"
#include "Player_WorldBase.h"
#include "NPC_Base.h"

#include "Spawn_Prefab.h"
#include "Client_Constants.h"

namespace
{
    const Vec3 kSimonPortalPosition = Vec3(30.f, 0.f, 128.f);
    const Vec3 kSimonPortalRotation = Vec3::Zero;
    constexpr const wchar_t* kRewardPortalLayer = L"Layer_Portal";

    void FaceDialogueTarget(const Shared<GameObject>& actorObject, const Vec3& targetPos)
    {
        if (actorObject == nullptr)
            return;

        Shared<TransformCom> actorTransform = actorObject->Get_Transform();
        if (actorTransform == nullptr)
            return;

        Vec3 actorPos = actorTransform->Get_WorldPosition();
        Vec3 toTarget = targetPos - actorPos;
        toTarget.y = 0.f;

        if (toTarget.LengthSquared() <= 0.0001f)
            return;

        const float targetYawDeg = XMConvertToDegrees(atan2f(toTarget.x, toTarget.z));
        Vec3 actorRotation = actorTransform->Get_WorldRotationEuler();
        actorRotation.y = targetYawDeg;
        actorTransform->Set_WorldRotationEuler(actorRotation);
    }
}

DialogueManager::DialogueManager() = default;

void DialogueManager::StartDialogue(const DialogueRequest& request)
{
    if (_isActive || _waitInteractKeyRelease)
        return;

    const string resolvedDialogueKey = Resolve_DialogueKey(request.dialogueKey);
    const DialogueData* dialogueData = CLIENT->FindDialogueData(resolvedDialogueKey);
    if (dialogueData == nullptr || dialogueData->lines.empty())
        return;

    const uint32 currentLevelIndex = GAME->Current_LevelIndex();
    if (_uiDialogue != nullptr && _uiDialogueLevelIndex != currentLevelIndex)
    {
        _uiDialogue->Free();
        _uiDialogue.reset();
        _uiDialogueLevelIndex = 0;
    }

    if (_uiDialogue == nullptr)
    {
        _uiDialogue = UI_Dialogue::Create(currentLevelIndex);
        if (_uiDialogue == nullptr)
            return;

        _uiDialogueLevelIndex = currentLevelIndex;
    }
    else
    {
        _uiDialogue->Close();
    }

    _currentDialogueKey = resolvedDialogueKey;
    _currentDialogueData = dialogueData;
    _currentLineIndex = 0;
    _selectIndex = 0;
    _isChoiceVisible = false;
    _isActive = true;
    _waitInteractKeyRelease = true;
    _dialogueNpcObject = request.speaker;
    _dialoguePlayerObject = static_pointer_cast<GameObject>(CLIENT->Get_ActiveHero());
    CLIENT->Set_DialogueFocusActive(true);

    if (auto npc = dynamic_pointer_cast<NPC_Base>(_dialogueNpcObject))
    {
        _startAnimationKey = npc->Get_CurrentAnimationKey();
        _startAnimationOffsetY = npc->Get_DialogueVisualOffsetY();
    }
    else
    {
        _startAnimationKey.clear();
        _startAnimationOffsetY = 0.f;
    }

    AlignDialogueActors();

    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);

    CLIENT->Request_DialogueCamera();

    if (_uiDialogue)
        _uiDialogue->Show_Dialogue();

    ApplyCurrentLine();
}

void DialogueManager::ContinueDialogue()
{
    if (_currentDialogueData == nullptr)
    {
        EndDialogue();
        return;
    }

    const DialogueLine* currentLine = GetCurrentLine();
    if (currentLine == nullptr)
    {
        EndDialogue();
        return;
    }

    if (!currentLine->choices.empty())
        return;

    if (currentLine->nextLineIndex >= 0)
    {
        MoveToLine(currentLine->nextLineIndex);
        return;
    }

    if (ExecuteLineAction(currentLine->action))
        return;

    EndDialogue();
}

void DialogueManager::EndDialogue(bool restoreWorldState)
{
    if (auto npc = dynamic_pointer_cast<NPC_Base>(_dialogueNpcObject))
    {
        if (_currentDialogueData->restoreToStartAnimation == true)
        {
            if (!_startAnimationKey.empty())
                npc->Set_DialogueAnimation(_startAnimationKey);
            else
                npc->Reset_ToDefaultAnimation();

            npc->Set_DialogueVisualOffsetY(_startAnimationOffsetY);
        }
    }

    _currentDialogueKey.clear();
    _currentDialogueData = nullptr;
    _currentLineIndex = 0;
    _selectIndex = 0;
    _isChoiceVisible = false;
    _isActive = false;
    _waitInteractKeyRelease = true;
    CLIENT->Set_DialogueFocusActive(false);

    _lastAnimationKey.clear();
    _lastAnimationOffsetY = 0.f;

    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);

    if (restoreWorldState)
        CLIENT->Restore_PreviousCamera();

    if (_uiDialogue)
    {
        _uiDialogue->Close();
    }
}

void DialogueManager::Set_DialogueOverride(const string& sourceDialogueKey, const string& targetDialogueKey)
{
    if (sourceDialogueKey.empty())
        return;

    if (targetDialogueKey.empty())
    {
        _dialogueOverrides.erase(sourceDialogueKey);
        return;
    }

    _dialogueOverrides[sourceDialogueKey] = targetDialogueKey;
}

string DialogueManager::Resolve_DialogueKey(const string& dialogueKey) const
{
    const auto iter = _dialogueOverrides.find(dialogueKey);
    if (iter == _dialogueOverrides.end())
        return dialogueKey;

    return iter->second;
}

void DialogueManager::AlignDialogueActors()
{
    if (_dialoguePlayerObject == nullptr || _dialogueNpcObject == nullptr)
        return;

    Shared<TransformCom> playerTransform = _dialoguePlayerObject->Get_Transform();
    Shared<TransformCom> npcTransform = _dialogueNpcObject->Get_Transform();
    if (playerTransform == nullptr || npcTransform == nullptr)
        return;

    const Vec3 playerPos = playerTransform->Get_WorldPosition();
    const Vec3 npcPos = npcTransform->Get_WorldPosition();

    FaceDialogueTarget(_dialoguePlayerObject, npcPos);

    if (auto npc = dynamic_pointer_cast<NPC_Base>(_dialogueNpcObject))
    {
        if (npc->Should_FacePlayerOnDialogueStart())
            FaceDialogueTarget(_dialogueNpcObject, playerPos);
    }
}

void DialogueManager::Update()
{
    if (_waitInteractKeyRelease &&
        !GAME->KeyPress(KEY_TYPE::F) &&
        !GAME->KeyDown(KEY_TYPE::F) &&
        !GAME->KeyUp(KEY_TYPE::F))
    {
        _waitInteractKeyRelease = false;
    }

    if (!_isActive)
        return;

    const DialogueLine* currentLine = GetCurrentLine();
    if (currentLine == nullptr)
        return;

    if (!currentLine->choices.empty())
    {
        if (!_isChoiceVisible)
        {
            if (GAME->KeyDown(KEY_TYPE::F))
            {
                _isChoiceVisible = true;

                if (_uiDialogue)
                {
                    _uiDialogue->Show_Choices();

                    if (_selectIndex == 0)
                        _uiDialogue->Begin_FirstChoiceAnimation();
                    else if (_selectIndex == 1)
                        _uiDialogue->Begin_SecondChoiceAnimation();
                }
            }

            return;
        }

        SelectChoice();
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F))
        ContinueDialogue();
}

void DialogueManager::SelectChoice()
{
    const DialogueLine* currentLine = GetCurrentLine();
    if (currentLine == nullptr || currentLine->choices.empty())
        return;

    if (_uiDialogue != nullptr)
    {
        int clickedChoiceIndex = -1;
        if (_uiDialogue->Consume_ClickedChoice(clickedChoiceIndex) &&
            clickedChoiceIndex >= 0 &&
            clickedChoiceIndex < static_cast<int>(currentLine->choices.size()))
        {
            _selectIndex = clickedChoiceIndex;

            if (_selectIndex == 0)
                _uiDialogue->Begin_FirstChoiceAnimation();
            else if (_selectIndex == 1)
                _uiDialogue->Begin_SecondChoiceAnimation();

            ApplyChoice();
            return;
        }
    }

    bool isChanged = false;

    if (GAME->KeyDown(KEY_TYPE::DOWN) && _selectIndex + 1 < static_cast<int>(currentLine->choices.size()))
    {
        ++_selectIndex;
        isChanged = true;
    }

    if (GAME->KeyDown(KEY_TYPE::UP) && _selectIndex > 0)
    {
        --_selectIndex;
        isChanged = true;
    }

    if (isChanged)
    {
        LOG_INFO("Current Select : {}", currentLine->choices[_selectIndex].text);

        if (_uiDialogue)
        {
            if (_selectIndex == 0)
                _uiDialogue->Begin_FirstChoiceAnimation();
            else if (_selectIndex == 1)
                _uiDialogue->Begin_SecondChoiceAnimation();
        }
    }

    if (GAME->KeyDown(KEY_TYPE::F))
        ApplyChoice();
}

void DialogueManager::ApplyChoice()
{
    const DialogueLine* currentLine = GetCurrentLine();
    if (currentLine == nullptr || currentLine->choices.empty())
        return;

    if (_selectIndex < 0 || _selectIndex >= static_cast<int>(currentLine->choices.size()))
        return;

    const DialogueChoice& choice = currentLine->choices[_selectIndex];

    if (choice.action == "OpenShop")
    {
        LOG_INFO("OpenShop");
        EndDialogue();

        const Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI();
        if (gameplayUI == nullptr || !gameplayUI->Open_MerchantShop())
            LOG_WARN("DialogueManager: failed to open merchant shop.");

        return;
    }

    if (choice.action == "OpenForge")
    {
        LOG_INFO("OpenForge");
        EndDialogue();

        const Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI();
        if (gameplayUI == nullptr || !gameplayUI->Open_WeaponUpgrade())
            LOG_WARN("DialogueManager: failed to open weapon upgrade.");

        return;
    }

    if (choice.action == "OpenArenaCharacterSelect")
    {
        LOG_INFO("OpenArenaCharacterSelect");
        EndDialogue();

        const Shared<GameplayUI> gameplayUI = CLIENT->Get_GameplayUI();
        if (gameplayUI == nullptr || !gameplayUI->Open_ArenaCharacterSelect())
            LOG_WARN("DialogueManager: failed to open arena character select.");

        return;
    }

    MoveToLine(choice.nextLineIndex);
}

void DialogueManager::Reset_LastDialogueNpcAnimationToStart()
{
    auto npc = dynamic_pointer_cast<NPC_Base>(_dialogueNpcObject);
    if (npc == nullptr)
        return;

    if (!_startAnimationKey.empty())
        npc->Set_DialogueAnimation(_startAnimationKey);
    else
        npc->Reset_ToDefaultAnimation();

    npc->Set_DialogueVisualOffsetY(_startAnimationOffsetY);

    _lastAnimationKey.clear();
    _lastAnimationOffsetY = 0.f;
}

bool DialogueManager::ExecuteLineAction(const string& action)
{
    if (action.empty())
        return false;

    if (action == "Battle_PotatoBag")
    {
        EndDialogue();
        CLIENT->Request_DialogueBattlePotatoBag();
        return true;
    }

    if (action == "Spawn_DuallisteRewardPortal")
    {
        EndDialogue();

        const Shared<GameObject> spawned = Spawn_Prefab::Prefab(Constants::Prefab::Portal_GestralReward)
            .AtLevel(GAME->Current_LevelIndex())
            .InLayer(kRewardPortalLayer)
            .Position(kSimonPortalPosition)
            .Rotation(kSimonPortalRotation)
            .Spawn();

        if (spawned != nullptr)
        {
            spawned->Set_RuntimeAuthoringLoaded(true);

            if (const Shared<LevelChange_Portal> portal = dynamic_pointer_cast<LevelChange_Portal>(spawned))
                portal->Set_GameEntryRevealLevel(LevelType::Stage01);
        }

        return true;
    }

    if (action == "Spawn_WarmingUpPictosAndRemoveNpc")
    {
        Shared<GameObject> npcObject = _dialogueNpcObject;
        Vec3 spawnPosition = Vec3::Zero;

        if (npcObject != nullptr)
        {
            if (const Shared<TransformCom> npcTransform = npcObject->Get_Transform())
                spawnPosition = npcTransform->Get_WorldPosition();
        }

        EndDialogue();

        const Shared<GameObject> spawned = Spawn_Prefab::Prefab(Constants::Prefab::Item_Pictos_WarmingUp)
            .AtLevel(GAME->Current_LevelIndex())
            .InLayer(TEXT("Layer_GameObject"))
            .Position(Vec3(-82.34f, 20.75f, 71.09f))
            .Rotation(Vec3::Zero)
            .Spawn();

        if (spawned != nullptr)
        {
            spawned->Set_RuntimeAuthoringLoaded(true);

            if (const Shared<Item> spawnedItem = dynamic_pointer_cast<Item>(spawned))
                spawnedItem->Start_SpawnReveal(0.45f);
        }

        if (const Shared<NPC_Base> npc = dynamic_pointer_cast<NPC_Base>(npcObject))
            npc->Remove_InteractionTrigger();

        if (npcObject != nullptr && !npcObject->Is_MarkDestroy())
            GAME->Remove_GameObject(GAME->Current_LevelIndex(), npcObject);

        _dialogueNpcObject.reset();

        return true;
    }

    LOG_WARN("Unknown dialogue line action: {}", action.c_str());
    EndDialogue();
    return true;
}

const DialogueLine* DialogueManager::GetCurrentLine()
{
    if (_currentDialogueData == nullptr)
        return nullptr;

    if (_currentLineIndex < 0 || _currentLineIndex >= static_cast<int>(_currentDialogueData->lines.size()))
        return nullptr;

    return &_currentDialogueData->lines[_currentLineIndex];
}

void DialogueManager::MoveToLine(int lineIndex)
{
    if (_currentDialogueData == nullptr)
    {
        EndDialogue();
        return;
    }

    if (lineIndex < 0 || lineIndex >= static_cast<int>(_currentDialogueData->lines.size()))
    {
        EndDialogue();
        return;
    }

    _currentLineIndex = lineIndex;
    _selectIndex = 0;
    _isChoiceVisible = false;

    ApplyCurrentLine();
}

void DialogueManager::ApplyCurrentLine()
{
    const DialogueLine* currentLine = GetCurrentLine();
    if (currentLine == nullptr)
    {
        EndDialogue();
        return;
    }

    Shared<GameObject> speakerObject = nullptr;
    Shared<GameObject> listenerObject = nullptr;

    if (currentLine->speakerRole == DialogueActorRole::Npc)
    {
        speakerObject = _dialogueNpcObject;
        listenerObject = _dialoguePlayerObject;
    }
    else
    {
        speakerObject = _dialoguePlayerObject;
        listenerObject = _dialogueNpcObject;
    }

    if (!currentLine->speakerAnimationKey.empty() &&
        currentLine->speakerRole == DialogueActorRole::Npc)
    {
        if (auto npc = dynamic_pointer_cast<NPC_Base>(speakerObject))
        {
            const bool animationChanged = _lastAnimationKey != currentLine->speakerAnimationKey;
            const bool offsetChanged = fabsf(_lastAnimationOffsetY - currentLine->speakerOffsetY) > 0.0001f;

            if (animationChanged)
                npc->Set_DialogueAnimation(currentLine->speakerAnimationKey);

            if (animationChanged || offsetChanged)
                npc->Set_DialogueVisualOffsetY(currentLine->speakerOffsetY);

            _lastAnimationKey = currentLine->speakerAnimationKey;
            _lastAnimationOffsetY = currentLine->speakerOffsetY;
        }
    }

    if (speakerObject != nullptr && listenerObject != nullptr)
    {
        Shared<DialogueCamera> dialogueCamera =
            dynamic_pointer_cast<DialogueCamera>(GAME->Get_CameraByType(ObjectType::DialogueCamera));

        if (dialogueCamera != nullptr)
        {
            dialogueCamera->Set_CameraPresetId(currentLine->cameraPresetId);
            dialogueCamera->Set_AnchorTransform(speakerObject->Get_Transform());
            dialogueCamera->Set_SpeakerTransform(speakerObject->Get_Transform());
            dialogueCamera->Set_ListenerTransform(listenerObject->Get_Transform());
            dialogueCamera->Snap_ToDialogueTargets();
        }
    }

    if (_uiDialogue != nullptr)
    {
        const wstring displayName = String::ToWString(_currentDialogueData->displayName);
        const wstring firstChoice = currentLine->choices.size() > 0
            ? String::ToWString(currentLine->choices[0].text)
            : L"";
        const wstring secondChoice = currentLine->choices.size() > 1
            ? String::ToWString(currentLine->choices[1].text)
            : L"";

        _uiDialogue->Set_DialogueElements(currentLine->text, firstChoice, secondChoice, displayName);

        _uiDialogue->Show_Dialogue();

    }

    if (!currentLine->soundCueId.empty())
        GAME->Play_SoundCue(String::ToWString(currentLine->soundCueId));

    LOG_INFO("Dialogue Line : {}", String::ToString(currentLine->text));

    if (!currentLine->choices.empty())
        LOG_INFO("Current Select : {}", currentLine->choices[_selectIndex].text);
}

Unique<DialogueManager> DialogueManager::Create()
{
    return make_unique<DialogueManager>();
}

void DialogueManager::Free()
{
    if (_uiDialogue)
    {
        _uiDialogue->Free();
        _uiDialogue.reset();
        _uiDialogueLevelIndex = 0;
    }

    Base::Free();
}


