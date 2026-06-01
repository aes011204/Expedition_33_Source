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

    _root->Update(timeDelta);

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

    if (_root)
        _root->Stop_UIAnimation();

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

    // Intro 애니메이션으로 시작
    if (_root)
        _root->Play_UIAnimation(L"QTE_Intro", true);
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

    return S_OK;
}

void UI_QTE::Begin_Progress()
{
    _stage = QTE_Stage::Progress;
    _elapsed = 0.f;

    if (_root)
        _root->Play_UIAnimation(L"QTE_Progress", true);
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

    switch (result)
    {
    case BattleQTEResult::SuccessPerfect:
        _root->Play_UIAnimation(L"QTE_Success_Perfect", true);
        break;

    case BattleQTEResult::SuccessNormal:
        _root->Play_UIAnimation(L"QTE_Success_Normal", true);
        break;

    case BattleQTEResult::Fail:
        _root->Play_UIAnimation(L"QTE_Fail", true);
        break;

    default:
        break;
    }
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
