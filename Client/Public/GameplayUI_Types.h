#pragma once

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class IRuntimeUI_EditorTarget;

enum class GameplayMenu
{
    None,
    SavePoint,
    CharacterStatus,
    SkillTree,
    WeaponUpgrade,
};

struct RuntimeUIEntry
{
    string category{};
    string name{};
    Shared<UI_Base> ui{};                            // 하이어라키 선택
    IRuntimeUI_EditorTarget* editorTarget = nullptr; // 인스팩터가 소유자 찾을 포인터
    bool positionDrivenByCode = false;
};

NS_END
