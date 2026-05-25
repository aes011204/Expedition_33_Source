#include "UI_CrossHair.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_CrossHair::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();

    return S_OK;
}

void UI_CrossHair::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    if (!Is_Open())
        return;


}

void UI_CrossHair::Open()
{
    const bool wasOpen = Is_Open();

    UI_RuntimePanelBase::Open();

    Apply_Color();

    if (!wasOpen && _crossHairImage)
        _crossHairImage->Play_UIAnimation(L"Ready_CrossAir", true); // 처음 열릴 때만 Ready_CrossAir 연출을 다시 시작한다.
}

void UI_CrossHair::Close()
{
    _isHit = false;
    Apply_Color();

    UI_RuntimePanelBase::Close();
}

void UI_CrossHair::Set_HitState(bool isHit)
{
    _isHit = isHit;
    Apply_Color();
}

HRESULT UI_CrossHair::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_CrossAir_Path;
    desc.animationPath = Constants::UI_Anim::UI_CrossAirAnim_Path;
    desc.prototypeTagPrefix = L"BattleCrossHair";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_CrossAir");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_CrossHair::Bind_UI()
{
    Shared<UI_Base> crossHairBase = Find_UI(_uiByName, L"Image_CrossAir");
    Shared<UI_Base> outlineBase = Find_UI(_uiByName, L"Image_CrossAir_Outline");

    CHECK_NULL(crossHairBase, E_FAIL);

    _crossHairImage = dynamic_pointer_cast<UI_Image>(crossHairBase);
    if (outlineBase)
        _crossHairOutlineImage = dynamic_pointer_cast<UI_Image>(outlineBase);

    CHECK_NULL(_crossHairImage, E_FAIL);

    Apply_Color();
    return S_OK;
}

void UI_CrossHair::Apply_Color()
{
    if (_crossHairImage)
        _crossHairImage->Set_Color(_isHit ? _hitColor : _normalColor); 

    if (_crossHairOutlineImage)
        _crossHairOutlineImage->Set_Color(_isHit ? _outlineHitColor : _outlineNormalColor);
}

Unique<UI_CrossHair> UI_CrossHair::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_CrossHair>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_CrossHair");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_CrossHair::Free()
{
    if (_root)
        _root->Close();

    _crossHairImage.reset();
    _crossHairOutlineImage.reset();
    _root.reset();
    _uiByName.clear();

    Base::Free();
}
