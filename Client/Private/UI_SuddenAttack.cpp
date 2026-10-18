#include "UI_SuddenAttack.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_SuddenAttack::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_SuddenAttack::Update(float timeDelta)
{
    if (!Is_Open() || !_isPlaying)
        return;

    _elapsed += timeDelta;

    if (_elapsed >= _lifeTime)
        Close();
}

void UI_SuddenAttack::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_SuddenAttack::Close()
{
    _isPlaying = false;
    _elapsed = 0.f;

    if (_root)
        _root->Stop_UIAnimation();

    UI_RuntimePanelBase::Close();
}

void UI_SuddenAttack::Play_SuddenAttack()
{
    if (_root == nullptr)
        return;

    Close();

    Open();

    _isPlaying = true;
    _elapsed = 0.f;

    _root->Play_UIAnimation(L"FirstStrike", true);
}

HRESULT UI_SuddenAttack::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_FirstStrike_Path;
    desc.animationPath = Constants::UI_Anim::UI_FirstStrikeAnim_Path;
    desc.prototypeTagPrefix = L"SuddenAttack";
    desc.forceRenderGroup = true;
    desc.renderGroupOverride = UI_RenderGroup::Overlay;
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    return S_OK;
}

HRESULT UI_SuddenAttack::Bind_UI()
{
    _root = Find_UI(_uiByName, L"Panel_FirstStrike");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

Unique<UI_SuddenAttack> UI_SuddenAttack::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_SuddenAttack>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_SuddenAttack");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_SuddenAttack::Free()
{
    UI_RuntimePanelBase::Free();
}
