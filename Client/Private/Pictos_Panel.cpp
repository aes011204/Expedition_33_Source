#include "Pictos_Panel.h"

#include "ClientInstance.h"
#include "GameInstance.h"
#include "PlayerInventory.h"
#include "PlayerStatCom.h"
#include "Player_WorldBase.h"
#include "PreviewPlayer.h"
#include "UIPreviewManager.h"
#include "UI_Base.h"
#include "UI_Button.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

IMPLEMENT_REFLECTION(Pictos_Panel)
{
    auto& info = GetStaticReflectionInfo();
    info.properties.clear();

    info.displayName = "Pictos_Panel";
    info.category = "Runtime UI";

    PROPERTY_VEC2("Tooltip Position", _tooltipPosition, 1.f);

    return true;
}

namespace
{
    void Set_UIMapOpenState(const umap<wstring, Shared<UI_Base>>& uiByName, bool isOpen)
    {
        for (const auto& [uiName, ui] : uiByName)
        {
            UNREFERENCED_PARAMETER(uiName);

            if (ui == nullptr)
                continue;

            if (isOpen)
                ui->Open();
            else
                ui->Close();
        }
    }

    vector<UIPreviewManager::PreviewSlotDesc> Build_PictosPreviewLayout()
    {
        return
        {
            { HeroId::Lune,    Vec3(1.1f,  0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
            { HeroId::Gustave, Vec3(0.0f,  0.f,  0.f), Vec3(0.f, 0.f,  0.f) },
            { HeroId::Maelle,  Vec3(-1.1f, 0.f, -0.1f), Vec3(0.f, 0.f, -0.1f) },
        };
    }

    void Show_PictosPreview(HeroId heroId)
    {
        const auto previewLayout = Build_PictosPreviewLayout();
        CLIENT->Apply_PreviewLayout(previewLayout);
        CLIENT->Show_Preview(heroId, PreviewShotPanel::Pictos);

        if (const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager())
        {
            for (const auto& previewHero : previewManager->Get_PreviewHeroes())
            {
                if (previewHero.player == nullptr)
                    continue;

                previewHero.player->Set_Visible(previewHero.heroId == heroId); // Begin_Preview가 3명을 모두 켠 뒤 선택 영웅만 남긴다.
            }
        }
    }

    // 픽토스 버튼 Json 위치
    wstring Build_PictosButtonJsonPath(const string& buttonUiName)
    {
        return L"UI/UI_Json/CommonSystem/Pictos/Button/" +
            String::ToWString(buttonUiName) +
            L".json";
    }
    // 픽토스 툴팁 Json 위치
    wstring Build_PictosTooltipJsonPath(const string& tooltipUiName)
    {
        return L"UI/UI_Json/CommonSystem/Pictos/Tooltip/" +
            String::ToWString(tooltipUiName) +
            L".json";
    }

    wstring Build_PictosButtonRootName(const string& pictosId)
    {
        return L"Panel_Pictos_" + String::ToWString(pictosId);
    }

    wstring Build_PictosButtonButtonName(const string& pictosId)
    {
        return L"Button_Pictos_" + String::ToWString(pictosId);
    }

    wstring Build_PictosButtonMarkImageName(const string& pictosId)
    {
        return L"Image_Pictos_" + String::ToWString(pictosId) + L"_Mark";
    }

    wstring Build_PictosTooltipRootName(const string& pictosId)
    {
        return L"Panel_Pictos_" + String::ToWString(pictosId) + L"_Tooltip";
    }

    void Match_ButtonHitBoxToTarget(const Shared<UI_Button>& button, const Shared<UI_Button>& target)
    {
        if (button == nullptr || target == nullptr || button->Get_Transform() == nullptr || target->Get_Transform() == nullptr)
            return;

        UI_Transform* buttonTransform = button->Get_Transform();
        const UI_Transform* targetTransform = target->Get_Transform();

        buttonTransform->Set_Position(targetTransform->Get_Position());
        buttonTransform->Set_Size(targetTransform->Get_Size());
        buttonTransform->Set_Scale(targetTransform->Get_Scale());
        buttonTransform->Set_Pivot(targetTransform->Get_Pivot());
        buttonTransform->Set_Anchors(targetTransform->Get_AnchorMin(), targetTransform->Get_AnchorMax());
        buttonTransform->Set_RotationRadians(targetTransform->Get_RotationRadians());
        button->Refresh_OpenBaseTransform();
    }
}

HRESULT Pictos_Panel::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Close_Immediately(false);

    return S_OK;
}

void Pictos_Panel::Update(float timeDelta)
{
    if (!Is_Open())
        return;

    if (_isClickInputLatched)
    {
        if (GAME->KeyDown(KEY_TYPE::LBUTTON) || GAME->KeyPress(KEY_TYPE::LBUTTON))
            return; // 같은 클릭 입력으로 UI가 다시 눌리는 현상을 막는다.

        _isClickInputLatched = false;
    }

    if (Handle_ClearEquippedPictosInput())
        return;

    if (Handle_AvailablePictosInput())
    {
        Refresh_Tooltips();
        Refresh_ActionHints();
        return; // Available 선택 클릭은 후보 지정만 하고, 같은 프레임에 Equip 슬롯 입력으로 재소비하지 않는다.
    }

    Handle_SlotSelectionInput();

    if (_isClickInputLatched)
        return; // 실제 장착으로 UI를 다시 그린 프레임에는 같은 클릭을 다른 버튼이 다시 먹지 않게 여기서 끊는다.

    Refresh_Tooltips();
    Refresh_ActionHints();
}

void Pictos_Panel::Open()
{
    if (_root == nullptr)
        return;

    if (Is_Open())
        return;

    UI_RuntimePanelBase::Open();
    Set_UIMapOpenState(_uiByName, true);

    if (_selectedHeroId == HeroId::END)
        _selectedHeroId = Resolve_DefaultHeroId();

    CLIENT->Begin_Preview(PreviewShotPanel::Pictos);
    Show_PictosPreview(_selectedHeroId);
    CLIENT->Play_PreviewAnimation(_selectedHeroId, PreviewAnimState::Pictos_Intro, PreviewAnimState::Pictos_Idle);

    _isTooltipHoverArmed = false; // 패널을 연 직후 커서가 이미 버튼 위에 있어도 툴팁이 바로 뜨지 않게 한다.
    Refresh_All();

    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);
    GAME->Unlock_Mouse();
    GAME->Set_GameplayPaused(true);
}

