#include "UI_BattleFloatingTextBase.h"

#include "Character_BattleBase.h"
#include "TransformCom.h"
#include "UI_Base.h"
#include "UI_Utils.h"

using namespace UI_Utils;

void UI_BattleFloatingTextBase::Update(float timeDelta)
{
    if (!Is_Open() || !_isPlaying || !_hasTarget)
        return;

    if (!Is_ValidTarget(_target))
    {
        Close(); // 대상이 사라졌으면 이전 screenPos에 popup을 남기지 않는다.
        return;
    }

    _elapsed += timeDelta;

    const float safeLifeTime = max(_lifeTime, 0.0001f);
    const float t = clamp(_elapsed / safeLifeTime, 0.f, 1.f);

    Vec2 screenPos{};
    if (_anchorMode == AnchorMode::FollowWorld)
    {
        if (!Try_ResolveScreenPosition(t, screenPos))
        {
            Close(); // 카메라 밖이면 원본 popup처럼 표시를 중단한다.
            return;
        }
    }
    else
    {
        screenPos = _snapshotScreenPos;
        Apply_RiseOffset(screenPos, t);
    }

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(screenPos);

    if (_elapsed >= _lifeTime)
        Close();
}

void UI_BattleFloatingTextBase::Close()
{
    _target = {};
    _hasTarget = false;
    _isPlaying = false;
    _elapsed = 0.f;
    _stackIndex = 0;
    _snapshotScreenPos = Vec2::Zero;
    _worldAnchorRandomY = 0.f;

    UI_RuntimePanelBase::Close();
}

bool UI_BattleFloatingTextBase::Begin_FloatingText(const BattleChar& target, uint32 stackIndex)
{
    if (!Is_ValidTarget(target))
        return false;

    _target = target;
    _hasTarget = true;
    _stackIndex = stackIndex;
    _elapsed = 0.f;
    _isPlaying = true;
    _worldAnchorRandomY = GAME->Random(_worldAnchorRandomMinY, _worldAnchorRandomMaxY);

    Vec2 startPos{};
    if (!Try_ResolveScreenPosition(0.f, startPos))
    {
        Close();
        return false;
    }

    _snapshotScreenPos = startPos;

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(startPos);

    Set_EditorPositionDrivenByCode(true); // Open()이 JSON 기본 위치로 되돌리지 않게 floating text 위치 소유권을 코드에 둔다.
    UI_RuntimePanelBase::Open();
    return true;
}

bool UI_BattleFloatingTextBase::Refresh_SnapshotScreenPosition()
{
    if (_anchorMode != AnchorMode::SnapshotScreen || !_isPlaying || !_hasTarget)
        return false;

    Vec2 snapshotPos{};
    if (!Try_ResolveScreenPosition(0.f, snapshotPos))
        return false;

    _snapshotScreenPos = snapshotPos;

    const float safeLifeTime = max(_lifeTime, 0.0001f);
    const float t = clamp(_elapsed / safeLifeTime, 0.f, 1.f);
    Vec2 currentPos = _snapshotScreenPos;
    Apply_RiseOffset(currentPos, t);

    if (_root && _root->Get_Transform())
        _root->Get_Transform()->Set_Position(currentPos);

    return true;
}

void UI_BattleFloatingTextBase::Set_FloatingMotion(float lifeTime, float riseDistance)
{
    _lifeTime = lifeTime;
    _riseDistance = riseDistance;
}

void UI_BattleFloatingTextBase::Set_StackStep(float horizontalOffset, float verticalOffset)
{
    _stackHorizontalOffset = horizontalOffset;
    _stackVerticalOffset = verticalOffset;
}

bool UI_BattleFloatingTextBase::Is_ValidTarget(const BattleChar& target) const
{
    if (target.actor == nullptr)
        return false;

    return target.actor->Get_Transform() != nullptr;
}

bool UI_BattleFloatingTextBase::Try_GetFeedbackWorldPosition(const BattleChar& target, Vec3& outWorldPos) const
{
    if (!Is_ValidTarget(target))
        return false;

    if (auto* battleActor = dynamic_cast<Character_BattleBase*>(target.actor.get()))
    {
        if (battleActor->Try_GetDamageFeedbackWorldPosition(outWorldPos))
        {
            // 앵커를 받은 뒤, UI별 월드 보정과 랜덤 높이만 추가한다.
            outWorldPos += _worldAnchorOffset;
            outWorldPos.y += _worldAnchorRandomY;
            return true;
        }
    }

    outWorldPos = target.actor->Get_Transform()->Get_WorldPosition();
    outWorldPos += _worldAnchorOffset;
    outWorldPos.y += _worldAnchorRandomY;
    return true;
}

bool UI_BattleFloatingTextBase::Try_ResolveScreenPosition(float t, Vec2& outScreenPos) const
{
    Vec3 worldPos{};
    if (!Try_GetFeedbackWorldPosition(_target, worldPos))
        return false;

    if (!Try_ProjectWorldToScreen(worldPos, outScreenPos))
        return false;

    outScreenPos += Resolve_ScreenBaseOffset(worldPos);
    Apply_EditorOffsets(outScreenPos);
    Apply_StackOffset(outScreenPos);
    Apply_RiseOffset(outScreenPos, t);
    Apply_ScreenClamp(outScreenPos);
    return true;
}

void UI_BattleFloatingTextBase::Apply_StackOffset(Vec2& screenPos) const
{
    float horizontal = 0.f;

    if (_stackIndex > 0)
        horizontal = (_stackIndex % 2 == 1) ? _stackHorizontalOffset : -_stackHorizontalOffset;

    const float vertical = -_stackVerticalOffset * static_cast<float>(_stackIndex);

    screenPos.x += horizontal;
    screenPos.y += vertical;
}

void UI_BattleFloatingTextBase::Apply_RiseOffset(Vec2& screenPos, float t) const
{
    const float eased = 1.f - (1.f - t) * (1.f - t);
    screenPos.y -= _riseDistance * eased;
}

void UI_BattleFloatingTextBase::Apply_ScreenClamp(Vec2& screenPos) const
{
    if (!_screenClampEnabled)
        return;

    const float width = static_cast<float>(max(1u, GAME->Get_ViewportWidth()));
    const float height = static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

    const float paddingX = min(max(0.f, _screenClampPadding.x), width * 0.5f);
    const float paddingY = min(max(0.f, _screenClampPadding.y), height * 0.5f);

    screenPos.x = clamp(screenPos.x, paddingX, width - paddingX);
    screenPos.y = clamp(screenPos.y, paddingY, height - paddingY);
}

void UI_BattleFloatingTextBase::Free()
{
    UI_RuntimePanelBase::Free();
}
