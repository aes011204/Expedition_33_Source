#include "UI_SL_HUD.h"

#include "Client_Constants.h"
#include "ClientInstance.h"
#include "GameDataTable.h"
#include "PlayerStatCom.h"
#include "ResourceLoader.h"
#include "SL_Maelle.h"
#include "UI_Base.h"
#include "UI_Image.h"
#include "UI_RuntimeLoader.h"
#include "UI_Text.h"
#include "UI_Utils.h"

using namespace UI_Utils;

HRESULT UI_SL_HUD::Initialize(uint32 levelIndex)
{
    _levelIndex = levelIndex;

    CHECK_FAILED(Load_UI(levelIndex), E_FAIL);
    CHECK_FAILED(Bind_UI(), E_FAIL);

    Clear_PlayerHUD();
    return S_OK;
}

void UI_SL_HUD::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void UI_SL_HUD::Update_PlayerHUD(const Shared<SL_Maelle>& player)
{
    if (player == nullptr)
    {
        Clear_PlayerHUD();
        return;
    }

    const Shared<PlayerStatCom> playerStat =
        dynamic_pointer_cast<PlayerStatCom>(player->Get_Stat());

    if (playerStat == nullptr)
    {
        Clear_PlayerHUD();
        return;
    }

    Open();

    Apply_PlayerHp(
        playerStat->Get_CurHp(),
        playerStat->Get_StatInfo().maxHp);

    Apply_SkillIcons(playerStat);
}

void UI_SL_HUD::Clear_PlayerHUD()
{
    Close();

    Apply_PlayerHp(0.f, 1.f);

    for (uint32 i = 0; i < _skillIcons.size(); ++i)
        Clear_SkillSlot(i);
}

HRESULT UI_SL_HUD::Load_UI(uint32 levelIndex)
{
    UI_RuntimeLoader::UI_LoadDesc desc{};
    desc.levelIndex = levelIndex;
    desc.jsonPath = Constants::UI::UI_SL_HUD_Path;
    desc.prototypeTagPrefix = L"SLHUD";
    desc.outUIByName = &_uiByName;

    CHECK_FAILED(UI_RuntimeLoader::Load_Document(desc), E_FAIL);

    _root = Find_UI(_uiByName, L"Panel_SL_HUD");
    CHECK_NULL(_root, E_FAIL);

    return S_OK;
}

