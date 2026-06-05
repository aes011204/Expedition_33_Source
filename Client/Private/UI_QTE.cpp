#include "UI_QTE.h"

#include "Client_Constants.h"
#include "GameInstance.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_QTE::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);

    Close();

    return S_OK;
}

void UI_QTE::Update(float timeDelta)
{
    if (!Is_Open() || _root == nullptr)
        return;

    // QTE 시작
    if (_stage == QTE_Stage::Intro)
    {
        _elapsed += timeDelta;

        if (_elapsed >= _request.introDuration)
            Begin_Progress();

        return;
    }

    // QTE 진행 중
    if (_stage == QTE_Stage::Progress)
    {
        _elapsed += timeDelta;

        if (GAME->KeyDown(KEY_TYPE::SPACE))
        {
            const float safeDuration = max(_request.progressDuration, 0.0001f);
            const float ratio = clamp(_elapsed / safeDuration, 0.f, 1.f);

            // Progress 진행률 기준으로 노란 구간 안이면 Perfect, 그 전이면 Normal.
            const bool isPerfect =
                ratio >= _request.perfectStartRatio &&
                ratio <= _request.perfectEndRatio;

            Resolve(isPerfect ? BattleQTEResult::SuccessPerfect : BattleQTEResult::SuccessNormal);
            return;
        }

        if (_elapsed >= _request.progressDuration)
            Resolve(BattleQTEResult::Fail);

        return;
    }

    // 결과 : 실패, 노말, 퍼펙트 판단
    if (_stage == QTE_Stage::Result)
    {
        _resultElapsed += timeDelta;

        if (_resultElapsed >= _resultDuration)
            Close();

        return;
    }
}

void UI_QTE::Open()
{
    UI_RuntimePanelBase::Open();
}

void UI_QTE::Close()
{
    _stage = QTE_Stage::Closed;
    _request = {};
    _pendingResult = BattleQTEResult::None;
    _hasPendingResult = false;
    _elapsed = 0.f;
    _resultElapsed = 0.f;

    Stop_QTEAnimations();
    Close_AllQTEVisuals();

    UI_RuntimePanelBase::Close();
}

void UI_QTE::Play_QTE(const BattleQTERequest& request)
{
    _request = request;
    _stage = QTE_Stage::Intro;
    _pendingResult = BattleQTEResult::None;
    _hasPendingResult = false;
    _elapsed = 0.f;
    _resultElapsed = 0.f;

    UI_RuntimePanelBase::Open();
    Open_AllQTEVisuals();
    Close_ResultVisuals();

    Apply_PresetPlacement(request);

    // Intro는 targetName으로 바인딩된 모든 UI에 같은 이름으로 재생해야 한다.
    GAME->Play_Sound(Constants::Sound::BattleQTEStart, SoundChannel::UI, 1.f);
    Play_QTEAnimation(L"QTE_Intro");
}

void UI_QTE::Apply_ExternalResult(BattleQTEResult  result)
{
    if (!Is_Open())
        return;

    if (result == BattleQTEResult::None || result == BattleQTEResult::END)
        return;

    _pendingResult = BattleQTEResult::None;
    _hasPendingResult = false;

    _stage = QTE_Stage::Result;
    _elapsed = 0.f;
    _resultElapsed = 0.f;

    // UI 내부 입력이 아니라 ANS_QTE 종료/턴 종료에서 확정된 결과도 같은 결과 애니메이션으로 표시
    Play_ResultAnimation(result);
}

bool UI_QTE::Try_ConsumeResult(BattleQTEResult& outResult)
{
    if (!_hasPendingResult)
        return false;

    outResult = _pendingResult;
    _pendingResult = BattleQTEResult::None;
    _hasPendingResult = false;

    return true;
}

HRESULT UI_QTE::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_QTE_Path;
    desc.animationPaths =
    {
        Constants::UI_Anim::UI_QTEIntroAnim_Path,
        Constants::UI_Anim::UI_QTEProgressAnim_Path,
        Constants::UI_Anim::UI_QTEFailAnim_Path,
        Constants::UI_Anim::UI_QTESuccessNormalAnim_Path,
        Constants::UI_Anim::UI_QTESuccessPerfectAnim_Path,
    };
    desc.prototypeTagPrefix = L"QTE";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = UI_Utils::Find_UI(_uiByName, L"Panel_QTE");
    CHECK_NULL(_root, E_FAIL);

    CHECK_FAILED(Bind_UIReferences(), E_FAIL);

    return S_OK;
}

