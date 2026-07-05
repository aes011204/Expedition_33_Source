#include "UI_Burn.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_Burn::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Set_FloatingMotion(1.05f, 72.f);
    Set_StackStep(-120.f, 22.f);
    Set_WorldAnchorOffset(Vec3(0.f, 0.f, 0.f));
    Set_ScreenBaseOffset(Vec2(0.f, -8.f));
    Set_ScreenClamp(true, Vec2(120.f, 130.f));
    Set_WorldAnchorRandomYRange(-0.12f, 0.18f);
    Set_AnchorMode(AnchorMode::FollowWorld);

    Close();
    return S_OK;
}

void UI_Burn::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_Burn::Close()
{
    UI_BattleFloatingTextBase::Close();

    if (_burnRoot)
    {
        _burnRoot->Close();
        _burnRoot->Clear_Parent();
    }
}

void UI_Burn::Show_Burn(const BattleChar& target, uint32 stackIndex)
{
    Close();

    if (_burnRoot == nullptr)
        return;

    _root = _burnRoot;

    if (!Begin_FloatingText(target, stackIndex))
        Close();
}

void UI_Burn::Update(float timeDelta)
{
    UI_BattleFloatingTextBase::Update(timeDelta);
}

HRESULT UI_Burn::Load_UI(uint32 levelIndex)
{
    umap<wstring, Shared<UI_Base>> burnUIByName{};

    UI_RuntimeLoader::UI_LoadDesc burnDesc{};
    burnDesc.levelIndex = levelIndex;
    burnDesc.jsonPath = Constants::UI::UI_Burn_Path;
    burnDesc.prototypeTagPrefix = L"BurnStatus";
    burnDesc.outUIByName = &burnUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(burnDesc), E_FAIL);

    _uiByName.clear();
    _uiByName.insert(burnUIByName.begin(), burnUIByName.end());

    return S_OK;
}

HRESULT UI_Burn::Bind_UI()
{
    _burnRoot = Find_UI(_uiByName, L"Panel_Burn");

    CHECK_NULL(_burnRoot, E_FAIL);

    _root = _burnRoot;
    return S_OK;
}

Unique<UI_Burn> UI_Burn::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_Burn>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_Burn");
        return nullptr;
    }

    return instance;
}

void UI_Burn::Free()
{
    _burnRoot.reset();
    UI_BattleFloatingTextBase::Free();
}