HRESULT UI_SL_HUD::Bind_UI()
{
    _playerHpGauge = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Character_HP_Gauge");
    _playerCurrentHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Character_Current_HP");
    _playerCurrentHpBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Character_Current_HP_BG");
    _playerMaxHpText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Character_Max_HP");
    _playerMaxHpBgText = Find_TypedUI<UI_Text>(_uiByName, L"Text_SL_Character_Max_HP_BG");

    CHECK_NULL(_playerHpGauge, E_FAIL);
    CHECK_NULL(_playerCurrentHpText, E_FAIL);
    CHECK_NULL(_playerCurrentHpBgText, E_FAIL);
    CHECK_NULL(_playerMaxHpText, E_FAIL);
    CHECK_NULL(_playerMaxHpBgText, E_FAIL);

    _playerHpGauge->Set_UseFillRatio(true);
    _playerHpGauge->Set_FillOrigin(UI_Image::FillOrigin::LeftToRight);

    _skillIcons[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_First_Skill_Icon");
    _skillIcons[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Second_Skill_Icon");
    _skillIcons[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Third_Skill_Icon");
    _skillIcons[3] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Fourth_Skill_Icon");

    _skillIconBgs[0] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_First_Skill_BG");
    _skillIconBgs[1] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Second_Skill_BG");
    _skillIconBgs[2] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Third_Skill_Icon_BG");
    _skillIconBgs[3] = Find_TypedUI<UI_Image>(_uiByName, L"Image_SL_Fourth_Skill_Icon_BG");

    for (uint32 i = 0; i < _skillIcons.size(); ++i)
    {
        CHECK_NULL(_skillIcons[i], E_FAIL);
        CHECK_NULL(_skillIconBgs[i], E_FAIL);
    }

    _staminaRoot = Find_UI(_uiByName, L"Panel_SL_Character_Stamina_Bar");
    _potionRoot = Find_UI(_uiByName, L"Panel_SL_Potion");

    if (_staminaRoot)
        _staminaRoot->Close(); // v1 has no stamina owner yet, so hide the JSON placeholder.

    return S_OK;
}

void UI_SL_HUD::Apply_PlayerHp(float curHp, float maxHp)
{
    maxHp = max(maxHp, 1.f);
    curHp = clamp(curHp, 0.f, maxHp);

    if (_playerHpGauge)
        _playerHpGauge->Set_FillRatio(curHp / maxHp);

    const wstring curText = to_wstring(static_cast<uint32>(curHp));
    const wstring maxText = to_wstring(static_cast<uint32>(maxHp));

    if (_playerCurrentHpText)
        _playerCurrentHpText->Set_Text(curText);

    if (_playerCurrentHpBgText)
        _playerCurrentHpBgText->Set_Text(curText);

    if (_playerMaxHpText)
        _playerMaxHpText->Set_Text(maxText);

    if (_playerMaxHpBgText)
        _playerMaxHpBgText->Set_Text(maxText);
}

void UI_SL_HUD::Apply_SkillIcons(const Shared<PlayerStatCom>& playerStat)
{
    const vector<string>* skillKeys = playerStat ? &playerStat->Get_SkillKeys() : nullptr;

    for (uint32 i = 0; i < _skillIcons.size(); ++i)
    {
        if (skillKeys == nullptr || i >= skillKeys->size() || (*skillKeys)[i].empty())
        {
            Clear_SkillSlot(i);
            continue;
        }

        const wstring portraitPath = Resolve_SkillPortraitTexturePath((*skillKeys)[i]);
        if (portraitPath.empty())
        {
            Clear_SkillSlot(i);
            continue;
        }

        if (_skillIcons[i] && SUCCEEDED(_skillIcons[i]->Set_TexturePath(portraitPath)))
            _skillIcons[i]->Open();

        if (_skillIconBgs[i] && SUCCEEDED(_skillIconBgs[i]->Set_TexturePath(portraitPath)))
            _skillIconBgs[i]->Open();
    }
}

void UI_SL_HUD::Clear_SkillSlot(uint32 slotIndex)
{
    if (slotIndex >= _skillIcons.size())
        return;

    if (_skillIcons[slotIndex])
        _skillIcons[slotIndex]->Close();

    if (_skillIconBgs[slotIndex])
        _skillIconBgs[slotIndex]->Close();
}

wstring UI_SL_HUD::Resolve_SkillPortraitTexturePath(const string& skillKey) const
{
    const GameDataTable::SkillTreeDesc* skillTreeDesc = CLIENT->Get_SkillTreeDesc(skillKey);
    if (skillTreeDesc == nullptr || skillTreeDesc->portraitTextureName.empty())
        return L"";

    string portraitName = skillTreeDesc->portraitTextureName;
    replace(portraitName.begin(), portraitName.end(), '\\', '/');

    if (fs::path(String::ToWString(portraitName)).extension().empty())
        portraitName += ".png";

    if (portraitName.find('/') != string::npos)
        return ResourceLoader::Resolve_RuntimePath(String::ToWString(portraitName));

    const wstring path =
        L"../Bin/Resources/Textures/UI/Progression/Skills/Maelle/" +
        String::ToWString(portraitName);

    return ResourceLoader::Resolve_RuntimePath(path);
}

Unique<UI_SL_HUD> UI_SL_HUD::Create(uint32 levelIndex)
{
    auto instance = make_unique<UI_SL_HUD>();

    if (FAILED(instance->Initialize(levelIndex)))
    {
        LOG_ERROR("Failed to Created : UI_SL_HUD");
        instance->Free();
        return nullptr;
    }

    return instance;
}

void UI_SL_HUD::Free()
{
    Close();

    _root.reset();
    _uiByName.clear();

    _playerHpGauge.reset();
    _playerCurrentHpText.reset();
    _playerCurrentHpBgText.reset();
    _playerMaxHpText.reset();
    _playerMaxHpBgText.reset();

    for (Shared<UI_Image>& icon : _skillIcons)
        icon.reset();

    for (Shared<UI_Image>& iconBg : _skillIconBgs)
        iconBg.reset();

    _staminaRoot.reset();
    _potionRoot.reset();

    Base::Free();
}
