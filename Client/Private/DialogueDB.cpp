#include "DialogueDB.h"
#include "DialogueTypes.h"
#include "Helper_String.h"
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

NS_BEGIN(Client)

const DialogueData* DialogueDB::FindDialogueData(const string& dialogueKey) const
{
    const auto iter = _dialogueMap.find(dialogueKey);

    if (iter == _dialogueMap.end())
        return nullptr;

    return &iter->second;
}

void DialogueDB::LoadFromFile(const string& filePath)
{
    ifstream file(filePath);
    if (!file.is_open())
        return;

    json jsonData{};
    file >> jsonData;

    _dialogueMap.clear();

    if (!jsonData.contains("dialogues") || !jsonData["dialogues"].is_array())
        return;

    for (const auto& dialogueJson : jsonData["dialogues"])
    {
        DialogueData dialogueData{};
        dialogueData.dialogueKey = dialogueJson.value("dialogueKey", "");

        if (dialogueData.dialogueKey.empty())
            continue;

        if (dialogueJson.contains("lines") && dialogueJson["lines"].is_array())
        {
            for (const auto& lineJson : dialogueJson["lines"])
            {
                DialogueLine line{};
                line.lineIndex = lineJson.value("lineIndex", 0);
                line.text = String::ToWString(lineJson.value("text", ""));
                line.cameraPresetId = lineJson.value("cameraPresetId", "");
                line.nextLineIndex = lineJson.value("nextLineIndex", -1);

                if (lineJson.contains("choices") && lineJson["choices"].is_array())
                {
                    for (const auto& choiceJson : lineJson["choices"])
                    {
                        DialogueChoice choice{};
                        choice.text = choiceJson.value("text", "");
                        choice.action = lineJson.value("action", "");
                        choice.nextLineIndex = choiceJson.value("nextLineIndex", -1);
                        line.choices.push_back(move(choice));
                    }
                }

                dialogueData.lines.push_back(move(line));
            }
        }

        _dialogueMap[dialogueData.dialogueKey] = move(dialogueData);
    }
}

void DialogueDB::Clear()
{
    _dialogueMap.clear();
}

Unique<DialogueDB> DialogueDB::Create()
{
    return make_unique<DialogueDB>();
}

void DialogueDB::Free()
{
    Clear();
    Base::Free();
}

NS_END
