#pragma once

NS_BEGIN(Client)

struct DialogueChoice
{
    string text;
    string action;
    int nextLineIndex;
};

struct DialogueLine
{
    int lineIndex;
    wstring text;
    string cameraPresetId;
    vector<DialogueChoice> choices;
    int nextLineIndex;
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