void Pictos_Panel::Close()
{
    Close_Immediately(true);
}

void Pictos_Panel::Close_Immediately(bool restoreWorldControl)
{
    _isProgressionContentMode = false;
    _isTooltipHoverArmed = false;
    _isClickInputLatched = false;
    UI_RuntimePanelBase::Close();
    Set_UIMapOpenState(_uiByName, false);
    CLIENT->Hide_Preview();

    for (auto& [pictosId, tooltipUiByName] : _tooltipUiById)
    {
        UNREFERENCED_PARAMETER(pictosId);
        Set_UIMapOpenState(tooltipUiByName, false); // 툴팁 루트만 닫으면 자식 텍스트/아이콘이 남을 수 있어서 전체 UI 맵을 같이 닫는다.
    }

    if (restoreWorldControl)
    {
        GAME->Set_GameplayPaused(false);
        GAME->Lock_Mouse();
    }
}

void Pictos_Panel::Close_ForMenuTransition()
{
    Close_Immediately(false);
}

void Pictos_Panel::Open_AsProgressionContent(HeroId selectedHeroId, size_t selectedSlotIndex)
{
    if (_root == nullptr)
        return;

    _isProgressionContentMode = true;
    _isTooltipHoverArmed = false; // Character Progression에서 탭 전환 직후의 정적 hover는 무시하고, 실제 마우스 이동 이후부터만 툴팁을 띄운다.
    _isClickInputLatched = false;
    _selectedHeroId = (selectedHeroId == HeroId::END) ? Resolve_DefaultHeroId() : selectedHeroId;
    _selectedEquipSlotIndex = min<size_t>(selectedSlotIndex, _equippedSlots.size() - 1);
    _pendingEquipPictosId.clear(); // 탭 재진입 시 이전 Available 선택이 남아 있지 않게 초기화한다.

    if (!Is_Open())
        UI_RuntimePanelBase::Open();

    Set_UIMapOpenState(_uiByName, true);
    Refresh_All();
}

void Pictos_Panel::Close_AsProgressionContent()
{
    if (!_isProgressionContentMode && !Is_Open())
        return;

    _isProgressionContentMode = false;
    _isTooltipHoverArmed = false;
    _isClickInputLatched = false;
    _pendingEquipPictosId.clear();
    UI_RuntimePanelBase::Close();
    Set_UIMapOpenState(_uiByName, false);

    for (auto& [pictosId, tooltipUiByName] : _tooltipUiById)
    {
        UNREFERENCED_PARAMETER(pictosId);
        Set_UIMapOpenState(tooltipUiByName, false); // Progression 탭에서 빠질 때도 tooltip 문서 전체를 내려야 잔상이 남지 않는다.
    }
}

void Pictos_Panel::Refresh_ForSelectedHero(HeroId selectedHeroId)
{
    _selectedHeroId = (selectedHeroId == HeroId::END) ? Resolve_DefaultHeroId() : selectedHeroId;
    _isClickInputLatched = false;
    _pendingEquipPictosId.clear(); // 다른 영웅으로 바뀌면 이전 영웅에서 고른 pending 장착 후보를 버린다.
    Refresh_All();
}

