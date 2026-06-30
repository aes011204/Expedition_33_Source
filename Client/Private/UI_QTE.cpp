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
    if (!Is_Open())
        return;

    const int32 inputSlotIndex = Find_FirstInputSlotIndex();

    for (uint32 slotIndex : _activeSlotOrder)
    {
        if (slotIndex >= _slots.size())
            continue;

        Update_Slot(_slots[slotIndex], timeDelta, static_cast<int32>(slotIndex) == inputSlotIndex);
    }

    Remove_InactiveSlotOrder();

    if (_activeSlotOrder.empty())
        UI_RuntimePanelBase::Close();
}

void UI_QTE::Open()
{
    _isOpen = true;
}

void UI_QTE::Close()
{
    _activeSlotOrder.clear();
    _pendingResults.clear();

    for (QTE_Slot& slot : _slots)
        Close_Slot(slot);

    UI_RuntimePanelBase::Close();
}

void UI_QTE::Play_QTE(const BattleQTERequest& request)
{
    int32 slotIndex = Find_FreeSlotIndex();
    if (slotIndex < 0)
        return;

    QTE_Slot& slot = _slots[slotIndex];
    slot.request = request;
    slot.stage = QTE_Stage::Intro;
    slot.elapsed = 0.f;
    slot.resultElapsed = 0.f;

    _activeSlotOrder.push_back(static_cast<uint32>(slotIndex));
    _isOpen = true;
    Open_AllQTEVisuals(slot);
    Close_ResultVisuals(slot);

    Apply_PresetPlacement(slot, request);

    GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_QTE_Start);
    Play_QTEAnimation(slot, L"QTE_Intro");
}

void UI_QTE::Apply_ExternalResult(BattleQTEResult  result)
{
    if (!Is_Open())
        return;

    if (result == BattleQTEResult::None || result == BattleQTEResult::END)
        return;

    for (uint32 slotIndex : _activeSlotOrder)
    {
        if (slotIndex >= _slots.size())
            continue;

        QTE_Slot& slot = _slots[slotIndex];
        if (slot.stage == QTE_Stage::Result)
            return;
    }

    const int32 inputSlotIndex = Find_FirstInputSlotIndex();
    if (inputSlotIndex < 0)
        return;

    QTE_Slot& slot = _slots[inputSlotIndex];
    slot.stage = QTE_Stage::Result;
    slot.elapsed = 0.f;
    slot.resultElapsed = 0.f;

    // UI 내부 입력이 아니라 ANS_QTE 종료/턴 종료에서 확정된 결과도 같은 결과 애니메이션으로 표시
    Play_ResultAnimation(slot, result);
}

bool UI_QTE::Try_ConsumeResult(BattleQTEResult& outResult)
{
    if (_pendingResults.empty())
        return false;

    outResult = _pendingResults.front();
    _pendingResults.erase(_pendingResults.begin());

    return true;
}

HRESULT UI_QTE::Load_UI(uint32 levelIndex)
{
    const vector<wstring> animationPaths =
    {
        Constants::UI_Anim::UI_QTEIntroAnim_Path,
        Constants::UI_Anim::UI_QTEProgressAnim_Path,
        Constants::UI_Anim::UI_QTEFailAnim_Path,
        Constants::UI_Anim::UI_QTESuccessNormalAnim_Path,
        Constants::UI_Anim::UI_QTESuccessPerfectAnim_Path,
    };

    for (uint32 slotIndex = 0; slotIndex < _slots.size(); ++slotIndex)
    {
        QTE_Slot& slot = _slots[slotIndex];

        UI_RuntimeLoader::UI_LoadDesc desc{};
        desc.levelIndex = levelIndex;
        desc.jsonPath = Constants::UI::UI_QTE_Path;
        desc.animationPaths = animationPaths;
        desc.prototypeTagPrefix = L"QTE" + to_wstring(slotIndex);
        desc.outUIByName = &slot.uiByName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

        slot.root = UI_Utils::Find_UI(slot.uiByName, L"Panel_QTE");
        CHECK_NULL(slot.root, E_FAIL);

        CHECK_FAILED(Bind_UIReferences(slot), E_FAIL);
    }

    _uiByName = _slots[0].uiByName;
    _root = _slots[0].root;

    return S_OK;
}

