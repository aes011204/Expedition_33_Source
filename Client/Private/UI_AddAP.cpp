#include "UI_AddAP.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_AddAP::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Set_FloatingMotion(40.f / 60.f, 32.f);
    Set_StackStep(18.f, 20.f);
    Set_AnchorMode(AnchorMode::SnapshotScreen);

    Set_WorldAnchorOffset(Vec3(0.f, 0.f, 0.f));
    Set_ScreenBaseOffset(Vec2(0.f, -120.f));

    Set_WorldAnchorRandomYRange(-0.06f, 0.10f);

    Close();

    return S_OK;
}

void UI_AddAP::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_AddAP::Play_AddAP(const BattleChar& target, uint32 apAmount, uint32 stackIndex)
{
    Reset_APVisualState();
    Set_APText(apAmount);

    if (!Begin_FloatingText(target, stackIndex))
        return;

    Play_AddAPAnimation();
}

HRESULT UI_AddAP::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_BattleSystem_Add_AP_Path;
    desc.animationPath = Constants::UI_Anim::UI_AddAPAnim_Path;
    desc.prototypeTagPrefix = L"AddAP";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_BattleSystem_Add_AP");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_AddAP::Bind_UI()
{
    _apText = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Add_AP");
    _apTextBg = UI_Utils::Find_TypedUI<UI_Text>(_uiByName, L"Text_BattleSystem_Add_AP_BG");
    _apImage = UI_Utils::Find_TypedUI<UI_Image>(_uiByName, L"Image_BattleSystem_Add_AP");
    _apImageBg = UI_Utils::Find_TypedUI<UI_Image>(_uiByName, L"Image_BattleSystem_Add_AP_BG");

    CHECK_NULL(_apText, E_FAIL);
    CHECK_NULL(_apTextBg, E_FAIL);
    CHECK_NULL(_apImage, E_FAIL);
    CHECK_NULL(_apImageBg, E_FAIL);

    return S_OK;
}

void UI_AddAP::Set_APText(uint32 apAmount)
{
    const wstring text = to_wstring(apAmount);

    _apText->Set_Text(text);
    _apTextBg->Set_Text(text);
}

void UI_AddAP::Reset_APVisualState()
{
    Stop_AddAPAnimations();

    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Open();
    }

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Scale(Vec2(1.f, 1.f));

    if (_apImage)
    {
        _apImage->Set_Color(Vec4(115.f / 255.f, 140.f / 255.f, 242.f / 255.f, 1.f));

        if (_apImage->Get_Transform())
            _apImage->Get_Transform()->Set_Scale(Vec2(1.f, 1.f));
    }

    if (_apImageBg)
    {
        _apImageBg->Set_Color(Vec4(0.f, 0.f, 0.f, 1.f));

        if (_apImageBg->Get_Transform())
            _apImageBg->Get_Transform()->Set_Scale(Vec2(1.f, 1.f));
    }

    if (_apText)
    {
        _apText->Set_Color(Vec4(1.f, 1.f, 1.f, 1.f));

        if (_apText->Get_Transform())
            _apText->Get_Transform()->Set_Scale(Vec2(1.f, 1.f));
    }

    if (_apTextBg)
    {
        _apTextBg->Set_Color(Vec4(0.f, 0.f, 0.f, 1.f));

        if (_apTextBg->Get_Transform())
            _apTextBg->Get_Transform()->Set_Scale(Vec2(1.f, 1.f));
    }
}

void UI_AddAP::Play_AddAPAnimation()
{
    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui == nullptr)
            continue;

        if (!ui->Play_UIAnimation(L"Add_AP", true))
            ui->Stop_UIAnimation();
    }
}

void UI_AddAP::Stop_AddAPAnimations()
{
    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Stop_UIAnimation();
    }
}

Unique<UI_AddAP> UI_AddAP::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_AddAP>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        MSG_BOX("Failed to Created : UI_AddAP");
        return nullptr;
    }

    return instance;
}

void UI_AddAP::Free()
{
    UI_BattleFloatingTextBase::Free();
}