void Pictos_Panel::Set_SelectedHeroId(HeroId heroId)
{
    _selectedHeroId = (heroId == HeroId::END) ? Resolve_DefaultHeroId() : heroId;
    _isClickInputLatched = false;
    _pendingEquipPictosId.clear();

    if (Is_Open())
        Refresh_All();
}

void Pictos_Panel::Set_SelectedEquipSlotIndex(size_t slotIndex)
{
    _selectedEquipSlotIndex = min<size_t>(slotIndex, _equippedSlots.size() - 1);

    if (Is_Open())
        Refresh_ActionHints();
}

void Pictos_Panel::Append_RuntimeUIEntries(
    vector<RuntimeUIEntry>& outEntries,
    const string& category,
    const string& rootName) const
{
    if (_root == nullptr)
        return;

    RuntimeUIEntry rootEntry{};
    rootEntry.category = category;
    rootEntry.name = rootName;
    rootEntry.ui = _root;
    rootEntry.editorTarget = const_cast<Pictos_Panel*>(this);
    outEntries.push_back(rootEntry);

    Append_RuntimeUIMapEntries(outEntries, category, _uiByName, _root, const_cast<Pictos_Panel*>(this));

    for (const AvailablePictosRowUI& row : _availableRows)
        Append_RuntimeUIMapEntries(outEntries, category, row.uiByName, nullptr, const_cast<Pictos_Panel*>(this));

    for (const auto& [pictosId, tooltipUiByName] : _tooltipUiById)
    {
        UNREFERENCED_PARAMETER(pictosId);
        Append_RuntimeUIMapEntries(outEntries, category, tooltipUiByName, nullptr, const_cast<Pictos_Panel*>(this));
    }
}