HRESULT UI_QTE::Bind_UIReferences(QTE_Slot& slot)
{
    slot.areaLineRoot = UI_Utils::Find_UI(slot.uiByName, L"Panel_QTE_Area_Line");
    slot.decoAngleRoot = UI_Utils::Find_UI(slot.uiByName, L"Panel_QTE_Deco_Angle");

    slot.keySpaceRoot = UI_Utils::Find_UI(slot.uiByName, L"Panel_Key_Space");
    slot.cursorProgressRoot = UI_Utils::Find_UI(slot.uiByName, L"Image_QTE_CursorProgress");

    slot.normalDecoRoot = UI_Utils::Find_UI(slot.uiByName, L"Panel_QTE_Normal_Deco");
    slot.perfectDecoRoot = UI_Utils::Find_UI(slot.uiByName, L"Panel_QTE_Perfect_Deco");
    slot.perfectTextRoot = UI_Utils::Find_UI(slot.uiByName, L"Panel_QTE_Perfect_Text");

    CHECK_NULL(slot.areaLineRoot, E_FAIL);
    CHECK_NULL(slot.decoAngleRoot, E_FAIL);
    CHECK_NULL(slot.keySpaceRoot, E_FAIL);
    CHECK_NULL(slot.cursorProgressRoot, E_FAIL);
    CHECK_NULL(slot.normalDecoRoot, E_FAIL);
    CHECK_NULL(slot.perfectDecoRoot, E_FAIL);
    CHECK_NULL(slot.perfectTextRoot, E_FAIL);

    return S_OK;
}

void UI_QTE::Reset_VisualState()
{
    for (QTE_Slot& slot : _slots)
    {
        Stop_QTEAnimations(slot);
        Close_ResultVisuals(slot);
    }
}

void UI_QTE::Open_AllQTEVisuals(QTE_Slot& slot)
{
    for (auto& [name, ui] : slot.uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Open();
    }
}

void UI_QTE::Close_AllQTEVisuals(QTE_Slot& slot)
{
    for (auto& [name, ui] : slot.uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui)
            ui->Close();
    }
}

void UI_QTE::Close_ResultVisuals(QTE_Slot& slot)
{
    Set_UIOpen(slot.normalDecoRoot, false);
    Set_UIOpen(slot.perfectDecoRoot, false);
    Set_UIOpen(slot.perfectTextRoot, false);
}

void UI_QTE::Play_QTEAnimation(QTE_Slot& slot, const wstring& animationName)
{
    for (auto& [name, ui] : slot.uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui == nullptr)
            continue;

        if (!ui->Play_UIAnimation(animationName, true))
            ui->Stop_UIAnimation();
    }
}

