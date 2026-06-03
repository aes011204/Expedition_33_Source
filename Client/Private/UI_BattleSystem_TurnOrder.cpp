#include "UI_BattleSystem_TurnOrder.h"

#include "Client_Constants.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Transform.h"
#include "UI_Utils.h"

using namespace UI_Utils;

namespace
{
    constexpr wchar_t kTurnOrderFrameIntroAnimationName[] = L"TurnOrder_Frame_Intro";
    constexpr wchar_t kTurnOrderCurrentTurnFramePulseAnimationName[] = L"TurnOrder_First_Frame_SizeUpDown";
    constexpr wchar_t kTurnOrderCurrentTurnMonsterPortraitPulseAnimationName[] = L"TurnOrder_CurrentTurn_Monster_Portrait_SizeUpDown";
    constexpr wchar_t kTurnOrderCurrentTurnPlayerPortraitPulseAnimationName[] = L"TurnOrder_CurrentTurn_Player_Portrait_SizeUpDown";
    constexpr wchar_t kTurnOrderMonsterPortraitIntroAnimationName[] = L"TurnOrder_Monster_Portrait_Intro";
    constexpr wchar_t kTurnOrderPlayerPortraitIntroAnimationName[] = L"TurnOrder_Player_Portrait_Intro";
    constexpr float kTurnOrderDefaultSlotAlpha = 1.f;
    constexpr float kTurnOrderSeventhSlotAlpha = 0.6f;
    constexpr float kTurnOrderEighthSlotAlpha = 0.3f;
}

HRESULT UI_BattleSystem_TurnOrder::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close();
    return S_OK;
}

void UI_BattleSystem_TurnOrder::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_BattleSystem_TurnOrder::Open()
{
    const bool wasClosed = (_root != nullptr) ? !_root->Is_Open() : false;

    UI_RuntimePanelBase::Open();

    if (wasClosed && _isAnimationPlaybackEnabled && _isFrameIntroPending)
    {
        Play_TargetAnimation(kTurnOrderFrameIntroAnimationName);
        _isFrameIntroPending = false;
    }
}

void UI_BattleSystem_TurnOrder::Close()
{
    _isFrameIntroPending = false;
    Stop_TargetAnimations();
    Close_UI(_preTurnCurrentFrameRoot);
    UI_RuntimePanelBase::Close();
}