HRESULT Pictos_Panel::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_Pictos_Path;
    desc.prototypeTagPrefix = L"Picktos";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_Pictos");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT Pictos_Panel::Bind_UI()
{
    _availableListRoot = Find_UI(_uiByName, L"Panel_AvailablePictos");
    _selectSlotHintRoot = Find_UI(_uiByName, L"Panel_Select_Pictos_Slot");
    _equipHintRoot = Find_UI(_uiByName, L"Panel_Pictos_Equip");

    _equippedSlots[0].root = Find_UI(_uiByName, L"Panel_Pictos_Slot_First");
    _equippedSlots[1].root = Find_UI(_uiByName, L"Panel_Pictos_Slot_Second");
    _equippedSlots[2].root = Find_UI(_uiByName, L"Panel_Pictos_Slot_Third");

    _equippedSlots[0].selectButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Pictos_Slot_First", "Picktos");
    _equippedSlots[1].selectButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Pictos_Slot_Second", "Picktos");
    _equippedSlots[2].selectButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Pictos_Slot_Third", "Picktos");

    _equippedSlots[0].emptyButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Pictos_Slot_First_Empty", "Picktos");
    _equippedSlots[1].emptyButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Pictos_Slot_Second_Empty", "Picktos");
    _equippedSlots[2].emptyButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_Pictos_Slot_Third_Empty", "Picktos");

    _builtinAvailableRoots[0] = Find_UI(_uiByName, L"Panel_AvailablePictos_First");
    _builtinAvailableRoots[1] = Find_UI(_uiByName, L"Panel_AvailablePictos_Second");
    _builtinAvailableRoots[2] = Find_UI(_uiByName, L"Panel_AvailablePictos_Third");
    _builtinAvailableRoots[3] = Find_UI(_uiByName, L"Panel_AvailablePictos_Fourth");
    _builtinAvailableRoots[4] = Find_UI(_uiByName, L"Panel_AvailablePictos_Fifth");
    _builtinAvailableRoots[5] = Find_UI(_uiByName, L"Panel_AvailablePictos_Sixth");

    _availableSlots[0].root = _builtinAvailableRoots[0];
    _availableSlots[1].root = _builtinAvailableRoots[1];
    _availableSlots[2].root = _builtinAvailableRoots[2];
    _availableSlots[3].root = _builtinAvailableRoots[3];
    _availableSlots[4].root = _builtinAvailableRoots[4];
    _availableSlots[5].root = _builtinAvailableRoots[5];

    _availableSlots[0].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_AvailablePictos_First", "Picktos");
    _availableSlots[1].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_AvailablePictos_Second", "Picktos");
    _availableSlots[2].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_AvailablePictos_Third", "Picktos");
    _availableSlots[3].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_AvailablePictos_Fourth", "Picktos");
    _availableSlots[4].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_AvailablePictos_Fifth", "Picktos");
    _availableSlots[5].button = Find_TypedUI<UI_Button>(_uiByName, L"Button_AvailablePictos_Sixth", "Picktos");

    _availableSlots[0].nameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_AvailablePictos_First", "Picktos");
    _availableSlots[1].nameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_AvailablePictos_Second", "Picktos");
    _availableSlots[2].nameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_AvailablePictos_Third", "Picktos");
    _availableSlots[3].nameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_AvailablePictos_Fourth", "Picktos");
    _availableSlots[4].nameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_AvailablePictos_Fifth", "Picktos");
    _availableSlots[5].nameText = Find_TypedUI<UI_Text>(_uiByName, L"Text_AvailablePictos_Sixth", "Picktos");

    _availableSlots[0].markRoot = Find_UI(_uiByName, L"Panel_AvailablePictos_First_Mark");
    _availableSlots[1].markRoot = Find_UI(_uiByName, L"Panel_AvailablePictos_Second_Mark");
    _availableSlots[2].markRoot = Find_UI(_uiByName, L"Panel_AvailablePictos_Third_Mark");
    _availableSlots[3].markRoot = Find_UI(_uiByName, L"Panel_AvailablePictos_Fourth_Mark");
    _availableSlots[4].markRoot = Find_UI(_uiByName, L"Panel_AvailablePictos_Fifth_Mark");
    _availableSlots[5].markRoot = Find_UI(_uiByName, L"Panel_AvailablePictos_Sixth_Mark");

    _availableSlots[0].markImage = Find_TypedUI<UI_Image>(_uiByName, L"Image_AvailablePictos_First_Mark", "Picktos");
    _availableSlots[1].markImage = Find_TypedUI<UI_Image>(_uiByName, L"Image_AvailablePictos_Second_Mark", "Picktos");
    _availableSlots[2].markImage = Find_TypedUI<UI_Image>(_uiByName, L"Image_AvailablePictos_Third_Mark", "Picktos");
    _availableSlots[3].markImage = Find_TypedUI<UI_Image>(_uiByName, L"Image_AvailablePictos_Fourth_Mark", "Picktos");
    _availableSlots[4].markImage = Find_TypedUI<UI_Image>(_uiByName, L"Image_AvailablePictos_Fifth_Mark", "Picktos");
    _availableSlots[5].markImage = Find_TypedUI<UI_Image>(_uiByName, L"Image_AvailablePictos_Sixth_Mark", "Picktos");

    CHECK_NULL(_availableListRoot, E_FAIL);
    CHECK_NULL(_selectSlotHintRoot, E_FAIL);
    CHECK_NULL(_equipHintRoot, E_FAIL);

    for (size_t i = 0; i < _equippedSlots.size(); ++i)
    {
        CHECK_NULL(_equippedSlots[i].root, E_FAIL);
        CHECK_NULL(_equippedSlots[i].selectButton, E_FAIL);
        CHECK_NULL(_equippedSlots[i].emptyButton, E_FAIL);
        Match_ButtonHitBoxToTarget(_equippedSlots[i].selectButton, _equippedSlots[i].emptyButton); // filled 슬롯의 입력 영역을 빈 슬롯 버튼과 같은 110x110 영역으로 고정한다.
    }

    for (size_t i = 0; i < _availableSlots.size(); ++i)
    {
        CHECK_NULL(_availableSlots[i].root, E_FAIL);
        CHECK_NULL(_availableSlots[i].button, E_FAIL);
        CHECK_NULL(_availableSlots[i].nameText, E_FAIL);
        CHECK_NULL(_availableSlots[i].markRoot, E_FAIL);
        CHECK_NULL(_availableSlots[i].markImage, E_FAIL);
        Clear_AvailableSlot(i);
    }

    return S_OK;
}

void Pictos_Panel::Handle_SlotSelectionInput()
{
    const Shared<PlayerStatCom> statCom = Resolve_SelectedPlayerStat();
    CHECK_NULL(statCom);

    const auto& equippedPictosIds = statCom->Get_EquippedPictosIds();

    size_t clickedSlotIndex = _equippedSlots.size();
    for (size_t i = 0; i < _equippedSlots.size(); ++i)
    {
        EquippedSlotUI& slot = _equippedSlots[i];
        const bool isFilledSlot = (i < equippedPictosIds.size() && !equippedPictosIds[i].empty());
        const bool rawClickedFilled = (slot.selectButton && slot.selectButton->Was_Clicked());
        const bool rawClickedEmpty = (slot.emptyButton && slot.emptyButton->Was_Clicked());

        const bool clickedFilled =
            (isFilledSlot && rawClickedFilled); // 실제 장착 데이터가 있는 슬롯만 filled 버튼 입력을 받는다.
        const bool clickedEmpty =
            (!isFilledSlot && rawClickedEmpty); // 장착 데이터가 비어 있는 슬롯만 empty 버튼 입력을 받는다.

        if (!clickedFilled && !clickedEmpty)
            continue;

        clickedSlotIndex = i;
        break;
    }

    if (clickedSlotIndex >= _equippedSlots.size())
        return;

    _selectedEquipSlotIndex = clickedSlotIndex; // 어떤 슬롯에 장착할지 계산 위치

    if (!_pendingEquipPictosId.empty())
    {
        statCom->Set_EquippedPictos(clickedSlotIndex, _pendingEquipPictosId);
        CLIENT->Sync_PreviewHeroRuntimeData(_selectedHeroId);

        _pendingEquipPictosId.clear();
        _selectedEquipSlotIndex = _equippedSlots.size();
        _isClickInputLatched = true;

        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        Refresh_All();
        return;
    }

    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
}

