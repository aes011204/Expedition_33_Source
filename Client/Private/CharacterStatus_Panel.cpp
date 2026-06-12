#include "CharacterStatus_Panel.h"
#include "ClientInstance.h"
#include "Client_Constants.h"
#include "CombatStatCom.h"
#include "EffectCom.h"
#include "GameInstance.h"
#include "LightingPipeline.h"
#include "Player_WorldBase.h"
#include "PlayerStatCom.h"
#include "PostProcessPipeline.h"
#include "UI_Base.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UIPreviewManager.h"
#include "UI_Utils.h"
#include "UI_Button.h"

using namespace UI_Utils;

namespace
{
	constexpr std::array<HeroId, 3> characterStatusOrder =
	{
		HeroId::Lune,
		HeroId::Gustave,
		HeroId::Maelle,
	};

	HeroId Get_SelectedCharacterStatusHeroId(uint32 selectedHeroIndex)
	{
		if (selectedHeroIndex >= characterStatusOrder.size())
			return HeroId::Gustave;

		return characterStatusOrder[selectedHeroIndex];
	}

	uint32 Get_CharacterStatusIndex(HeroId heroId)
	{
		for (uint32 i = 0; i < characterStatusOrder.size(); ++i)
		{
			if (characterStatusOrder[i] == heroId)
				return i;
		}

		return 0;
	}

	size_t To_StatIndex(StatType type)
	{
		return static_cast<size_t>(type);
	}
}

HRESULT CharacterStatus_Panel::Initialize(uint32 levelIndex)
{
	_levelIndex = levelIndex;

	CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
	CHECK_FAILED(Bind_UI(), E_FAIL);

	Close();

	return S_OK;
}

void CharacterStatus_Panel::Update(float timeDelta)
{
	UNREFERENCED_PARAMETER(timeDelta);

	if (!_isOpen)
		return;

	Handle_SelectionInput(); // Q,E는 캐릭터 선택
	Handle_StatInput();
}

void CharacterStatus_Panel::Open()
{
	const bool wasOpen = Is_Open();

	UI_RuntimePanelBase::Open();

	if (!wasOpen)
		GAME->Play_SoundCue(Constants::SoundCue::UI_Global_GenericEnter);

	Clear_PendingStats(); // 임시 저장값 비우기

    CLIENT->Begin_Preview(PreviewShotPanel::CharacterStatus);

    auto previewLayout = Build_PreviewLayout();
    CLIENT->Apply_PreviewLayout(previewLayout);
    CLIENT->Show_Preview(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex), PreviewShotPanel::CharacterStatus);
    Apply_PreviewOverrides();
	//const HeroId activeHeroId = CLIENT->Get_HeroId(CLIENT->Get_ActiveHeroSlot());
	//_selectedHeroIndex = Get_CharacterStatusIndex(activeHeroId);

	//CLIENT->Begin_Preview(PreviewShotTable::PreviewShotPanel::CharacterStatus);

	//auto previewLayout = Build_PreviewLayout();
	//CLIENT->Apply_PreviewLayout(previewLayout);
	//CLIENT->Show_Preview(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex), PreviewShotTable::PreviewShotPanel::CharacterStatus);

	Refresh_CombatStats();
	Refresh_CharacterSelectUI();

	GAME->Unlock_Mouse();
	GAME->Set_GameplayPaused(true);
}

void CharacterStatus_Panel::Close()
{
	UI_RuntimePanelBase::Close();

	Clear_PendingStats();
    Restore_PreviewOverrides();

	CLIENT->Hide_Preview();

	GAME->Set_GameplayPaused(false);
	GAME->Lock_Mouse();
}

void CharacterStatus_Panel::Toggle()
{
	UI_RuntimePanelBase::Toggle();

}