void UI_BattleSystem_TurnOrder::Set_TurnOrderPreview(const vector<TurnOrderEntryDesc>& entries)
{
    if (entries.empty())
    {
        Clear_TurnOrderPreview();
        Stop_TargetAnimations();
        if (_root)
            _root->Close();
        return;
    }

    if (_root)
        _root->Open();

    const bool playTurnStartBatch = (_isAnimationPlaybackEnabled && _isFrameIntroPending);

    if (playTurnStartBatch)
    {
        Play_TargetAnimation(kTurnOrderFrameIntroAnimationName);
        _isFrameIntroPending = false;
    }

    if (!_isAnimationPlaybackEnabled)
    {
        Open_UI(_preTurnCurrentFrameRoot);

        for (uint32 i = 0; i < _slots.size(); ++i)
        {
            TurnOrderSlot& slot = _slots[i];

            if (slot.anchor)
                slot.anchor->Set_AlphaMultiplier(kTurnOrderDefaultSlotAlpha);

            Close_UI(slot.playerRoot);
            Close_UI(slot.monsterRoot);
            Close_UI(slot.currentPlayerOverlay);
            Close_UI(slot.currentMonsterOverlay);

            slot.lastPortraitGuid.clear();
            slot.lastIsEnemy = false;
            slot.hasLastEntry = false;
            slot.lastActorToken = 0;
        }

        return;
    }

    Close_UI(_preTurnCurrentFrameRoot);

    for (uint32 i = 0; i < _slots.size(); ++i)
    {
        TurnOrderSlot& slot = _slots[i];

        if (slot.anchor)
        {
            if (i == 6)
                slot.anchor->Set_AlphaMultiplier(kTurnOrderSeventhSlotAlpha); // 7번째 턴부터는 덜 중요하게 보여서 살짝 흐리게
            else if (i == 7)
                slot.anchor->Set_AlphaMultiplier(kTurnOrderEighthSlotAlpha);  // 8번째 턴은 가장 뒤라 더 많이 흐리게
            else
                slot.anchor->Set_AlphaMultiplier(kTurnOrderDefaultSlotAlpha);
        }

        if (i >= entries.size())
        {
            Close_UI(slot.currentPlayerOverlay);
            Close_UI(slot.currentMonsterOverlay);
            Close_UI(slot.playerRoot);
            Close_UI(slot.monsterRoot);
            slot.lastPortraitGuid.clear();
            slot.lastIsEnemy = false;
            slot.hasLastEntry = false;
            slot.lastActorToken = 0;
            continue;
        }

        const TurnOrderEntryDesc& entry = entries[i];
        const bool isCurrentTurn = (i == 0); // 0번째. Current 턴인지
        const bool slotChanged =
            playTurnStartBatch ||
            !slot.hasLastEntry ||
            slot.lastIsEnemy != entry.isEnemy ||
            slot.lastPortraitGuid != entry.portraitGuid ||
            slot.lastActorToken != entry.actorToken;

        if (isCurrentTurn)
        {
            if (entry.isEnemy)
            {
                Close_UI(slot.currentPlayerOverlay);
                Open_UI(slot.currentMonsterOverlay);
                Close_UI(slot.playerRoot);
                Close_UI(slot.monsterRoot);

                if (slot.currentMonsterPortrait && !entry.portraitGuid.empty())
                    slot.currentMonsterPortrait->Set_TextureGuid(entry.portraitGuid);
            }
            else
            {
                Open_UI(slot.currentPlayerOverlay);
                Close_UI(slot.currentMonsterOverlay);
                Close_UI(slot.playerRoot);
                Close_UI(slot.monsterRoot);

                if (slot.currentPlayerPortrait && !entry.portraitGuid.empty())
                    slot.currentPlayerPortrait->Set_TextureGuid(entry.portraitGuid);
            }

        }
        else
        {
            Close_UI(slot.currentPlayerOverlay);
            Close_UI(slot.currentMonsterOverlay);

            if (entry.isEnemy)
            {
                Close_UI(slot.playerRoot);
                Open_UI(slot.monsterRoot);

                if (slot.monsterPortrait && !entry.portraitGuid.empty())
                    slot.monsterPortrait->Set_TextureGuid(entry.portraitGuid);
            }
            else
            {
                Open_UI(slot.playerRoot);
                Close_UI(slot.monsterRoot);

                if (slot.playerPortrait && !entry.portraitGuid.empty())
                    slot.playerPortrait->Set_TextureGuid(entry.portraitGuid);
            }
        }

        if (_isAnimationPlaybackEnabled && slotChanged)
            Play_SlotPortraitIntro(slot, entry.isEnemy);

        if (_isAnimationPlaybackEnabled && isCurrentTurn && slotChanged)
            Play_CurrentTurnPulse(entry.isEnemy);

        slot.lastPortraitGuid = entry.portraitGuid;
        slot.lastIsEnemy = entry.isEnemy;
        slot.hasLastEntry = true;
        slot.lastActorToken = entry.actorToken;
    }
}

void UI_BattleSystem_TurnOrder::Clear_TurnOrderPreview()
{
    for (TurnOrderSlot& slot : _slots)
    {
        slot.lastPortraitGuid.clear();
        slot.lastIsEnemy = false;
        slot.hasLastEntry = false;
        slot.lastActorToken = 0;
        Close_UI(slot.currentPlayerOverlay);
        Close_UI(slot.currentMonsterOverlay);
        Close_UI(slot.playerRoot);
        Close_UI(slot.monsterRoot);
    }
}

void UI_BattleSystem_TurnOrder::Notify_TurnStarted()
{
    _isAnimationPlaybackEnabled = true;
    _isFrameIntroPending = true;
}

void UI_BattleSystem_TurnOrder::Set_AnimationPlaybackEnabled(bool enabled)
{
    _isAnimationPlaybackEnabled = enabled;
    _isFrameIntroPending = false;

    if (!enabled)
        Stop_TargetAnimations();
}

