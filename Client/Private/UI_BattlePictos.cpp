#include "UI_BattlePictos.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kActivationAnimName[] = L"Battle_Pictos_Activation";
    constexpr wchar_t kDeactivationAnimName[] = L"Battle_Pictos_Deactivation";

    constexpr float kVisibleDurationSec = 1.1f;
    constexpr float kDeactivationDurationSec = 15.f / 45.f;
    constexpr float kStackStepY = 28.f;
}

HRESULT UI_BattlePictos::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Set_EditorPositionDrivenByCode(true);
    Close_Immediately();

    return S_OK;
}

void UI_BattlePictos::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    _elapsed += max(0.f, timeDelta);

    if (!_isDeactivating && _elapsed >= kVisibleDurationSec)
    {
        Begin_Deactivation();
        return;
    }

    if (_isDeactivating && _elapsed >= kDeactivationDurationSec)
        Close_Immediately();
}

void UI_BattlePictos::Play_Activation(const ActivationDesc& desc)
{
    _heroId = desc.heroId;
    _stackIndex = desc.stackIndex;
    _elapsed = 0.f;
    _isDeactivating = false;

    Reset_VisualState();

    if (_labelText)
        _labelText->Set_Text(desc.displayName);

    UI_RuntimePanelBase::Open();

    if (_root && _root->Get_Transform())
    {
        Vec2 position = desc.anchorPosition + Vec2{ 0.f, -kStackStepY * static_cast<float>(desc.stackIndex) };
        Apply_EditorOffsets(position);
        _root->Get_Transform()->Set_Position(position);
    }

    Play_TargetAnimation(kActivationAnimName);
}

void UI_BattlePictos::Close()
{
    Close_Immediately();
}

void UI_BattlePictos::Close_Immediately()
{
    Stop_TargetAnimations();

    _heroId = HeroId::END;
    _stackIndex = 0;
    _elapsed = 0.f;
    _isDeactivating = false;

    UI_RuntimePanelBase::Close();
}

HRESULT UI_BattlePictos::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Battle_Activation_Pictos_Path;
    desc.prototypeTagPrefix = L"BattlePictos";
    desc.outUIByName = &_uiByName;

    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePictosActivationAnim_Path);
    desc.animationPaths.push_back(Constants::UI_Anim::UI_BattlePictosDeactivationAnim_Path);

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Battle_Activation_Pictos");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_BattlePictos::Bind_UI()
{
    _labelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Battle_Activation_Pictos_Label");
    _stackXText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Battle_Activation_Pictos_Stack_X");
    _stackValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_Battle_Activation_Pictos_Stack_Value");

    CHECK_NULL(_labelText, E_FAIL);
    CHECK_NULL(_stackXText, E_FAIL);
    CHECK_NULL(_stackValueText, E_FAIL);

    return S_OK;
}

void UI_BattlePictos::Reset_VisualState()
{
    Stop_TargetAnimations();

    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Open();
    }

    if (_stackXText)
        _stackXText->Open();

    if (_stackValueText)
    {
        _stackValueText->Open();
        _stackValueText->Set_Text(L"1");
    }

    if (_root)
        _root->Set_AlphaMultiplier(1.f);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Scale(Vec2{ 1.f, 1.f });
}

void UI_BattlePictos::Begin_Deactivation()
{
    _isDeactivating = true;
    _elapsed = 0.f;

    Play_TargetAnimation(kDeactivationAnimName);
}

void UI_BattlePictos::Play_TargetAnimation(const wchar_t* animationName)
{
    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui == nullptr)
            continue;

        if (!ui->Play_UIAnimation(animationName, true))
            ui->Stop_UIAnimation();
    }
}

void UI_BattlePictos::Stop_TargetAnimations()
{
    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Stop_UIAnimation();
    }
}

Unique<UI_BattlePictos> UI_BattlePictos::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattlePictos>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattlePictos");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattlePictos::Free()
{
    Close_Immediately();

    _labelText.reset();
    _stackXText.reset();
    _stackValueText.reset();

    UI_RuntimePanelBase::Free();
}