void CharacterStatus_Panel::Handle_SelectionInput()
{
	const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
	if (partyEntries.empty())
		return;

	const uint32 heroCount = static_cast<uint32>(partyEntries.size());
	uint32 previousIndex = _selectedHeroIndex;

	if (GAME->KeyDown(KEY_TYPE::Q))
	{
		_selectedHeroIndex = (_selectedHeroIndex + heroCount - 1) % heroCount;
	}
	if (GAME->KeyDown(KEY_TYPE::E))
	{
		_selectedHeroIndex = (_selectedHeroIndex + 1) % heroCount;
	}

	if (previousIndex != _selectedHeroIndex)
	{
		GAME->Play_SoundCue(Constants::SoundCue::UI_Battle_SelectAlly);

		Refresh_CombatStats();
		Refresh_CharacterSelectUI();

		auto previewLayout = Build_PreviewLayout();
		CLIENT->Apply_PreviewLayout(previewLayout);
		CLIENT->Show_Preview(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex), PreviewShotPanel::CharacterStatus);
        // Update_PreviewHeroVisibility();
        Apply_PreviewOverrides();
	}
}


void CharacterStatus_Panel::Handle_StatInput()
{
	bool needsRefresh = false;

    const auto TryAdjustPending = [&](const Shared<UI_Button>& button, StatType type, int32 delta)
    {
		if (!button || !button->Was_Clicked())
			return;

		GAME->Play_SoundCue(Constants::SoundCue::UI_Global_NavigateClick);

        if (Adjust_PendingStat(type, delta))
            needsRefresh = true; // draft가 바뀐 경우만 화면 갱신
    };

	TryAdjustPending(_statDecreaseButtons[To_StatIndex(StatType::Vitality)], StatType::Vitality, -1);
    TryAdjustPending(_statIncreaseButtons[To_StatIndex(StatType::Vitality)], StatType::Vitality, 1);

    TryAdjustPending(_statDecreaseButtons[To_StatIndex(StatType::Might)], StatType::Might, -1);
    TryAdjustPending(_statIncreaseButtons[To_StatIndex(StatType::Might)], StatType::Might, 1);

    TryAdjustPending(_statDecreaseButtons[To_StatIndex(StatType::Agility)], StatType::Agility, -1);
    TryAdjustPending(_statIncreaseButtons[To_StatIndex(StatType::Agility)], StatType::Agility, 1);

    TryAdjustPending(_statDecreaseButtons[To_StatIndex(StatType::Defense)], StatType::Defense, -1);
    TryAdjustPending(_statIncreaseButtons[To_StatIndex(StatType::Defense)], StatType::Defense, 1);

    TryAdjustPending(_statDecreaseButtons[To_StatIndex(StatType::Luck)], StatType::Luck, -1);
    TryAdjustPending(_statIncreaseButtons[To_StatIndex(StatType::Luck)], StatType::Luck, 1);

	if (_statusUpgradeButton && _statusUpgradeButton->Was_Clicked())
    {
        Apply_PendingStats(); // F
        return;
    }

    if (_statusResetButton && _statusResetButton->Was_Clicked())
    {
        Reset_CurrentHeroStats(); // R
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::F))
    {
        Apply_PendingStats(); // 스탯 적용
        return;
    }

    if (GAME->KeyDown(KEY_TYPE::R))
    {
        Reset_CurrentHeroStats(); // stat reset
        return;
    }

    if (needsRefresh)
        Refresh_CombatStats();
}

uint32 CharacterStatus_Panel::Get_WrappedPartyIndex(int32 index) const
{
	const vector<HeroPartyEntry>& partyEntries = CLIENT->Get_HeroEntries();
	const int32 heroCount = static_cast<int32>(partyEntries.size());

	if (heroCount <= 0)
		return 0;

	int32 wrappedIndex = index % heroCount;
	if (wrappedIndex < 0)
		wrappedIndex += heroCount;

	return static_cast<uint32>(wrappedIndex);
}