HRESULT UI_BattleSystem_TurnOrder::Load_UI(uint32 levelIndex)
{
    UNREFERENCED_PARAMETER(levelIndex);

    _uiByName.clear();

    umap<wstring, Shared<UI_Base>> frameUIByName{};
    UI_RuntimeLoader::UI_LoadDesc frameDesc{};
    frameDesc.levelIndex = _levelIndex;
    frameDesc.jsonPath = Constants::UI::UI_BattleSystem_TurnOrder_Frame_Path;
    frameDesc.animationPaths.push_back(Constants::UI_Anim::UI_TurnOrderFrameIntroAnim_Path);
    frameDesc.animationPaths.push_back(Constants::UI_Anim::UI_TurnOrderCurrentTurnFramePulseAnim_Path);
    frameDesc.prototypeTagPrefix = L"BattleSystemTurnOrderFrame";
    frameDesc.outUIByName = &frameUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(frameDesc), E_FAIL);

    Merge_UIMap(frameUIByName);

    _root = Find_UI(_uiByName, L"Panel_BattleSystem_TurnOrder_Frame");
    CHECK_NULL(_root, E_FAIL);

    umap<wstring, Shared<UI_Base>> currentFrameUIByName{};
    UI_RuntimeLoader::UI_LoadDesc currentFrameDesc{};
    currentFrameDesc.levelIndex = _levelIndex;
    currentFrameDesc.jsonPath = Constants::UI::UI_BattleSystem_TurnOrder_CurFrame_Path;
    currentFrameDesc.prototypeTagPrefix = L"BattleSystemTurnOrderCurrentFrame";
    currentFrameDesc.outUIByName = &currentFrameUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(currentFrameDesc), E_FAIL);

    _preTurnCurrentFrameRoot =
        Find_UI(currentFrameUIByName, L"Panel_BattleSystem_TurnOrder_CurrentTurn_Frame");
    CHECK_NULL(_preTurnCurrentFrameRoot, E_FAIL);

    Merge_UIMap(currentFrameUIByName);

    for (uint32 i = 0; i < _slots.size(); ++i)
    {
        umap<wstring, Shared<UI_Base>> playerUIByName{};
        UI_RuntimeLoader::UI_LoadDesc playerDesc{};
        playerDesc.levelIndex = _levelIndex;
        playerDesc.jsonPath = Constants::UI::UI_BattleSystem_TurnOrder_Player_Path;
        playerDesc.animationPaths.push_back(Constants::UI_Anim::UI_TurnOrderPlayerPortraitIntroAnim_Path);
        playerDesc.prototypeTagPrefix = L"BattleSystemTurnOrderPlayer_" + to_wstring(i);
        playerDesc.outUIByName = &playerUIByName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(playerDesc), E_FAIL);

        _slots[i].playerRoot =
            Find_UI(playerUIByName, L"Panel_TurnOrder_Portrait_Player");

        _slots[i].playerPortrait =
            Find_TypedUI<UI_Image>(playerUIByName, L"Image_BattleSystem_TurnOrder_Portrait_Player");

        CHECK_NULL(_slots[i].playerRoot, E_FAIL);

        Merge_UIMap(playerUIByName);

        umap<wstring, Shared<UI_Base>> monsterUIByName{};
        UI_RuntimeLoader::UI_LoadDesc monsterDesc{};
        monsterDesc.levelIndex = _levelIndex;
        monsterDesc.jsonPath = Constants::UI::UI_BattleSystem_TurnOrder_Monster_Path;
        monsterDesc.animationPaths.push_back(Constants::UI_Anim::UI_TurnOrderMonsterPortraitIntroAnim_Path);
        monsterDesc.prototypeTagPrefix = L"BattleSystemTurnOrderMonster_" + to_wstring(i);
        monsterDesc.outUIByName = &monsterUIByName;

        CHECK_FAILED(UI_RuntimeLoader::Load_Document(monsterDesc), E_FAIL);

        _slots[i].monsterRoot =
            Find_UI(monsterUIByName, L"Panel_TurnOrder_Portrait_Monster");
        _slots[i].monsterPortrait =
            Find_TypedUI<UI_Image>(monsterUIByName, L"Image_BattleSystem_TurnOrder_Portrait_Monster");

        CHECK_NULL(_slots[i].monsterRoot, E_FAIL);

        Merge_UIMap(monsterUIByName);
    }

    umap<wstring, Shared<UI_Base>> currentPlayerUIByName{};
    UI_RuntimeLoader::UI_LoadDesc currentPlayerDesc{};
    currentPlayerDesc.levelIndex = _levelIndex;
    currentPlayerDesc.jsonPath = Constants::UI::UI_BattleSystem_TurnOrder_CurPlayer_Path;
    currentPlayerDesc.animationPaths.push_back(Constants::UI_Anim::UI_TurnOrderCurrentTurnPlayerPortraitPulseAnim_Path);
    currentPlayerDesc.prototypeTagPrefix = L"BattleSystemTurnOrderCurrentPlayer";
    currentPlayerDesc.outUIByName = &currentPlayerUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(currentPlayerDesc), E_FAIL);

    _slots[0].currentPlayerOverlay =
        Find_UI(currentPlayerUIByName, L"Panel_BattleSystem_TurnOrder_CurrentTurn_Player_Frame");
    _slots[0].currentPlayerPortrait =
        Find_TypedUI<UI_Image>(currentPlayerUIByName, L"Image_BattleSystem_TurnOrder_CurrentTurn_Player_Portrait");

    CHECK_NULL(_slots[0].currentPlayerOverlay, E_FAIL);
    CHECK_NULL(_slots[0].currentPlayerPortrait, E_FAIL);
    Merge_UIMap(currentPlayerUIByName);

    umap<wstring, Shared<UI_Base>> currentMonsterUIByName{};
    UI_RuntimeLoader::UI_LoadDesc currentMonsterDesc{};
    currentMonsterDesc.levelIndex = _levelIndex;
    currentMonsterDesc.jsonPath = Constants::UI::UI_BattleSystem_TurnOrder_CurMonster_Path;
    currentMonsterDesc.animationPaths.push_back(Constants::UI_Anim::UI_TurnOrderCurrentTurnMonsterPortraitPulseAnim_Path);
    currentMonsterDesc.prototypeTagPrefix = L"BattleSystemTurnOrderCurrentMonster";
    currentMonsterDesc.outUIByName = &currentMonsterUIByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(currentMonsterDesc), E_FAIL);

    _slots[0].currentMonsterOverlay =
        Find_UI(currentMonsterUIByName, L"Panel_BattleSystem_TurnOrder_CurrentTurn_Monster_Frame");
    _slots[0].currentMonsterPortrait =
        Find_TypedUI<UI_Image>(currentMonsterUIByName, L"Image_BattleSystem_TurnOrder_CurrentTurn_Monster_Portrait");

    CHECK_NULL(_slots[0].currentMonsterOverlay, E_FAIL);
    CHECK_NULL(_slots[0].currentMonsterPortrait, E_FAIL);
    Merge_UIMap(currentMonsterUIByName);

    return S_OK;
}

