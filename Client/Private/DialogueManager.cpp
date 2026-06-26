#include "DialogueManager.h"
#include <DialogueTypes.h>
#include "ClientInstance.h"
#include "DialogueCamera.h"
#include "UI_Dialogue.h"
#include "Player_WorldBase.h"
#include "NPC_Base.h"

namespace
{
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

    const DialogueData* dialogueData = CLIENT->FindDialogueData(request.dialogueKey);
    if (dialogueData == nullptr || dialogueData->lines.empty())
        return;

    if (_uiDialogue != nullptr)
    {
        _uiDialogue->Free();
        _uiDialogue.reset();
    }

    _uiDialogue = UI_Dialogue::Create(GAME->Current_LevelIndex());
    if (_uiDialogue == nullptr)
        return;

    LOG_INFO("========대화 시작========");

    _currentDialogueKey = request.dialogueKey;
    _currentDialogueData = dialogueData;
    _currentLineIndex = 0;
    _selectIndex = 0;
    _isChoiceVisible = false;
    _isActive = true;
    _waitInteractKeyRelease = true;
    _dialogueNpcObject = request.speaker;
    _dialoguePlayerObject = static_pointer_cast<GameObject>(CLIENT->Get_ActiveHero());

    AlignDialogueActors();

    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);

    CLIENT->Request_DialogueCamera();

    if(_uiDialogue)
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

    EndDialogue();
}

void DialogueManager::EndDialogue()
{
    LOG_INFO("========대화 종료========");

    _currentDialogueKey.clear();
    _currentDialogueData = nullptr;
    _currentLineIndex = 0;
    _selectIndex = 0;
    _isChoiceVisible = false;
    _isActive = false;
    _waitInteractKeyRelease = true;

    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);

    CLIENT->Restore_PreviousCamera();

    if (_uiDialogue)
    {
        _uiDialogue->Close();
        _uiDialogue->Free();
        _uiDialogue.reset();
    }
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

    if (dynamic_pointer_cast<NPC_Base>(_dialogueNpcObject) != nullptr)
        FaceDialogueTarget(_dialogueNpcObject, playerPos);
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
        return;
    }

    MoveToLine(choice.nextLineIndex);
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
    }

    Base::Free();
}
