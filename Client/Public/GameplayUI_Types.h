#pragma once

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

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
    Shared<Engine::UI_Base> ui{};
    bool positionDrivenByCode = false;
};

NS_END
