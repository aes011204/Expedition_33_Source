#pragma once

#include "Client_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

class IRuntimeUI_EditorTarget;

enum class RuntimeUIEntryKind
{
    UI,
    WorldPlacement,
};

struct RuntimeWorldUIPlacementTuning
{
    Vec3 screenPosition = Vec3::Zero;
    Vec3 baseOffset = Vec3::Zero;
    Vec3 qOffset = Vec3::Zero;
    Vec3 wOffset = Vec3::Zero;
    Vec3 eOffset = Vec3::Zero;
    Vec2 worldSize = Vec2::Zero;
    float visualScale = 1.f;
    float pitchDegrees = 0.f;
    float rollDegrees = 0.f;
    float yawOffsetDegrees = 0.f;
};

class IRuntimeWorldUI_EditorTarget
{
public:
    virtual ~IRuntimeWorldUI_EditorTarget() = default;

    virtual HeroId Get_EditorWorldPlacementHeroId() const { return HeroId::END; }
    virtual void Set_EditorWorldPlacementHeroId(HeroId) {}
    virtual RuntimeWorldUIPlacementTuning Get_EditorWorldPlacementTuning() const = 0;
    virtual void Set_EditorWorldPlacementTuning(
        const RuntimeWorldUIPlacementTuning& tuning,
        bool saveImmediately) = 0;
    virtual void Reload_EditorWorldPlacementTuning() = 0;
    virtual void Reset_EditorWorldPlacementTuning() = 0;
};

enum class GameplayMenu
{
    None,
    SavePoint,
    CharacterStatus,
    SkillTree,
    WeaponUpgrade,
    CharacterProgression,   // Tab 루트 메뉴
    ChangeSkill,            // 스킬 변경
    Wardrobe,               // 옷장
    

    END
};

struct RuntimeUIEntry
{
    string category{};
    string name{};
    Shared<UI_Base> ui{};                            // 하이어라키 선택
    IRuntimeUI_EditorTarget* editorTarget = nullptr; // 인스팩터가 소유자 찾을 포인터
    bool positionDrivenByCode = false;
    RuntimeUIEntryKind kind = RuntimeUIEntryKind::UI;
    IRuntimeWorldUI_EditorTarget* worldPlacementTarget = nullptr;
};

NS_END
