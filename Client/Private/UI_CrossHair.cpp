#include "UI_CrossHair.h"
#include "ClientInstance.h"
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

    _freeAimShotHitHandle = CLIENT->OnFreeAimShotHit.Add([this]()
        {
            Handle_FreeAimShot();
        });

    Close();

    return S_OK;
}

void UI_CrossHair::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    Update_HitPulse(timeDelta);
}

void UI_CrossHair::Open()
{
    const bool wasOpen = Is_Open();

    UI_RuntimePanelBase::Open();

    if (_outlineRoot)
        _outlineRoot->Open();

    if (!wasOpen)
    {
        _isHit = false;
        _hasHitState = false;
        Reset_HitPulse();

        Play_CrossAirAnimation(L"Ready_CrossAir");
    }
}

void UI_CrossHair::Close()
{
    _isHit = false;
    _hasHitState = false;
    Reset_HitPulse();

    UI_RuntimePanelBase::Close();

    if (_outlineRoot)
        _outlineRoot->Close();
}

void UI_CrossHair::Set_HitState(bool isHit)
{
    if (!_hasHitState)
    {
        _hasHitState = true;
        _isHit = isHit;

        if (_isHit)
            Play_CrossAirAnimation(L"Hover_Monster_CrossAir");

        return;
    }

    if (_isHit == isHit)
        return;

    _isHit = isHit;

    if (_isHit)
        Play_CrossAirAnimation(L"Hover_Monster_CrossAir");
    else
        Play_CrossAirAnimation(L"Hover_Cancel_Monster_CrossAir");
}

HRESULT UI_CrossHair::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_CrossAir_Path;
    desc.animationPaths = {
        Constants::UI_Anim::UI_CrossAirReadyAnim_Path,
        Constants::UI_Anim::UI_CrossAirHoverMonsterAnim_Path,
        Constants::UI_Anim::UI_CrossAirHoverCancelMonsterAnim_Path,
    };
    desc.prototypeTagPrefix = L"BattleCrossHair";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_CrossAir");
    CHECK_NULL(_root, E_FAIL);

    _outlineRoot = Find_UI(_uiByName, L"Panel_CrossAir_Outline");
    CHECK_NULL(_outlineRoot, E_FAIL);

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

    return S_OK;
}

void UI_CrossHair::Play_CrossAirAnimation(const wstring& animationName)
{
    if (_crossHairImage)
        _crossHairImage->Play_UIAnimation(animationName, true);

    if (_crossHairOutlineImage)
        _crossHairOutlineImage->Play_UIAnimation(animationName, true);
}

void UI_CrossHair::Handle_FreeAimShot()
{
    if (!Is_Open())
        return;

    Start_HitPulse();
}

void UI_CrossHair::Start_HitPulse()
{
    _hitPulseElapsed = 0.f;
    _isHitPulseActive = true;

    Apply_HitPulseScale(kHitPulseMinScale);
}

void UI_CrossHair::Update_HitPulse(float timeDelta)
{
    if (!_isHitPulseActive)
        return;

    _hitPulseElapsed += timeDelta;

    const float t = min(max(_hitPulseElapsed / kHitPulseDuration, 0.f), 1.f);
    const float scale = kHitPulseMinScale + (1.f - kHitPulseMinScale) * t;

    Apply_HitPulseScale(scale);

    if (t >= 1.f)
        Reset_HitPulse();
}

void UI_CrossHair::Apply_HitPulseScale(float scale)
{
    const Vec2 pulseScale{ scale, scale };

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Scale(pulseScale);

    if (_outlineRoot && _outlineRoot->Get_Transform())
        _outlineRoot->Get_Transform()->Set_Scale(pulseScale);
}

void UI_CrossHair::Reset_HitPulse()
{
    _hitPulseElapsed = 0.f;
    _isHitPulseActive = false;

    Apply_HitPulseScale(1.f);
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
    if (_freeAimShotHitHandle.IsValid())
    {
        CLIENT->OnFreeAimShotHit.Remove(_freeAimShotHitHandle);
        _freeAimShotHitHandle.Reset();
    }

    Reset_HitPulse();

    if (_outlineRoot)
        _outlineRoot->Close();

    if (_root)
        _root->Close();

    _crossHairImage.reset();
    _crossHairOutlineImage.reset();
    _outlineRoot.reset();
    _root.reset();
    _uiByName.clear();

    Base::Free();
}