HRESULT UI_BattleSystem_TurnOrder::Bind_UI()
{
    _currentTurnAnchor =
        Find_UI(_uiByName, L"Panel_BattleSystem_TurnOrder_Portrait");
    CHECK_NULL(_currentTurnAnchor, E_FAIL);

    CHECK_NULL(_preTurnCurrentFrameRoot, E_FAIL);
    _preTurnCurrentFrameRoot->Set_Parent(_currentTurnAnchor);
    Close_UI(_preTurnCurrentFrameRoot);

    for (uint32 i = 0; i < _slots.size(); ++i)
    {
        const wstring suffix = to_wstring(i + 1);
        TurnOrderSlot& slot = _slots[i];

        slot.anchor =
            Find_UI(_uiByName, L"Panel_BattleSystem_TurnOrder_Portrait_Frame_" + suffix);
        CHECK_NULL(slot.anchor, E_FAIL);

        CHECK_NULL(slot.playerRoot, E_FAIL);
        CHECK_NULL(slot.monsterRoot, E_FAIL);

        Place_OnSlot(slot.playerRoot, slot.anchor, false);
        Place_OnSlot(slot.monsterRoot, slot.anchor, false);

        if (i == 0)
        {
            slot.currentPlayerOverlay->Set_Parent(_currentTurnAnchor);
            slot.currentMonsterOverlay->Set_Parent(_currentTurnAnchor);
        }

        Close_UI(slot.currentPlayerOverlay);
        Close_UI(slot.currentMonsterOverlay);
        Close_UI(slot.playerRoot);
        Close_UI(slot.monsterRoot);
    }

    return S_OK;
}