bool Pictos_Panel::Handle_AvailablePictosInput()
{
    if (!_pendingEquipPictosId.empty())
        return false; // 이미 하나 집은 상태에서는 다른 Available 클릭으로 pending 후보를 계속 바꾸지 않는다.

    for (AvailablePictosSlotUI& slot : _availableSlots)
    {
        if (slot.button == nullptr || slot.pictosId.empty() || !slot.button->Was_Clicked())
            continue;

        _pendingEquipPictosId = slot.pictosId; // Available 클릭은 이제 즉시 장착이 아니라 장착 후보 선택만 담당한다.
        GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
        Refresh_ActionHints();
        return true;
    }

    return false;
}

bool Pictos_Panel::Handle_ClearEquippedPictosInput()
{
	if (!GAME->KeyDown(KEY_TYPE::R))
        return false;

    Clear_EquippedPictos();
    return true;
}

void Pictos_Panel::Clear_EquippedPictos()
{
	const Shared<PlayerStatCom> statCom = Resolve_SelectedPlayerStat();
	CHECK_NULL(statCom);

	bool hasEquippedPictos = false;
    const auto& equippedPictosIds = statCom->Get_EquippedPictosIds();

    for (const string& pictosId : equippedPictosIds)
    {
        if (!pictosId.empty())
        {
            hasEquippedPictos = true;
            break;
        }
    }

    if (!hasEquippedPictos)
        return;

    for (size_t slotIndex = 0; slotIndex < PlayerStatCom::EquippedPictosSlotCount; ++slotIndex)
        statCom->Set_EquippedPictos(slotIndex, ""); // Set_EquippedPictos 안에서 스탯 재계산까지 즉시 수행된다.

    _pendingEquipPictosId.clear();                  // 들고 있던 Available 후보도 같이 취소한다.
    _selectedEquipSlotIndex = _equippedSlots.size(); // 해제 후 특정 슬롯에 선택 상태가 남지 않게 푼다.

    CLIENT->Sync_PreviewHeroRuntimeData(_selectedHeroId);
    GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);
    Refresh_All();
}

void Pictos_Panel::Refresh_All()
{
    Refresh_EquippedPictos();
    Refresh_AvailablePictos();
    Refresh_Tooltips();
    Refresh_ActionHints();
}

void Pictos_Panel::Refresh_EquippedPictos()
{
    const Shared<PlayerStatCom> statCom = Resolve_SelectedPlayerStat();
    CHECK_NULL(statCom);

    for (size_t i = 0; i < _equippedSlots.size(); i++)
    {
        EquippedSlotUI& slot = _equippedSlots[i];
        const string pictosId = statCom->Get_EquippedPictosIds()[i];

        if (pictosId.empty())
        {
            Clear_EquippedSlotVisual(i);

            if (slot.emptyButton)
            {
                slot.emptyButton->Set_Disabled(false);
                slot.emptyButton->Open();
            }

            if (slot.selectButton)
            {
                slot.selectButton->Set_Disabled(true);
                slot.selectButton->Close(); // 넓은 투명 슬롯 버튼은 실제 슬롯 버튼과 입력이 겹치므로 쓰지 않는다.
            }

            continue;
        }

        const GameDataTable::PictosDesc* pictosDesc = CLIENT->Get_PictosDesc(pictosId);
        if (pictosDesc == nullptr || FAILED(Ensure_EquippedSlotVisualLoaded(i, pictosId)))
        {
            Clear_EquippedSlotVisual(i);

            if (slot.emptyButton)
            {
                slot.emptyButton->Set_Disabled(false);
                slot.emptyButton->Open();
            }

            if (slot.selectButton)
            {
                slot.selectButton->Set_Disabled(true);
                slot.selectButton->Close(); // 카드 로드 실패 시에도 넓은 filled 슬롯 버튼은 열지 않는다.
            }

            continue;
        }

        if (slot.emptyButton)
        {
            slot.emptyButton->Set_Disabled(true);
            slot.emptyButton->Close();
        }

        if (slot.selectButton)
        {
            slot.selectButton->Set_Disabled(false);
            slot.selectButton->Open(); // 장착된 슬롯은 슬롯 전용 버튼으로만 교체 대상을 고른다.
        }

        if (slot.visualRoot)
            slot.visualRoot->Open();

        Ensure_TooltipLoaded(*pictosDesc);
    }
}

