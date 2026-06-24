#include "DialogueManager.h"
#include <DialogueTypes.h>
#include "ClientInstance.h"
#include "DialogueCamera.h"
//#include "GameObject.h"
#include "Player_WorldBase.h"

DialogueManager::DialogueManager() = default;

void DialogueManager::StartDialogue(const DialogueRequest& request)
{
    if(_isActive || _waitInteractKeyRelease)
        return;

    const DialogueData* dialogueData = CLIENT->FindDialogueData(request.dialogueKey);

    if(dialogueData == nullptr || dialogueData->lines.empty())
        return;

    LOG_INFO("========대화 시작========");

    _currentDialogueKey = request.dialogueKey;
    _currentDialogueData = dialogueData;
    _currentLineIndex = 0;
    _selectIndex = 0;
    _isActive = true;
    _waitInteractKeyRelease = true;
    _dialogueNpcObject = request.speaker;
    _dialoguePlayerObject = static_pointer_cast<GameObject>(CLIENT->Get_ActiveHero());
     
    GAME->Set_GameplayInputMode(GameplayInputMode::UIOnly);

    CLIENT->Request_DialogueCamera();

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
    _isActive = false;
    _waitInteractKeyRelease = true;

    GAME->Set_GameplayInputMode(GameplayInputMode::Normal);
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

    if(!_isActive)
        return;

    const DialogueLine* currentLine = GetCurrentLine();

    if (currentLine == nullptr)
        return;

    if (!currentLine->choices.empty())
    {
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
        LOG_INFO("Current Select : {}", currentLine->choices[_selectIndex].text);

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

    const DialogueChoice& choice =  currentLine->choices[_selectIndex];

    if(choice.action == "OpenShop")
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
    {
        return nullptr;
    }

    if (_currentLineIndex < 0 || _currentLineIndex >= static_cast<int>(_currentDialogueData->lines.size()))
    {
        return nullptr;
    }

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

    Shared<DialogueCamera> dialogueCamera = dynamic_pointer_cast<DialogueCamera>(GAME->Get_CameraByType(ObjectType::DialogueCamera));

    if(dialogueCamera == nullptr)
        return;

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

    dialogueCamera->Set_CameraPresetId(currentLine->cameraPresetId);
    dialogueCamera->Set_SpeakerTransform(speakerObject->Get_Transform());
    dialogueCamera->Set_ListenerTransform(listenerObject->Get_Transform());

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
    Base::Free();
}
