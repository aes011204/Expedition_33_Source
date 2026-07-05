#include "UI_EventDialoguePlayer.h"

#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

HRESULT UI_EventDialoguePlayer::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;
    _isOpen = false;
    _autoCloseRemainingSec = 0.f;
    _hasAutoClose = false;

    return S_OK;
}

void UI_EventDialoguePlayer::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    if (!_hasAutoClose)
        return;

    _autoCloseRemainingSec -= max(0.f, timeDelta);
    if (_autoCloseRemainingSec <= 0.f)
        Close_Immediately();
}

bool UI_EventDialoguePlayer::Open_EventDialogue(const EventDialogueUIRequest& request)
{
    Close_Immediately();

    if (request.uiJsonPath.empty())
    {
        LOG_WARN("EventDialogueUI: _uiJsonPath is empty.");
        return false;
    }

    if (request.rootUIName.empty())
    {
        LOG_WARN("EventDialogueUI: _rootUIName is empty.");
        return false;
    }

    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = request.uiJsonPath;
    desc.animationPath = request.animationPath;
    desc.prototypeTagPrefix = L"EventDialogue";
    desc.outUIByName = &_uiByName;

    if (FAILED(UI_RuntimeLoader::Load_Document(desc)))
    {
        LOG_WARN("EventDialogueUI: failed to load UI document.");
        Release_LoadedDocument();
        return false;
    }

    _root = UI_Utils::Find_UI(_uiByName, request.rootUIName);
    if (_root == nullptr)
    {
        LOG_WARN("EventDialogueUI: failed to find root UI.");
        Release_LoadedDocument();
        return false;
    }

    _currentRootUIName = request.rootUIName;
    _autoCloseRemainingSec = max(0.f, request.autoCloseDelaySec);
    _hasAutoClose = _autoCloseRemainingSec > 0.f;

    UI_RuntimePanelBase::Open();
    return true;
}

void UI_EventDialoguePlayer::Close()
{
    Close_Immediately();
}

void UI_EventDialoguePlayer::Close_Immediately()
{
    if (_root)
        _root->Close();

    _isOpen = false;
    _autoCloseRemainingSec = 0.f;
    _hasAutoClose = false;

    Release_LoadedDocument();
}

void UI_EventDialoguePlayer::Release_LoadedDocument()
{
    _currentRootUIName.clear();
    _root.reset();
    UI_RuntimeLoader::Release_Document(_uiByName);
    Reset_EditorDesignPositionCache();
}

Unique<UI_EventDialoguePlayer> UI_EventDialoguePlayer::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_EventDialoguePlayer>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_EventDialoguePlayer");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_EventDialoguePlayer::Free()
{
    Release_LoadedDocument();
    UI_RuntimePanelBase::Free();
}