void Pictos_Panel::Refresh_AvailablePictos()
{
    const auto inventory = CLIENT->Get_PlayerInventory();
    if (inventory == nullptr)
    {
        for (size_t i = 0; i < _availableSlots.size(); ++i)
            Clear_AvailableSlot(i);
        return;
    }

    const vector<string>& unlockedPictosIds = inventory->Get_UnlockedPictosIds();
    for (size_t i = 0; i < _availableSlots.size(); ++i)
    {
        if (i >= unlockedPictosIds.size())
        {
            Clear_AvailableSlot(i);
            continue;
        }

        const GameDataTable::PictosDesc* pictosDesc = CLIENT->Get_PictosDesc(unlockedPictosIds[i]);
        if (pictosDesc == nullptr)
        {
            Clear_AvailableSlot(i);
            continue;
        }

        AvailablePictosSlotUI& slot = _availableSlots[i];
        slot.pictosId = pictosDesc->id;

        if (slot.nameText)
            slot.nameText->Set_Text(pictosDesc->name);

        if (slot.markRoot)
            slot.markRoot->Open();

        if (slot.root)
            slot.root->Open();

        if (FAILED(Ensure_TooltipLoaded(*pictosDesc)))
            continue;
    }
}

void Pictos_Panel::Refresh_Tooltips()
{
    for (auto& [pictosId, tooltipUiByName] : _tooltipUiById)
    {
        UNREFERENCED_PARAMETER(pictosId);
        Set_UIMapOpenState(tooltipUiByName, false); // hover 전에는 모든 tooltip 문서 전체를 닫아 둔다.
    }

    _isTooltipHoverArmed = true; // 현재는 hover 디버깅/실사용 우선으로 즉시 arm 한다.

    for (AvailablePictosSlotUI& slot : _availableSlots)
    {
        if (slot.root == nullptr || slot.button == nullptr || slot.pictosId.empty() || !slot.root->Is_Open())
            continue;

        if (!slot.button->Is_Hovered())
            continue;

        const auto it = _tooltipRootsById.find(slot.pictosId);
        if (it == _tooltipRootsById.end() || it->second == nullptr)
        {
            LOG_WARN("[Pictos Tooltip] missing tooltip root for pictosId={}", slot.pictosId);
            return;
        }

        Shared<UI_Base> tooltipRoot = it->second;
        if (tooltipRoot->Get_Transform())
        {
            tooltipRoot->Get_Transform()->Set_Position(_tooltipPosition); // hover된 픽토스 설명 위치를 인스펙터 튜닝값으로 맞춘다.
            tooltipRoot->Refresh_OpenBaseTransform();
        }
        Set_UIMapOpenState(_tooltipUiById[slot.pictosId], true); // 선택된 tooltip은 루트와 자식 UI를 함께 열어야 정상 배치된다.
        return; // 동시에 하나의 tooltip만 연다.
    }
}

void Pictos_Panel::Refresh_ActionHints()
{
    if (_selectSlotHintRoot == nullptr || _equipHintRoot == nullptr)
        return;

    const Shared<PlayerStatCom> statCom = Resolve_SelectedPlayerStat();
    const bool hasValidStatCom = (statCom != nullptr);
    const bool hasPendingPictos = !_pendingEquipPictosId.empty();

    if (!hasValidStatCom || !hasPendingPictos)
    {
        _selectSlotHintRoot->Open();
        _equipHintRoot->Close();
        return;
    }

    _selectSlotHintRoot->Close(); // Available에서 픽토스를 하나 고른 뒤에는 Equip 슬롯을 누르라는 힌트만 남긴다.
    _equipHintRoot->Open(); 
}

Shared<PlayerStatCom> Pictos_Panel::Resolve_SelectedPlayerStat() const
{
    const Shared<Player_WorldBase> hero = CLIENT->Find_Hero(_selectedHeroId);
    if (hero == nullptr)
        return nullptr;

    return hero->Get_Component<PlayerStatCom>();
}

HRESULT Pictos_Panel::Ensure_AvailableRowCount(size_t rowCount)
{
    while (_availableRows.size() < rowCount)
        _availableRows.push_back({});

    return S_OK;
}