HRESULT CharacterStatus_Panel::Load_UI(uint32 levelIndex)
{
	UI_RuntimeLoader::UI_LoadDesc desc{};
	desc.levelIndex = levelIndex;
	desc.jsonPath = Constants::UI::UI_CharacterStatus_Path;
	desc.prototypeTagPrefix = L"CharacterStatus";
	desc.outUIByName = &_uiByName;

	CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

	_root = Find_UI(_uiByName, L"Panel_CharacterStatus");
	CHECK_NULL(_root, E_FAIL);

	return S_OK;
}

HRESULT CharacterStatus_Panel::Bind_UI()
{
	_levelText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLevel");
	_statPointText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatPoint");

	_vitalityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentVitality");
	_mightText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentMight");
	_agilityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentAgility");
	_defenseStatText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentDefense");
	_luckText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentLuck");

    _upgradeVitalityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_UpgradeVitality"); 
	_upgradeMightText = Find_TypedUI<UI_Text>(_uiByName, L"Text_UpgradeMight");       
	_upgradeAgilityText = Find_TypedUI<UI_Text>(_uiByName, L"Text_UpgradeAgility");   
	_upgradeDefenseText = Find_TypedUI<UI_Text>(_uiByName, L"Text_UpgradeDefense");   
	_upgradeLuckText = Find_TypedUI<UI_Text>(_uiByName, L"Text_UpgradeLuck");         


	_hpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatHP");
	_attackText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatAttack");
	_speedText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatSpeed");
	_defenseValueText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatDefense");
	_criticalRateText = Find_TypedUI<UI_Text>(_uiByName, L"Text_CurrentStatCriticalPercent");

	_selectButton01 = Find_UI(_uiByName, L"Button_CharacterSelect_01");
	_selectButton02 = Find_UI(_uiByName, L"Button_CharacterSelect_02");
	_selectButton03 = Find_UI(_uiByName, L"Button_CharacterSelect_03");

	_selectFrame01 = Find_UI(_uiByName, L"Image_CharacterSelectButton_01");
	_selectFrame02 = Find_UI(_uiByName, L"Image_CharacterSelectButton_02");
	_selectFrame03 = Find_UI(_uiByName, L"Image_CharacterSelectButton_03");

	_selectPortrait01 = Find_UI(_uiByName, L"Image_Lune");
	_selectPortrait02 = Find_UI(_uiByName, L"Image_Gustave");
	_selectPortrait03 = Find_UI(_uiByName, L"Image_Maelle");

	_selectIndicator01 = Find_UI(_uiByName, L"Image_PartyIndicator_01");
	_selectIndicator02 = Find_UI(_uiByName, L"Image_PartyIndicator_02");
	_selectIndicator03 = Find_UI(_uiByName, L"Image_PartyIndicator_03");

	_statusUpgradeButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_StatusUpgrade"); // F와 같은 의미
	_statusResetButton = Find_TypedUI<UI_Button>(_uiByName, L"Button_StatusReset");     // R과 같은 의미

	_statDecreaseButtons[To_StatIndex(StatType::Vitality)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Vitality_Upgrade_Left");
	_statIncreaseButtons[To_StatIndex(StatType::Vitality)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Vitality_Upgrade_Right");

	_statDecreaseButtons[To_StatIndex(StatType::Might)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Might_Upgrade_Left");
	_statIncreaseButtons[To_StatIndex(StatType::Might)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Might_Upgrade_Right");

	_statDecreaseButtons[To_StatIndex(StatType::Agility)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Agility_Upgrade_Left");
	_statIncreaseButtons[To_StatIndex(StatType::Agility)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Agility_Upgrade_Right");

	_statDecreaseButtons[To_StatIndex(StatType::Defense)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Defense_Upgrade_Left");
	_statIncreaseButtons[To_StatIndex(StatType::Defense)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Defense_Upgrade_Right");

	_statDecreaseButtons[To_StatIndex(StatType::Luck)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Luck_Upgrade_Left");
	_statIncreaseButtons[To_StatIndex(StatType::Luck)] = Find_TypedUI<UI_Button>(_uiByName, L"Button_Luck_Upgrade_Right");

	return S_OK;
}

void CharacterStatus_Panel::Refresh_CombatStats()
{
	Shared<Player_WorldBase> player = CLIENT->Find_Hero(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex));
	CHECK_NULL(player);

	Shared<PlayerStatCom> playerStat = player->Get_Component<PlayerStatCom>();
	CHECK_NULL(playerStat);

	const PlayerStatCom::PRuntimeCombatStat& statInfo = playerStat->Get_PlayerRuntimeStat();
	const std::array<int32, static_cast<size_t>(StatType::END)>& pending = _pendingStatDelta[_selectedHeroIndex];

	const int32 vitality = statInfo.CurVitality + pending[To_StatIndex(StatType::Vitality)];
	const int32 might = statInfo.CurMight + pending[To_StatIndex(StatType::Might)];
	const int32 agility = statInfo.CurAgility + pending[To_StatIndex(StatType::Agility)];
	const int32 defense = statInfo.CurDefense + pending[To_StatIndex(StatType::Defense)];
	const int32 luck = statInfo.CurLuck + pending[To_StatIndex(StatType::Luck)];

	const int32 usedPendingPoint =
		pending[To_StatIndex(StatType::Vitality)] +
		pending[To_StatIndex(StatType::Might)] +
		pending[To_StatIndex(StatType::Agility)] +
		pending[To_StatIndex(StatType::Defense)] +
		pending[To_StatIndex(StatType::Luck)];

	const uint32 previewStatPoint =
		(usedPendingPoint > 0 && statInfo.CurStatPoint >= static_cast<uint32>(usedPendingPoint))
		? statInfo.CurStatPoint - static_cast<uint32>(usedPendingPoint)
		: statInfo.CurStatPoint;

	const PlayerStatCom::PreviewStat preview = PlayerStatCom::Calculate_PreviewStat(
		vitality, might, agility, defense, luck);

	const float hpRate = (statInfo.CurMaxHp > 0.f) ? (statInfo.CurHp / statInfo.CurMaxHp) : 1.f;
	const float previewHp = min(preview.maxHp, preview.maxHp * hpRate); // 현재 체력 비율만 유지

	Set_UIntText(_levelText, statInfo.CurLevel);
	Set_UIntText(_statPointText, previewStatPoint); // 아직 저장 안 한 pending만큼 미리 차감 표시

    // 왼쪽 현재값
	Set_IntText(_vitalityText, statInfo.CurVitality);
	Set_IntText(_mightText, statInfo.CurMight);
	Set_IntText(_agilityText, statInfo.CurAgility);
	Set_IntText(_defenseStatText, statInfo.CurDefense);
	Set_IntText(_luckText, statInfo.CurLuck);

    // 오른쪽 펜딩값
    Set_IntText(_upgradeVitalityText, vitality);          
	Set_IntText(_upgradeMightText, might);                
	Set_IntText(_upgradeAgilityText, agility);            
	Set_IntText(_upgradeDefenseText, defense);            
	Set_IntText(_upgradeLuckText, luck);                  

	Set_HpText(_hpText, previewHp, preview.maxHp);
	Set_IntText(_attackText, static_cast<int32>(preview.physicalAttack));
	Set_IntText(_speedText, static_cast<int32>(preview.speed));
	Set_IntText(_defenseValueText, static_cast<int32>(preview.physicalDefense));
	Set_PercentText(_criticalRateText, preview.criticalRate);
}

void CharacterStatus_Panel::Refresh_CharacterSelectUI()
{
	static const Vec2 kSelectedScale = Vec2(1.4f, 1.4f);
	static const Vec2 kNormalScale = Vec2(1.f, 1.f);

	Shared<UI_Base> buttons[3] =
	{
		_selectButton01, _selectButton02, _selectButton03,
	};

	Shared<UI_Base> frames[3] =
	{
		_selectFrame01, _selectFrame02, _selectFrame03,
	};

	Shared<UI_Base> portraits[3] =
	{
		_selectPortrait01, _selectPortrait02, _selectPortrait03,
	};

	Shared<UI_Base> indicators[3] =
	{
		_selectIndicator01, _selectIndicator02, _selectIndicator03,
	};

	for (uint32 i = 0; i < 3; i++)
	{
		const bool isSelected = (i == _selectedHeroIndex);
		const Vec2 targetScale = isSelected ? kSelectedScale : kNormalScale;

		if (buttons[i] && buttons[i]->Get_Transform())
			buttons[i]->Get_Transform()->Set_Scale(targetScale);

		if (frames[i] && frames[i]->Get_Transform())
			frames[i]->Get_Transform()->Set_Scale(targetScale);

		if (portraits[i] && portraits[i]->Get_Transform())
			portraits[i]->Get_Transform()->Set_Scale(targetScale);

		if (indicators[i] && indicators[i]->Get_Transform())
			indicators[i]->Get_Transform()->Set_Scale(targetScale);
	}
}

bool CharacterStatus_Panel::Adjust_PendingStat(StatType type, int32 value)
{
    if (value == 0)
        return false;

	HeroStatPending& pending = _pendingStatDelta[_selectedHeroIndex];
	int32& pendingValue = pending[To_StatIndex(type)];

	if (value < 0)
    {
        if (pendingValue <= 0)
            return false; // 저장 전 증가한 만큼만 되돌릴 수 있음

        pendingValue += value;
        return true;
    }

	Shared<Player_WorldBase> player = CLIENT->Find_Hero(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex));
    CHECK_NULL(player, false);

	Shared<PlayerStatCom> playerStat = player->Get_Component<PlayerStatCom>();
    CHECK_NULL(playerStat, false);

	const PlayerStatCom::PRuntimeCombatStat& statInfo = playerStat->Get_PlayerRuntimeStat();
	const int32 usedPendingPoint =
		pending[To_StatIndex(StatType::Vitality)] +
		pending[To_StatIndex(StatType::Might)] +
		pending[To_StatIndex(StatType::Agility)] +
		pending[To_StatIndex(StatType::Defense)] +
		pending[To_StatIndex(StatType::Luck)];

	if (statInfo.CurStatPoint <= static_cast<uint32>(usedPendingPoint))
        return false; // 남은 포인트가 없으면 더 못 올림

    pendingValue += value;
    return true;
}

void CharacterStatus_Panel::Apply_PendingStats()
{
	Shared<Player_WorldBase> player = CLIENT->Find_Hero(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex));
    CHECK_NULL(player);

    Shared<PlayerStatCom> playerStat = player->Get_Component<PlayerStatCom>();
    CHECK_NULL(playerStat);

    std::array<int32, static_cast<size_t>(StatType::END)>& pending = _pendingStatDelta[_selectedHeroIndex];

    const StatType statTypes[] =
    {
        StatType::Vitality,
        StatType::Might,
        StatType::Agility,
        StatType::Defense,
        StatType::Luck,
    };

    for (StatType type : statTypes)
    {
        int32& pendingValue = pending[To_StatIndex(type)];

        while (pendingValue > 0)
        {
            if (!playerStat->Add_Stat(type))
                break; // runtime 반영 실패 시 여기서 중단

            --pendingValue; // 실제 반영된 수만큼만 pending 차감
        }
    }

    Refresh_CombatStats();
}

void CharacterStatus_Panel::Reset_CurrentHeroStats()
{
	Shared<Player_WorldBase> player = CLIENT->Find_Hero(Get_SelectedCharacterStatusHeroId(_selectedHeroIndex));
    CHECK_NULL(player);

    Shared<PlayerStatCom> playerStat = player->Get_Component<PlayerStatCom>();
    CHECK_NULL(playerStat);

    if (!playerStat->Reset_Stat())
        return; // respecCount가 없으면 아무것도 하지 않음

    _pendingStatDelta[_selectedHeroIndex].fill(0); // 실제 초기화 성공 후 draft도 비움
    Refresh_CombatStats();
}

void CharacterStatus_Panel::Clear_PendingStats()
{
	for (auto& pending : _pendingStatDelta)
        pending.fill(0); // 0으로 채우기
}

void CharacterStatus_Panel::Set_IntText(const Shared<UI_Text>& text, int32 value)
{
	if (text)
		text->Set_Text(to_wstring(value));
}

void CharacterStatus_Panel::Set_UIntText(const Shared<UI_Text>& text, uint32 value)
{
	if (text)
		text->Set_Text(to_wstring(value));
}

void CharacterStatus_Panel::Set_PercentText(const Shared<UI_Text>& text, float value)
{
	if (text)
		text->Set_Text(to_wstring(static_cast<int32>(value)));
}

void CharacterStatus_Panel::Set_HpText(const Shared<UI_Text>& text, float hp, float maxHp)
{
	if (text == nullptr)
		return;

	const int32 hpValue = static_cast<int32>(hp);
	const int32 maxHpValue = static_cast<int32>(maxHp);

	text->Set_Text(to_wstring(hpValue) + L" / " + to_wstring(maxHpValue));
}

vector<UIPreviewManager::PreviewSlotDesc> CharacterStatus_Panel::Build_PreviewLayout()
{
	static const Vec3 positions[] =
	{
		Vec3(1.1f,  0.f, -0.1f),
		Vec3(0.0f,  0.f, 0.f),
		Vec3(-1.1f, 0.f, -0.1f),
	};

	static const Vec3 rotations[] =
	{
		Vec3(0.f, 0.f, -0.1f),
		Vec3(0.f, 0.f, 0.f),
		Vec3(0.f, 0.f, -0.1f),
	};

	vector<UIPreviewManager::PreviewSlotDesc> layout;
	layout.reserve(characterStatusOrder.size());

	for (uint32 i = 0; i < characterStatusOrder.size(); ++i)
	{
		layout.push_back({ characterStatusOrder[i], positions[i], rotations[i] });
	}

	return layout;
}

// UI Preview용 조명과 포스트프로세스 세팅을 적용한다. 닫을 때 원래 세팅으로 돌아갈 수 있게 현재 세팅을 저장해 둔다.
void CharacterStatus_Panel::Apply_PreviewOverrides()
{
    const Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    const Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    const LIGHT_DESC* primaryLightDesc = GAME->Get_PrimaryShadowLightDesc();
    const ShadowLightDesc* shadowLightDesc = GAME->Get_ShadowLightDesc();
    if (primaryLightDesc == nullptr)
    {
        const uint32 lightCount = GAME->Get_LightCount();
        for (uint32 lightIndex = 0; lightIndex < lightCount; ++lightIndex)
        {
            const LIGHT_DESC* lightDesc = GAME->Get_LightDesc(lightIndex);
            if (lightDesc == nullptr || lightDesc->type != LightType::Directional)
                continue;

            primaryLightDesc = lightDesc;
            break;
        }
    }

    if (previewManager == nullptr)
        return;

    if (_previewOverrideState.isCaptured == false)
    {
        // 닫을 때 원래 월드 조명으로 정확히 돌아가게 한 번만 저장한다.
        _previewOverrideState.isCaptured = true;
        _previewOverrideState.cascadeLevel = GAME->Get_ShadowCascadeLevel();
        _previewOverrideState.hasPrimaryLight = (primaryLightDesc != nullptr);
        _previewOverrideState.hasShadowLight = (shadowLightDesc != nullptr);
        _previewOverrideState.hasPostProcess = (postProcessPipeline != nullptr);
        _previewOverrideState.hasLighting = (lightingPipeline != nullptr);

        if (primaryLightDesc != nullptr)
            _previewOverrideState.primaryLight = *primaryLightDesc;

        if (shadowLightDesc != nullptr)
            _previewOverrideState.shadowLight = *shadowLightDesc;

        if (postProcessPipeline != nullptr)
        {
            _previewOverrideState.postProcess = postProcessPipeline->Get_PostProcessParams();
            _previewOverrideState.postProcessFeatures = postProcessPipeline->Get_PostProcessFeatures();
        }

        if (lightingPipeline != nullptr)
        {
            _previewOverrideState.deferredLighting = lightingPipeline->Get_DeferredLightingParams();
            _previewOverrideState.iblEnabled = lightingPipeline->Is_IBLEnabled();
        }
    }

    if (_previewOverrideState.hasPrimaryLight)
    {
        LIGHT_DESC previewLight = _previewOverrideState.primaryLight;
        Vec3 previewLightDir = kPreviewLightDirection;
        if (previewLightDir.LengthSquared() > 0.0001f)
            previewLightDir.Normalize();

        previewLight.direction = Vec4(previewLightDir.x, previewLightDir.y, previewLightDir.z, 0.f);
        previewLight.diffuse = kPreviewLightDiffuse;
        previewLight.ambient = kPreviewLightAmbient;
        previewLight.specular = kPreviewLightSpecular;
        GAME->Update_PrimaryShadowLightDesc(previewLight);
        GAME->Set_ShadowCascadeLevel(Engine::CascadeLevel::Close);
    }

    if (_previewOverrideState.hasShadowLight)
    {
        ShadowLightDesc previewShadow = _previewOverrideState.shadowLight;
        previewShadow.intensity = max(previewShadow.intensity, kPreviewShadowIntensity);
        GAME->Update_ShadowLight(previewShadow);
    }

    if (_previewOverrideState.hasPostProcess && postProcessPipeline != nullptr)
    {
        Engine::PostProcessCB previewPostProcess = _previewOverrideState.postProcess;
        previewPostProcess.exposure = 0.60f;
        previewPostProcess.gamma = 1.33f;
        previewPostProcess.bloomThreshold = 0.78f;
        previewPostProcess.bloomIntensity = 0.64f;
        previewPostProcess.bloomScatter = 0.88f;
        postProcessPipeline->Set_PostProcessFeatures(
            Engine::PostProcessFeature::HDR |
            Engine::PostProcessFeature::ToneMapping |
            Engine::PostProcessFeature::Exposure |
            Engine::PostProcessFeature::Bloom |
            Engine::PostProcessFeature::Gamma);
        postProcessPipeline->Set_PostProcessParams(previewPostProcess);
    }

    if (_previewOverrideState.hasLighting && lightingPipeline != nullptr)
    {
        Engine::DeferredLightingCB previewDeferredLighting = _previewOverrideState.deferredLighting;
        previewDeferredLighting.iblIntensity = 0.64f;
        previewDeferredLighting.iblDiffuseIntensity = 0.34f;
        previewDeferredLighting.iblSpecularIntensity = 1.18f;
        lightingPipeline->Set_DeferredLightingParams(previewDeferredLighting);
        lightingPipeline->Set_IBLEnabled(true);
    }

    Apply_PreviewRimState();
}

// UI Preview용으로 변경한 조명과 포스트프로세스 세팅을 원래대로 돌려놓는다.
void CharacterStatus_Panel::Restore_PreviewOverrides()
{
    if (!_previewOverrideState.isCaptured)
        return;

    Clear_PreviewRimState();

    const Shared<PostProcessPipeline> postProcessPipeline = GAME->Get_PostProcessPipeline();
    const Shared<LightingPipeline> lightingPipeline = GAME->Get_LightingPipeline();

    if (_previewOverrideState.hasPrimaryLight)
    {
        GAME->Update_PrimaryShadowLightDesc(_previewOverrideState.primaryLight);
        GAME->Set_ShadowCascadeLevel(_previewOverrideState.cascadeLevel);
    }

    if (_previewOverrideState.hasShadowLight)
        GAME->Update_ShadowLight(_previewOverrideState.shadowLight);

    if (_previewOverrideState.hasPostProcess && postProcessPipeline != nullptr)
    {
        postProcessPipeline->Set_PostProcessFeatures(_previewOverrideState.postProcessFeatures);
        postProcessPipeline->Set_PostProcessParams(_previewOverrideState.postProcess);
    }

    if (_previewOverrideState.hasLighting && lightingPipeline != nullptr)
    {
        lightingPipeline->Set_DeferredLightingParams(_previewOverrideState.deferredLighting);
        lightingPipeline->Set_IBLEnabled(_previewOverrideState.iblEnabled);
    }

    _previewOverrideState = PreviewOverrideState{};
}

// 선택된 캐릭터에게 림 라이트 효과를 적용한다. 다른 캐릭터들은 림 라이트를 끈다.
void CharacterStatus_Panel::Apply_PreviewRimState()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        const Shared<EffectCom> effectCom = previewHero.player->Get_Component<EffectCom>();
        if (effectCom == nullptr)
            continue;

        if (previewHero.player->Is_Visible())
            effectCom->Set_PreviewRim(
                kPreviewRimColor,
                kPreviewRimIntensity,
                kPreviewRimPower,
                kPreviewHairRimIntensity);
        else
            effectCom->Clear_PreviewRim();
    }
}

 // 모든 미리보기 캐릭터에서 림 라이트 효과를 제거한다.
void CharacterStatus_Panel::Clear_PreviewRimState()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        if (const Shared<EffectCom> effectCom = previewHero.player->Get_Component<EffectCom>())
            effectCom->Clear_PreviewRim();
    }
}

 // 선택된 캐릭터는 보이게, 나머지 캐릭터는 안 보이게 설정한다. 
void CharacterStatus_Panel::Update_PreviewHeroVisibility()
{
    const Shared<UIPreviewManager> previewManager = CLIENT->Get_PreviewManager();
    if (previewManager == nullptr)
        return;

    const HeroId selectedHeroId = Get_SelectedCharacterStatusHeroId(_selectedHeroIndex);
    for (const auto& previewHero : previewManager->Get_PreviewHeroes())
    {
        if (previewHero.player == nullptr)
            continue;

        previewHero.player->Set_Visible(previewHero.heroId == selectedHeroId);
    }
}

Unique<CharacterStatus_Panel> CharacterStatus_Panel::Create(uint32 levelIndex)
{
	auto instance = make_unique<CharacterStatus_Panel>();

	if (FAILED(instance->Initialize(levelIndex)))
	{
		LOG_ERROR("Failed to Created : CharacterStatus_Panel");
		instance->Free();
		return nullptr;
	}

	return instance;
}

void CharacterStatus_Panel::Free()
{
    Restore_PreviewOverrides();

	if (_root)
        _root->Close();

    _root.reset(); 

    _levelText.reset();
    _statPointText.reset();

    _vitalityText.reset();
    _mightText.reset();
    _agilityText.reset();
    _defenseStatText.reset();
    _luckText.reset();

    _hpText.reset();
    _attackText.reset();
    _speedText.reset();
    _defenseValueText.reset();
    _criticalRateText.reset();

    _upgradeVitalityText.reset();
    _upgradeMightText.reset();
    _upgradeAgilityText.reset();
    _upgradeDefenseText.reset();
    _upgradeLuckText.reset();

    _statusUpgradeButton.reset();
    _statusResetButton.reset();
    _statDecreaseButtons.fill(nullptr);
    _statIncreaseButtons.fill(nullptr);

    _uiByName.clear();
    _isOpen = false;

    GAME->Set_GameplayPaused(false);

    Base::Free();
}