void UI_QTE::Stop_QTEAnimations(QTE_Slot& slot)
{
    for (auto& [name, ui] : slot.uiByName)
    {
        UNREFERENCED_PARAMETER(name);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

void UI_QTE::Close_Slot(QTE_Slot& slot)
{
    slot.stage = QTE_Stage::Closed;
    slot.request = {};
    slot.elapsed = 0.f;
    slot.resultElapsed = 0.f;

    Stop_QTEAnimations(slot);
    Close_AllQTEVisuals(slot);
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

void UI_QTE::Update_Slot(QTE_Slot& slot, float timeDelta, bool allowInput)
{
    if (slot.stage == QTE_Stage::Closed || slot.root == nullptr)
        return;

    if (slot.stage == QTE_Stage::Intro)
    {
        slot.elapsed += timeDelta;

        if (slot.elapsed >= slot.request.introDuration)
            Begin_Progress(slot);

        return;
    }

    if (slot.stage == QTE_Stage::Progress)
    {
        slot.elapsed += timeDelta;

        if (allowInput && GAME->KeyDown(KEY_TYPE::SPACE))
        {
            const float safeDuration = max(slot.request.progressDuration, 0.0001f);
            const float ratio = clamp(slot.elapsed / safeDuration, 0.f, 1.f);

            const bool isPerfect =
                ratio >= slot.request.perfectStartRatio &&
                ratio <= slot.request.perfectEndRatio;

            Resolve(slot, isPerfect ? BattleQTEResult::SuccessPerfect : BattleQTEResult::SuccessNormal);
            return;
        }

        if (allowInput && slot.elapsed >= slot.request.progressDuration)
            Resolve(slot, BattleQTEResult::Fail);

        return;
    }

    if (slot.stage == QTE_Stage::Result)
    {
        slot.resultElapsed += timeDelta;

        if (slot.resultElapsed >= _resultDuration)
            Close_Slot(slot);
    }
}

void UI_QTE::Begin_Progress(QTE_Slot& slot)
{
    slot.stage = QTE_Stage::Progress;
    slot.elapsed = 0.f;

    Open_AllQTEVisuals(slot);
    Close_ResultVisuals(slot);

    // Progress도 루트가 아니라 모든 바인딩 타겟에 재생 요청한다.
    Play_QTEAnimation(slot, L"QTE_Progress");
}

void UI_QTE::Resolve(QTE_Slot& slot, BattleQTEResult result)
{
    if (slot.stage != QTE_Stage::Progress)
        return;

    _pendingResults.push_back(result);

    slot.stage = QTE_Stage::Result;
    slot.elapsed = 0.f;
    slot.resultElapsed = 0.f;

    // QTE 결과
    Play_ResultAnimation(slot, result);
}

void UI_QTE::Play_ResultAnimation(QTE_Slot& slot, BattleQTEResult result)
{
    if (slot.root == nullptr)
        return;

    Open_AllQTEVisuals(slot);

    switch (result)
    {
    case BattleQTEResult::SuccessPerfect:
        Set_UIOpen(slot.perfectDecoRoot, true);
        Set_UIOpen(slot.perfectTextRoot, true);
        Set_UIOpen(slot.normalDecoRoot, false);
        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_QTE_SuccessPerfect);
        Play_QTEAnimation(slot, L"QTE_Success_Perfect");
        break;

    case BattleQTEResult::SuccessNormal:
        Set_UIOpen(slot.normalDecoRoot, true);
        Set_UIOpen(slot.perfectDecoRoot, false);
        Set_UIOpen(slot.perfectTextRoot, false);
        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_QTE_SuccessRegular);
        Play_QTEAnimation(slot, L"QTE_Success_Normal");
        break;

    case BattleQTEResult::Fail:
        Close_ResultVisuals(slot);
        GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_QTE_Miss);
        Play_QTEAnimation(slot, L"QTE_Fail");
        break;

    default:
        break;
    }
}

void UI_QTE::Apply_PresetPlacement(QTE_Slot& slot, const BattleQTERequest& request)
{
     if (slot.root == nullptr || slot.root->Get_Transform() == nullptr)
        return;

    auto rootTr = slot.root->Get_Transform();

    rootTr->Set_Position(request.screenPosition);
    rootTr->Set_Scale(Vec2(request.screenScale, request.screenScale));

    slot.root->Refresh_OpenBaseTransform();
}

int32 UI_QTE::Find_FreeSlotIndex() const
{
    for (uint32 slotIndex = 0; slotIndex < _slots.size(); ++slotIndex)
    {
        if (_slots[slotIndex].stage == QTE_Stage::Closed)
            return static_cast<int32>(slotIndex);
    }

    return -1;
}

int32 UI_QTE::Find_FirstInputSlotIndex() const
{
    for (uint32 slotIndex : _activeSlotOrder)
    {
        if (slotIndex >= _slots.size())
            continue;

        const QTE_Slot& slot = _slots[slotIndex];
        if (slot.stage == QTE_Stage::Intro || slot.stage == QTE_Stage::Progress)
            return static_cast<int32>(slotIndex);
    }

    return -1;
}

void UI_QTE::Remove_InactiveSlotOrder()
{
    _activeSlotOrder.erase(
        remove_if(
            _activeSlotOrder.begin(),
            _activeSlotOrder.end(),
            [this](uint32 slotIndex)
            {
                return slotIndex >= _slots.size() || _slots[slotIndex].stage == QTE_Stage::Closed;
            }),
        _activeSlotOrder.end());
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