HRESULT Pictos_Panel::Ensure_AvailableRowLoaded(size_t rowIndex, const GameDataTable::PictosDesc& pictosDesc)
{
    if (rowIndex >= _availableRows.size())
        return E_FAIL;

    AvailablePictosRowUI& row = _availableRows[rowIndex];

    if (row.pictosId == pictosDesc.id && row.root != nullptr && row.button != nullptr)
    {
        if (row.root->Get_Transform())
        {
            row.root->Get_Transform()->Set_Position(Get_AvailableRowPosition(rowIndex));
            row.root->Refresh_OpenBaseTransform();
        }

        return S_OK;
    }

    Clear_AvailableRow(rowIndex);

    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = Build_PictosButtonJsonPath(pictosDesc.buttonUi);
    desc.prototypeTagPrefix = L"PictosAvailable_" + to_wstring(rowIndex);
    desc.outUIByName = &row.uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    row.root = Find_UI(row.uiByName, Build_PictosButtonRootName(pictosDesc.id));
    row.button = Find_TypedUI<UI_Button>(row.uiByName, Build_PictosButtonButtonName(pictosDesc.id), "PictosAvailable");
    row.pictosId = pictosDesc.id;
    row.wasHovered = false;

    CHECK_NULL(row.root, E_FAIL);
    CHECK_NULL(row.button, E_FAIL);
    CHECK_NULL(_availableListRoot, E_FAIL);

    row.root->Set_Parent(_availableListRoot);

    if (row.root->Get_Transform())
    {
        row.root->Get_Transform()->Set_Position(Get_AvailableRowPosition(rowIndex)); // ChangeSkill과 같은 좌/우 2열 배치
        row.root->Refresh_OpenBaseTransform();
    }

    row.root->Close();

    return S_OK;
}

HRESULT Pictos_Panel::Ensure_EquippedSlotVisualLoaded(size_t slotIndex, const string& pictosId)
{
    if (slotIndex >= _equippedSlots.size())
        return E_FAIL;

    EquippedSlotUI& slot = _equippedSlots[slotIndex];
    const GameDataTable::PictosDesc* pictosDesc = CLIENT->Get_PictosDesc(pictosId);
    CHECK_NULL(pictosDesc, E_FAIL);

    if (slot.pictosId == pictosId && slot.visualRoot != nullptr && slot.visualButton != nullptr)
    {
        slot.visualButton->Set_Disabled(true); // 동적 카드 버튼은 시각 요소로만 사용한다.

        if (slot.visualRoot->Get_Transform() && slot.emptyButton && slot.emptyButton->Get_Transform())
        {
            Vec2 slotPosition = slot.emptyButton->Get_Transform()->Get_Position();
            if (slot.visualButton->Get_Transform() && slot.visualMarkImage && slot.visualMarkImage->Get_Transform())
                slotPosition += slot.visualButton->Get_Transform()->Get_Position() - slot.visualMarkImage->Get_Transform()->Get_Position(); // 카드 마크의 로컬 위치 차이만큼 더해서 다이아 중심을 빈 슬롯 중심에 맞춘다.

            slot.visualRoot->Get_Transform()->Set_Position(slotPosition);
            slot.visualRoot->Refresh_OpenBaseTransform();
        }

        return S_OK;
    }

    Clear_EquippedSlotVisual(slotIndex);

    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = Build_PictosButtonJsonPath(pictosDesc->buttonUi);
    desc.prototypeTagPrefix = L"PictosEquipped_" + to_wstring(slotIndex);
    desc.outUIByName = &slot.visualUiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    slot.visualRoot = Find_UI(slot.visualUiByName, Build_PictosButtonRootName(pictosId));
    slot.visualButton = Find_TypedUI<UI_Button>(slot.visualUiByName, Build_PictosButtonButtonName(pictosId), "PictosEquipped");
    slot.visualMarkImage = Find_TypedUI<UI_Image>(slot.visualUiByName, Build_PictosButtonMarkImageName(pictosId), "PictosEquipped");
    slot.pictosId = pictosId;

    CHECK_NULL(slot.visualRoot, E_FAIL);
    CHECK_NULL(slot.visualButton, E_FAIL);
    CHECK_NULL(slot.visualMarkImage, E_FAIL);
    CHECK_NULL(slot.root, E_FAIL);

    slot.visualButton->Set_Disabled(true); // 동적 카드 버튼은 시각 요소로만 사용한다.
    slot.visualRoot->Set_Parent(slot.root);

    if (slot.visualRoot->Get_Transform())
    {
        Vec2 slotPosition(0.f, 0.f);
        if (slot.emptyButton && slot.emptyButton->Get_Transform())
            slotPosition = slot.emptyButton->Get_Transform()->Get_Position();

        if (slot.visualButton && slot.visualButton->Get_Transform() && slot.visualMarkImage && slot.visualMarkImage->Get_Transform())
            slotPosition += slot.visualButton->Get_Transform()->Get_Position() - slot.visualMarkImage->Get_Transform()->Get_Position(); // 카드 루트 중심이 아니라 카드 안 마크 중심이 슬롯 중심에 오도록 보정한다.

        slot.visualRoot->Get_Transform()->Set_Position(slotPosition);
        slot.visualRoot->Refresh_OpenBaseTransform();
    }

    slot.visualRoot->Close();

    return S_OK;
}

