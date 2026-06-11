#pragma once

#include "Battle_Enum.h"

NS_BEGIN(Engine)
class UI_Base;
NS_END

NS_BEGIN(Client)

// MainEditor가 어떤 런타임 UI든 같은 방식으로 오프셋을 읽고 쓰기 위한 공통 인터페이스
class IRuntimeUI_EditorTarget
{
public:
    virtual ~IRuntimeUI_EditorTarget() = default;

public:
    // Inspector와 Hierarchy가 붙을 실제 루트 UI
    virtual Shared<UI_Base> Get_EditorRootUI() const = 0;

    // 최종 Position을 매 프레임 덮어쓰는 UI인지 알려준다.
    virtual bool Is_EditorPositionDrivenByCode() const = 0;

    // 모든 런타임 UI가 공통으로 가지는 기본 테스트 오프셋이다.
    virtual Vec2 Get_EditorDefaultTestOffset() const = 0;
    virtual void Set_EditorDefaultTestOffset(const Vec2& offset) = 0;

    // 일반 UI는 Inspector에서 값을 바꾸면 바로 루트 Position에 반영해야 한다.
    virtual void Apply_EditorDefaultOffsetToRoot() = 0;

    // BattleCommandHUD 같은 UI만 Hero별로 오프셋 값을 따로주기 위한 확장용
    virtual bool Supports_EditorHeroTestOffset() const { return false; }
    virtual HeroId Get_EditorActiveHeroId() const { return HeroId::END; }
    virtual Vec2 Get_EditorHeroTestOffset(HeroId heroId) const { return Vec2::Zero; }
    virtual void Set_EditorHeroTestOffset(HeroId heroId, const Vec2& offset) { UNREFERENCED_PARAMETER(heroId); UNREFERENCED_PARAMETER(offset); }

    virtual bool Supports_EditorBattleWorldUIPlacement() const { return false; }
    virtual Vec2 Get_EditorBattleWorldUISize() const { return Vec2::Zero; }
    virtual void Set_EditorBattleWorldUISize(const Vec2& size) { UNREFERENCED_PARAMETER(size); }
    virtual Vec3 Get_EditorBattleWorldUIOffset() const { return Vec3::Zero; }
    virtual void Set_EditorBattleWorldUIOffset(const Vec3& offset) { UNREFERENCED_PARAMETER(offset); }
    virtual Vec3 Get_EditorBattleWorldUIRotation() const { return Vec3::Zero; }
    virtual void Set_EditorBattleWorldUIRotation(const Vec3& rotation) { UNREFERENCED_PARAMETER(rotation); }
    virtual uint64 Get_EditorRuntimeTuningRevision() const { return 0; }
};
NS_END