HRESULT UI_BattleSystem_TurnOrder::Load_UIFragment(
    const wchar_t* jsonPath,
    const wstring& prototypeTagPrefix,
    umap<wstring, Shared<UI_Base>>& outUIByName) const
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = jsonPath;
    desc.prototypeTagPrefix = prototypeTagPrefix;
    desc.outUIByName = &outUIByName;

    return UI_RuntimeLoader::Load_Document(desc);
}

void UI_BattleSystem_TurnOrder::Merge_UIMap(const umap<wstring, Shared<UI_Base>>& src)
{
    for (const auto& [name, ui] : src)
        _uiByName[name] = ui;
}

void UI_BattleSystem_TurnOrder::Place_OnSlot(
    const Shared<UI_Base>& ui,
    const Shared<UI_Base>& parent,
    bool keepOriginalSize) const
{
    if (ui == nullptr)
        return;

    ui->Set_Parent(parent);

    UI_Transform* transform = ui->Get_Transform();
    if (transform == nullptr)
        return;

    transform->Set_Anchor(Vec2(0.f, 0.f));
    transform->Set_Position(Vec2(100.f, 50.f));
    transform->Set_Pivot(Vec2(0.5f, 0.5f));
    transform->Set_Scale(Vec2(1.f, 1.f));
    transform->Set_RotationRadians(0.f);

    if (!keepOriginalSize)
        transform->Set_Size(Vec2(200.f, 100.f));

    ui->Refresh_OpenBaseTransform();
}

void UI_BattleSystem_TurnOrder::Open_UI(const Shared<UI_Base>& ui) const
{
    if (ui)
        ui->Open();
}

void UI_BattleSystem_TurnOrder::Close_UI(const Shared<UI_Base>& ui) const
{
    if (ui)
        ui->Close();
}

void UI_BattleSystem_TurnOrder::Play_TargetAnimation(const wchar_t* animationName)
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Play_UIAnimation(animationName, true);
    }
}

void UI_BattleSystem_TurnOrder::Stop_TargetAnimations()
{
    for (auto& [uiName, ui] : _uiByName)
    {
        UNREFERENCED_PARAMETER(uiName);

        if (ui == nullptr)
            continue;

        ui->Stop_UIAnimation();
    }
}

void UI_BattleSystem_TurnOrder::Play_SlotPortraitIntro(TurnOrderSlot& slot, bool isEnemy)
{
    if (isEnemy)
    {
        if (slot.monsterPortrait)
            slot.monsterPortrait->Play_UIAnimation(kTurnOrderMonsterPortraitIntroAnimationName, true);

        return;
    }

    if (slot.playerPortrait)
        slot.playerPortrait->Play_UIAnimation(kTurnOrderPlayerPortraitIntroAnimationName, true);
}

void UI_BattleSystem_TurnOrder::Play_CurrentTurnPulse(bool isEnemy)
{
    Play_TargetAnimation(kTurnOrderCurrentTurnFramePulseAnimationName);

    if (isEnemy)
    {
        Play_TargetAnimation(kTurnOrderCurrentTurnMonsterPortraitPulseAnimationName);
        return;
    }

    Play_TargetAnimation(kTurnOrderCurrentTurnPlayerPortraitPulseAnimationName);
}

Unique<UI_BattleSystem_TurnOrder> UI_BattleSystem_TurnOrder::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_BattleSystem_TurnOrder>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_BattleSystem_TurnOrder");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_BattleSystem_TurnOrder::Free()
{
    if (_root)
        _root->Close();

    Stop_TargetAnimations();
    Close_UI(_preTurnCurrentFrameRoot);

    _root.reset();
    _uiByName.clear();
    _currentTurnAnchor.reset();
    _preTurnCurrentFrameRoot.reset();

    for (TurnOrderSlot& slot : _slots)
    {
        slot.lastPortraitGuid.clear();
        slot.lastIsEnemy = false;
        slot.hasLastEntry = false;
        slot.lastActorToken = 0;
        slot.anchor.reset();
        slot.currentPlayerOverlay.reset();
        slot.currentMonsterOverlay.reset();
        slot.playerRoot.reset();
        slot.monsterRoot.reset();
        slot.playerPortrait.reset();
        slot.monsterPortrait.reset();
        slot.currentPlayerPortrait.reset();
        slot.currentMonsterPortrait.reset();
    }

    Base::Free();
}
