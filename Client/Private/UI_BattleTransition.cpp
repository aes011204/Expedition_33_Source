#include "UI_BattleTransition.h"
#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_BattleTransition::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    // 처음에는 안보이게
    Close();

    return S_OK;
}

void UI_BattleTransition::Update(float timeDelta)
{
    if (!_isPlaying)
        return;

    if (_skipFirstUpdate)
    {
        _skipFirstUpdate = false;
        return;
    }

    _elapsed += min(timeDelta, 1.f / 30.f);

    const float holeTimer = min(_elapsed / _duration, 1.f);
    const float radius = _startRadius + (_endRadius - _startRadius) * holeTimer;

    _holeMaskImage->Set_HoleRadius(radius);

    if (holeTimer >= 1.f)
        Close();

}

void UI_BattleTransition::Play_BattleEnter()
{
    //CHECK_NULL(_root);
    //
    //_elapsed = 0.f;
    //_isPlaying = true;
    //_skipFirstUpdate = false;
    //
    //_holeMaskImage->Set_MaskMode(UI_Image::MaskMode::CircularHole);
    //_holeMaskImage->Set_HoleCenterUV(Vec2(0.5f, 0.5f));
    //_holeMaskImage->Set_HoleRadius(_startRadius);
    //_holeMaskImage->Set_HoleFeather(_holeFeather);
    //_holeMaskImage->Set_Color(Vec4(0.f, 0.f, 0.f, 1.f)); 
    //
    //UI_RuntimePanelBase::Open();
}

void UI_BattleTransition::Close()
{
    _isPlaying = false;
    _skipFirstUpdate = false;
    _elapsed = 0.f;

    UI_RuntimePanelBase::Close();
}

HRESULT UI_BattleTransition::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Battle_Fade_Path;
    desc.prototypeTagPrefix = L"BattleTransition";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    return S_OK;
}

HRESULT UI_BattleTransition::Bind_UI()
{
    _root = Find_UI(_uiByName, L"Panel_Battle_Fade");
    CHECK_NULL(_root, E_FAIL);

    Shared<UI_Base> holeBase = Find_UI(_uiByName, L"Image_Battle_HoleMask");
    CHECK_NULL(holeBase, E_FAIL);

    _holeMaskImage = dynamic_pointer_cast<UI_Image>(holeBase);
    CHECK_NULL(_holeMaskImage, E_FAIL);

    return S_OK;
}

Unique<UI_BattleTransition> UI_BattleTransition::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleTransition>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleTransition");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleTransition::Free()
{
    if (_root)
        _root->Close();

    _root.reset();
    _holeMaskImage.reset();
    _uiByName.clear();

    Base::Free();
}
