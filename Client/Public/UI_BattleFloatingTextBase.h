#pragma once

#include "UI_RuntimePanelBase.h"
#include "Battle_Enum.h"

NS_BEGIN(Client)

class UI_BattleFloatingTextBase abstract : public UI_RuntimePanelBase
{
protected:
    enum class AnchorMode
    {
        SnapshotScreen, // 처음 위치만 받아오기
        FollowWorld     // 따라오기
    };

public:
    UI_BattleFloatingTextBase() = default;
    ~UI_BattleFloatingTextBase() override = default;

public:
    void Update(float timeDelta) override;
    void Close() override;

    bool  Is_Playing() const { return _isPlaying; }
    float Get_Elapsed() const { return _elapsed; }
    bool  Is_PlayingForTarget(const BattleChar& target) const
    {
        return _isPlaying && _hasTarget && target.actor != nullptr && _target.actor.get() == target.actor.get();
    }

protected:
    bool Begin_FloatingText(const BattleChar& target, uint32 stackIndex);
    bool Refresh_SnapshotScreenPosition();

    void Set_FloatingMotion(float lifeTime, float riseDistance);
    void Set_StackStep(float horizontalOffset, float verticalOffset);

    // projection 전에 월드 기준점을 조절. 카메라 거리/FOV 변화에 자연스럽게 반응하는 높이 보정
    void Set_WorldAnchorOffset(const Vec3& worldAnchorOffset) { _worldAnchorOffset = worldAnchorOffset; }

    // projection 이후 화면 좌표 오프셋 세팅
    void Set_ScreenBaseOffset(const Vec2& screenBaseOffset) { _screenBaseOffset = screenBaseOffset; }
    void Set_WorldAnchorRandomYRange(float minY, float maxY) { _worldAnchorRandomMinY = minY; _worldAnchorRandomMaxY = maxY; }
    void Set_AnchorMode(AnchorMode anchorMode) { _anchorMode = anchorMode; }

protected:
    bool Is_ValidTarget(const BattleChar& target) const;

    // Part_Body 위치를 우선 쓰고 없으면 actor root
    virtual bool Try_GetFeedbackWorldPosition(const BattleChar& target, Vec3& outWorldPos) const;
    virtual Vec2 Resolve_ScreenBaseOffset(const Vec3& worldPos) const { return _screenBaseOffset; }

private:
    // world anchor를 screenPos로 투영하고 원본 BaseOffset, editor offset, stack/rise를 적용
    bool Try_ResolveScreenPosition(float t, Vec2& outScreenPos) const;

    // 동시에 뜬 숫자끼리 겹치지 않도록 기존 좌우/상하 분산 규칙을 유지한다.
    void Apply_StackOffset(Vec2& screenPos) const;
    void Apply_RiseOffset(Vec2& screenPos, float t) const;

protected:
    BattleChar  _target{};          
    bool        _hasTarget = false; 
    Vec2        _snapshotScreenPos = Vec2::Zero;

    bool        _isPlaying = false;
    float       _elapsed = 0.f;
    float       _lifeTime = 1.f;
    float       _riseDistance = 32.f;

    uint32      _stackIndex = 0;
    float       _stackHorizontalOffset = 18.f;
    float       _stackVerticalOffset = 22.f;

    Vec3        _worldAnchorOffset = Vec3::Zero;       // projection 전 월드 기준점 보정값
    Vec2        _screenBaseOffset = Vec2(0.f, -10.f);  // projection 후 미세 보정값
    float       _worldAnchorRandomMinY = 0.f;
    float       _worldAnchorRandomMaxY = 0.f;
    float       _worldAnchorRandomY = 0.f;
    AnchorMode  _anchorMode = AnchorMode::SnapshotScreen;

public:
    void Free() override;
};

NS_END
