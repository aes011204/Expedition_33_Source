#pragma once

NS_BEGIN(Client)

enum class DialogueActorRole : uint8
{
    Player,
    Npc
};

struct DialogueChoice
{
    string text;
    string action;
    int nextLineIndex = -1;
};

struct DialogueLine
{
    int lineIndex = 0;
    wstring text;
    string cameraPresetId;
    DialogueActorRole speakerRole = DialogueActorRole::Npc;
    vector<DialogueChoice> choices;
    int nextLineIndex = -1;
};

struct DialogueData
{
    string dialogueKey;
    vector<DialogueLine> lines;
};

struct DialogueRequest
{
    Shared<GameObject> speaker;
    string speakerPrefabName;
    string dialogueKey;
};

NS_END
