#include "UI_Slow.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_Slow::Initialize(uint32 levelIndex)
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

void UI_Slow::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_Slow::Close()
{
    UI_BattleFloatingTextBase::Close();

    if (_slowRoot)
    {
        _slowRoot->Close();
        _slowRoot->Clear_Parent();
    }
}

void UI_Slow::Show_Slow(const BattleChar& target, uint32 stackIndex)
{
    Close();

    if (_slowRoot == nullptr)
        return;

    _root = _slowRoot;

    if (!Begin_FloatingText(target, stackIndex))
        Close();
}

void UI_Slow::Update(float timeDelta)
{
    UI_BattleFloatingTextBase::Update(timeDelta);
}

HRESULT UI_Slow::Load_UI(uint32 levelIndex)
{
    umap<wstring, Shared<UI_Base>> slowUIByName{};

    UI_RuntimeLoader::UI_LoadDesc slowDesc{};
    slowDesc.levelIndex = levelIndex;
    slowDesc.jsonPath = Constants::UI::UI_Slow_Path;
    slowDesc.prototypeTagPrefix = L"SlowStatus";
    slowDesc.outUIByName = &slowUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(slowDesc), E_FAIL);

    _uiByName.clear();
    _uiByName.insert(slowUIByName.begin(), slowUIByName.end());

    return S_OK;
}

HRESULT UI_Slow::Bind_UI()
{
    _slowRoot = Find_UI(_uiByName, L"Panel_Slow");

    CHECK_NULL(_slowRoot, E_FAIL);

    _root = _slowRoot;
    return S_OK;
}

Unique<UI_Slow> UI_Slow::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_Slow>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_Slow");
        return nullptr;
    }

    return instance;
}

void UI_Slow::Free()
{
    _slowRoot.reset();
    UI_BattleFloatingTextBase::Free();
}