HRESULT Pictos_Panel::Ensure_TooltipLoaded(const GameDataTable::PictosDesc& pictosDesc)
{
    if (_tooltipRootsById.contains(pictosDesc.id))
        return S_OK;

    umap<wstring, Shared<UI_Base>>& tooltipUiByName = _tooltipUiById[pictosDesc.id];

    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = _levelIndex;
    desc.jsonPath = Build_PictosTooltipJsonPath(pictosDesc.tooltipUi);
    desc.prototypeTagPrefix = L"PictosTooltip_" + String::ToWString(pictosDesc.id);
    desc.outUIByName = &tooltipUiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    Shared<UI_Base> tooltipRoot = Find_UI(tooltipUiByName, Build_PictosTooltipRootName(pictosDesc.id));
    CHECK_NULL(tooltipRoot, E_FAIL);
    CHECK_NULL(_root, E_FAIL);

    tooltipRoot->Set_Parent(_root);

    if (tooltipRoot->Get_Transform())
    {
        tooltipRoot->Get_Transform()->Set_Position(_tooltipPosition);
        tooltipRoot->Refresh_OpenBaseTransform();
    }

    Set_UIMapOpenState(tooltipUiByName, false); // 첫 로드 직후 tooltip 문서 전체를 닫아 두어 초기 노출을 막는다.
    _tooltipRootsById[pictosDesc.id] = tooltipRoot;

    return S_OK;
}

void Pictos_Panel::Clear_EquippedSlotVisual(size_t slotIndex)
{
    if (slotIndex >= _equippedSlots.size())
        return;

    EquippedSlotUI& slot = _equippedSlots[slotIndex];

    if (slot.visualRoot)
        slot.visualRoot->Close();

    slot.visualUiByName.clear();
    slot.visualRoot.reset();
    slot.visualButton.reset();
    slot.pictosId.clear();

    if (slot.selectButton)
        slot.selectButton->Close();

    if (slot.emptyButton)
        slot.emptyButton->Open();
}

void Pictos_Panel::Clear_AvailableSlot(size_t slotIndex)
{
    if (slotIndex >= _availableSlots.size())
        return;

    AvailablePictosSlotUI& slot = _availableSlots[slotIndex];
    slot.pictosId.clear();

    if (slot.nameText)
        slot.nameText->Set_Text(L"");

    if (slot.markRoot)
        slot.markRoot->Close();

    if (slot.root)
        slot.root->Close();
}

void Pictos_Panel::Clear_AvailableRow(size_t rowIndex)
{
    if (rowIndex >= _availableRows.size())
        return;

    AvailablePictosRowUI& row = _availableRows[rowIndex];

    if (row.root)
        row.root->Close();

    row.uiByName.clear();
    row.root.reset();
    row.button.reset();
    row.pictosId.clear();
    row.wasHovered = false;
}

void Pictos_Panel::Close_BuiltinAvailableRoots()
{
    for (const Shared<UI_Base>& builtinRoot : _builtinAvailableRoots)
    {
        if (builtinRoot)
            builtinRoot->Close();
    }
}

HeroId Pictos_Panel::Resolve_DefaultHeroId()
{
    const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
    if (!partyEntries.empty())
        return partyEntries.front().heroId;

    return HeroId::Lune;
}

Vec2 Pictos_Panel::Get_AvailableRowPosition(size_t rowIndex) const
{
    if (rowIndex < _builtinAvailableRoots.size())
    {
        const Shared<UI_Base>& builtinRoot = _builtinAvailableRoots[rowIndex];
        if (builtinRoot && builtinRoot->Get_Transform())
            return builtinRoot->Get_Transform()->Get_Position(); // authoring된 6개 슬롯 위치를 그대로 재사용한다.
    }

    constexpr float startX = 235.f;
    constexpr float startY = 150.f;
    constexpr float columnGap = 285.f;
    constexpr float rowGap = 100.f;

    return Vec2(
        startX + static_cast<float>(rowIndex % 2) * columnGap,
        startY + static_cast<float>(rowIndex / 2) * rowGap);
}

Unique<Pictos_Panel> Pictos_Panel::Create(uint32 levelIndex)
{
    auto instance = make_unique<Pictos_Panel>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : Pictos_Panel");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void Pictos_Panel::Free()
{
    for (size_t i = 0; i < _availableRows.size(); ++i)
        Clear_AvailableRow(i);

    for (size_t i = 0; i < _equippedSlots.size(); ++i)
        Clear_EquippedSlotVisual(i);

    _tooltipRootsById.clear();
    _tooltipUiById.clear();

    _availableListRoot.reset();
    _selectSlotHintRoot.reset();
    _equipHintRoot.reset();

    UI_RuntimePanelBase::Free();
}
