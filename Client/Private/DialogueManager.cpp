#include "DialogueManager.h"
#include <DialogueTypes.h>
#include "ClientInstance.h"

DialogueManager::DialogueManager() = default;

void DialogueManager::StartDialogue(const string& dialogueKey)
{
    _currentDialogueKey = dialogueKey;
    _currentDialogueData = CLIENT->FindDialogueData(dialogueKey);
    _currentLineIndex = 0;
    _selectIndex = 0;

    if (_currentDialogueData == nullptr || _currentDialogueData->lines.empty())
        return;

    ApplyCurrentLine();
}

void DialogueManager::ContinueDialogue()
{
    if (_currentDialogueData == nullptr)
        return;

    const DialogueLine* currentLine = GetCurrentLine();

    if (currentLine == nullptr)
        return;

    if (!currentLine->choices.empty())
        return;

    if (currentLine->nextLineIndex >= 0)
        MoveToLine(currentLine->nextLineIndex);
}

void DialogueManager::EndDialogue()
{
    _currentDialogueKey.clear();
    _currentDialogueData = nullptr;
    _currentLineIndex = 0;
    _selectIndex = 0;
}

void DialogueManager::Update()
{
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

    if (GAME->KeyDown(KEY_TYPE::DOWN) && _selectIndex + 1 < static_cast<int>(currentLine->choices.size()))
        ++_selectIndex;

    if (GAME->KeyDown(KEY_TYPE::UP) && _selectIndex > 0)
        --_selectIndex;

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
        return nullptr;

    if (_currentLineIndex < 0 || _currentLineIndex >= static_cast<int>(_currentDialogueData->lines.size()))
        return nullptr;

    return &_currentDialogueData->lines[_currentLineIndex];
}

void DialogueManager::MoveToLine(int lineIndex)
{
    if (_currentDialogueData == nullptr)
        return;

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
        return;

    LOG_INFO("Dialogue Line : {}", String::ToString(currentLine->text));
}

Unique<DialogueManager> DialogueManager::Create()
{
    return make_unique<DialogueManager>();
}

void DialogueManager::Free()
{
    Base::Free();
}