HRESULT UI_QTE::Bind_UIReferences()
{
    _areaLineRoot = UI_Utils::Find_UI(_uiByName, L"Panel_QTE_Area_Line");
    _decoAngleRoot = UI_Utils::Find_UI(_uiByName, L"Panel_QTE_Deco_Angle");

    _keySpaceRoot = UI_Utils::Find_UI(_uiByName, L"Panel_Key_Space");
    _cursorProgressRoot = UI_Utils::Find_UI(_uiByName, L"Image_QTE_CursorProgress");

    _normalDecoRoot = UI_Utils::Find_UI(_uiByName, L"Panel_QTE_Normal_Deco");
    _perfectDecoRoot = UI_Utils::Find_UI(_uiByName, L"Panel_QTE_Perfect_Deco");
    _perfectTextRoot = UI_Utils::Find_UI(_uiByName, L"Panel_QTE_Perfect_Text");

    CHECK_NULL(_areaLineRoot, E_FAIL);
    CHECK_NULL(_decoAngleRoot, E_FAIL);
    CHECK_NULL(_keySpaceRoot, E_FAIL);
    CHECK_NULL(_cursorProgressRoot, E_FAIL);
    CHECK_NULL(_normalDecoRoot, E_FAIL);
    CHECK_NULL(_perfectDecoRoot, E_FAIL);
    CHECK_NULL(_perfectTextRoot, E_FAIL);

    return S_OK;
}

void UI_QTE::Reset_VisualState()
{
    Stop_QTEAnimations();
    Close_ResultVisuals();
}

void UI_QTE::Open_AllQTEVisuals()
{
    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Open();
    }
}

void UI_QTE::Close_AllQTEVisuals()
{
    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Close();
    }
}

void UI_QTE::Close_ResultVisuals()
{
    Set_UIOpen(_normalDecoRoot, false);
    Set_UIOpen(_perfectDecoRoot, false);
    Set_UIOpen(_perfectTextRoot, false);
}

void UI_QTE::Play_QTEAnimation(const wstring& animationName)
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

void UI_QTE::Stop_QTEAnimations()
{
    for (auto& [name, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

void UI_QTE::Set_UIOpen(const Shared<UI_Base>& ui, bool isOpen) const
{
    if (ui == nullptr)
        return;

    if (isOpen)
        ui->Open();
    else
        ui->Close();
}

void UI_QTE::Begin_Progress()
{
    _stage = QTE_Stage::Progress;
    _elapsed = 0.f;

    Open_AllQTEVisuals();
    Close_ResultVisuals();

    // Progress도 루트가 아니라 모든 바인딩 타겟에 재생 요청한다.
    Play_QTEAnimation(L"QTE_Progress");
}

void UI_QTE::Resolve(BattleQTEResult result)
{
    if (_stage != QTE_Stage::Progress)
        return;

    _pendingResult = result;
    _hasPendingResult = true;

    _stage = QTE_Stage::Result;
    _elapsed = 0.f;
    _resultElapsed = 0.f;

    // QTE 결과
    Play_ResultAnimation(result);
}

void UI_QTE::Play_ResultAnimation(BattleQTEResult result)
{
    if (_root == nullptr)
        return;

    Open_AllQTEVisuals();

    switch (result)
    {
    case BattleQTEResult::SuccessPerfect:
        Set_UIOpen(_perfectDecoRoot, true);
        Set_UIOpen(_perfectTextRoot, true);
        Set_UIOpen(_normalDecoRoot, false);
        GAME->Play_Sound(Constants::Sound::BattleQTESuccessPerfect, SoundChannel::UI, 1.f);
        Play_QTEAnimation(L"QTE_Success_Perfect");
        break;

    case BattleQTEResult::SuccessNormal:
        Set_UIOpen(_normalDecoRoot, true);
        Set_UIOpen(_perfectDecoRoot, false);
        Set_UIOpen(_perfectTextRoot, false);
        GAME->Play_Sound(Constants::Sound::BattleQTESuccessRegular, SoundChannel::UI, 1.f);
        Play_QTEAnimation(L"QTE_Success_Normal");
        break;

    case BattleQTEResult::Fail:
        Close_ResultVisuals();
        GAME->Play_Sound(Constants::Sound::BattleQTEMiss, SoundChannel::UI, 1.f);
        Play_QTEAnimation(L"QTE_Fail");
        break;

    default:
        break;
    }
}

void UI_QTE::Apply_PresetPlacement(const BattleQTERequest& request)
{
     if (_root == nullptr || _root->Get_Transform() == nullptr)
        return;

    auto rootTr = _root->Get_Transform();

    rootTr->Set_Position(request.screenPosition);
    rootTr->Set_Scale(Vec2(request.screenScale, request.screenScale));

    _root->Refresh_OpenBaseTransform();
}

Unique<UI_QTE> UI_QTE::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_QTE>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        MSG_BOX("Failed to Created : UI_QTE");
        return nullptr;
    }

    return instance;
}

void UI_QTE::Free()
{
    UI_RuntimePanelBase::Free();
}
