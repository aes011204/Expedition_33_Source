#include "UI_BattleJumpReaction.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kJumpTextIntroAnimName[] = L"Jump_Text_Intro";
    constexpr wchar_t kJumpCounterIntroAnimName[] = L"JumpCounter_Intro";
    constexpr wchar_t kJumpCounterSuccessAnimName[] = L"JumpCounter_Success";

    constexpr float kJumpTextDurationSec = 60.f / 30.f;
    constexpr float kJumpCounterSuccessDurationSec = 60.f / 40.f;
}

HRESULT UI_BattleJumpReaction::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Set_EditorPositionDrivenByCode(true);
    Close_Immediately();

    return S_OK;
}

void UI_BattleJumpReaction::Update(float timeDelta)
{
    const float safeDelta = max(0.f, timeDelta);

    if (_isJumpTextOpen)
    {
        _jumpTextElapsed += safeDelta;

        if (_jumpTextElapsed >= kJumpTextDurationSec)
        {
            Stop_TargetAnimations(_jumpUIByName);
            _isJumpTextOpen = false;
            _jumpTextElapsed = 0.f;

            if (_jumpRoot)
                _jumpRoot->Close();
        }
    }

    if (_isJumpCounterSuccessPlaying)
    {
        _jumpCounterElapsed += safeDelta;

        if (_jumpCounterElapsed >= kJumpCounterSuccessDurationSec)
            Close_JumpCounter();
    }

    Close_IfAllClosed();
}

void UI_BattleJumpReaction::Show_JumpText(const PromptDesc& desc)
{
    if (_jumpRoot == nullptr)
        return;

    _isOpen = true;
    _isJumpTextOpen = true;
    _jumpTextElapsed = 0.f;

    _jumpRoot->Open();
    Set_RootPosition(_jumpRoot, desc.screenPosition); // Move jump text before playing its intro animation.
    Play_TargetAnimation(_jumpUIByName, kJumpTextIntroAnimName);
}

void UI_BattleJumpReaction::Show_JumpCounterIntro(const PromptDesc& desc)
{
    UNREFERENCED_PARAMETER(desc);

    if (_jumpCounterRoot == nullptr)
        return;

    _isOpen = true;
    _isJumpCounterOpen = true;
    _isJumpCounterSuccessPlaying = false;
    _jumpCounterElapsed = 0.f;

    _jumpCounterRoot->Open();
    Play_TargetAnimation(_jumpCounterUIByName, kJumpCounterIntroAnimName);
}

void UI_BattleJumpReaction::Play_JumpCounterSuccess(const PromptDesc& desc)
{
    UNREFERENCED_PARAMETER(desc);

    if (_jumpCounterRoot == nullptr)
        return;

    _isOpen = true;
    _isJumpCounterOpen = true;
    _isJumpCounterSuccessPlaying = true;
    _jumpCounterElapsed = 0.f;

    _jumpCounterRoot->Open();
    Play_TargetAnimation(_jumpCounterUIByName, kJumpCounterSuccessAnimName);
}

void UI_BattleJumpReaction::Close_JumpCounter()
{
    Stop_TargetAnimations(_jumpCounterUIByName);

    _isJumpCounterOpen = false;
    _isJumpCounterSuccessPlaying = false;
    _jumpCounterElapsed = 0.f;

    if (_jumpCounterRoot)
        _jumpCounterRoot->Close();

    Close_IfAllClosed();
}

void UI_BattleJumpReaction::Close()
{
    Close_Immediately();
}

void UI_BattleJumpReaction::Close_Immediately()
{
    Stop_TargetAnimations(_jumpUIByName);
    Stop_TargetAnimations(_jumpCounterUIByName);

    _isOpen = false;
    _isJumpTextOpen = false;
    _isJumpCounterOpen = false;
    _isJumpCounterSuccessPlaying = false;
    _jumpTextElapsed = 0.f;
    _jumpCounterElapsed = 0.f;

    if (_jumpRoot)
        _jumpRoot->Close();

    if (_jumpCounterRoot)
        _jumpCounterRoot->Close();
}

HRESULT UI_BattleJumpReaction::Load_UI(uint32 levelIndex)
{
    {
        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = Constants::UI::UI_Jump_Path;
        desc.prototypeTagPrefix = L"BattleJump";
        desc.outUIByName = &_jumpUIByName;

        desc.animationPaths.push_back(Constants::UI_Anim::UI_JumpTextIntroAnim_Path);

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        _jumpRoot = Find_UI(_jumpUIByName, L"Panel_Jump");
        CHECK_NULL(_jumpRoot, E_FAIL);

        _uiByName.insert(_jumpUIByName.begin(), _jumpUIByName.end());
    }

    {
        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = Constants::UI::UI_JumpCounter_Path;
        desc.prototypeTagPrefix = L"BattleJumpCounter";
        desc.outUIByName = &_jumpCounterUIByName;

        desc.animationPaths.push_back(Constants::UI_Anim::UI_JumpCounterIntroAnim_Path);
        desc.animationPaths.push_back(Constants::UI_Anim::UI_JumpCounterSuccessAnim_Path);

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        _jumpCounterRoot = Find_UI(_jumpCounterUIByName, L"Image_Jump_Counter_Left_Mouse");
        CHECK_NULL(_jumpCounterRoot, E_FAIL);

        _uiByName.insert(_jumpCounterUIByName.begin(), _jumpCounterUIByName.end());
    }

    _root = _jumpRoot;

    return S_OK;
}

HRESULT UI_BattleJumpReaction::Bind_UI()
{
    CHECK_NULL(_jumpRoot, E_FAIL);
    CHECK_NULL(_jumpCounterRoot, E_FAIL);

    return S_OK;
}

void UI_BattleJumpReaction::Close_IfAllClosed()
{
    _isOpen = _isJumpTextOpen || _isJumpCounterOpen;
}

void UI_BattleJumpReaction::Set_RootPosition(const Shared<Engine::UI_Base>& root, const Vec2& position) const
{
    if (root == nullptr || root->Get_Transform() == nullptr)
        return;

    Vec2 finalPosition = position;
    Apply_EditorOffsets(finalPosition);
    root->Get_Transform()->Set_Position(finalPosition);
}

void UI_BattleJumpReaction::Play_TargetAnimation(
    const umap<wstring, Shared<Engine::UI_Base>>& uiByName,
    const wchar_t* animationName)
{
    for (auto& [name, ui] : uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui == nullptr)
            continue;

        if (!ui->Play_UIAnimation(animationName, true))
            ui->Stop_UIAnimation();
    }
}

void UI_BattleJumpReaction::Stop_TargetAnimations(const umap<wstring, Shared<Engine::UI_Base>>& uiByName)
{
    for (auto& [name, ui] : uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Stop_UIAnimation();
    }
}

Unique<UI_BattleJumpReaction> UI_BattleJumpReaction::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleJumpReaction>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleJumpReaction");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleJumpReaction::Free()
{
    Close_Immediately();

    _jumpUIByName.clear();
    _jumpCounterUIByName.clear();
    _uiByName.clear();
    _jumpRoot.reset();
    _jumpCounterRoot.reset();
    _root.reset();

    UI_RuntimePanelBase::Free();
}
