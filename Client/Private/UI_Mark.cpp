#include "UI_Mark.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_Mark::Initialize(uint32 levelIndex)
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

void UI_Mark::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_Mark::Close()
{
    UI_BattleFloatingTextBase::Close();

    if (_markRoot)
    {
        _markRoot->Close();
        _markRoot->Clear_Parent();
    }
}

void UI_Mark::Show_Mark(const BattleChar& target, uint32 stackIndex)
{
    Close();

    if (_markRoot == nullptr)
        return;

    _root = _markRoot;

    if (!Begin_FloatingText(target, stackIndex))
        Close();
}

void UI_Mark::Update(float timeDelta)
{
    UI_BattleFloatingTextBase::Update(timeDelta);
}

HRESULT UI_Mark::Load_UI(uint32 levelIndex)
{
    umap<wstring, Shared<UI_Base>> markUIByName{};

    UI_RuntimeLoader::UI_LoadDesc markDesc{};
    markDesc.levelIndex = levelIndex;
    markDesc.jsonPath = Constants::UI::UI_Mark_Path;
    markDesc.prototypeTagPrefix = L"MarkStatus";
    markDesc.outUIByName = &markUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(markDesc), E_FAIL);

    _uiByName.clear();
    _uiByName.insert(markUIByName.begin(), markUIByName.end());

    return S_OK;
}

HRESULT UI_Mark::Bind_UI()
{
    _markRoot = Find_UI(_uiByName, L"Panel_Mark");

    CHECK_NULL(_markRoot, E_FAIL);

    _root = _markRoot;
    return S_OK;
}

Unique<UI_Mark> UI_Mark::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_Mark>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_Mark");
        return nullptr;
    }

    return instance;
}

void UI_Mark::Free()
{
    _markRoot.reset();
    UI_BattleFloatingTextBase::Free();
}
