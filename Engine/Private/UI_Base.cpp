#include "UI_Base.h"

#include "GameInstance.h"

#include <cmath>

namespace
{
    Vec2 Rotate_Point(const Vec2& point, float rotationRadians)
    {
        const float cosAngle = cosf(rotationRadians);
        const float sinAngle = sinf(rotationRadians);

        return Vec2(
            point.x * cosAngle - point.y * sinAngle,
            point.x * sinAngle + point.y * cosAngle);
    }

    Vec2 Transform_PointByParent(const Vec2& point, const UI_Base& parent)
    {
        const UI_Transform* parentTransform = parent.Get_Transform();
        if (nullptr == parentTransform)
            return point;

        const Vec2 parentPosition = parent.Get_ResolvedLocalPosition();
        const Vec2 parentSize = parent.Get_ResolvedLocalSize();
        const Vec2& parentScale = parentTransform->Get_Scale();
        const Vec2& parentPivot = parentTransform->Get_Pivot();

        const Vec2 pivotOffset(
            parentSize.x * parentPivot.x,
            parentSize.y * parentPivot.y);

        const Vec2 scaledOffset(
            (point.x - pivotOffset.x) * parentScale.x,
            (point.y - pivotOffset.y) * parentScale.y);
        const Vec2 rotatedOffset = Rotate_Point(scaledOffset, parentTransform->Get_RotationRadians());

        return Vec2(
            parentPosition.x + pivotOffset.x + rotatedOffset.x,
            parentPosition.y + pivotOffset.y + rotatedOffset.y);
    }
}

UI_Base::UI_Base()
{
}

UI_Base::UI_Base(const UI_Base& Prototype)
    : _isOpen{ false }
    , _sortOrder{ Prototype._sortOrder }
    , _renderGroup{Prototype._renderGroup }
{
    Set_Name(Prototype.Get_Name());

    if (nullptr != Prototype._transform)
        _transform = make_unique<UI_Transform>(*Prototype._transform);
}

HRESULT UI_Base::Initialize_Prototype()
{
    _transform = UI_Transform::Create();
    CHECK_NULL(_transform, E_FAIL);

    return S_OK;
}

HRESULT UI_Base::Initialize(void* Arg)
{
    UI_DESC Desc{};

    if (nullptr != Arg)
        Desc = *static_cast<UI_DESC*>(Arg);

    _sortOrder = Desc.sortOrder;
    _isOpen = Desc.isOpenOnCreate;
    _renderGroup = Desc.renderGroup;

    _transform = UI_Transform::Create(&Desc.transformDesc);
    CHECK_NULL(_transform, E_FAIL);

    _animationPlayer.Set_BaseTransformState(
        _transform->Get_Position(),
        _transform->Get_Scale(),
        _transform->Get_RotationRadians());
    _animationPlayer.Set_Clips(Desc.animationClips);
    _animationPlayer.Set_KeyframeAssets(Desc.keyframeAnimationAssets);
    if (_isOpen)
        _animationPlayer.Play_OnOpen();

    return S_OK;
}

void UI_Base::Open()
{
    _isOpen = true;
    _animationPlayer.Play_OnOpen();
}

void UI_Base::Close()
{
    _isOpen = false;
    _animationPlayer.Stop();
}

void UI_Base::Update(float TimeDelta)
{
    if (nullptr == _transform)
        return;

    UI_AnimationSample sample = {};
    if (!_animationPlayer.Update(TimeDelta, sample))
        return;

    if (sample.hasPosition)
        _transform->Set_Position(sample.position);

    if (sample.hasScale)
        _transform->Set_Scale(sample.scale);

    if (sample.hasRotation)
        _transform->Set_RotationRadians(sample.rotationRadians);

    if (sample.hasColor)
    {
        Vec4 color = sample.hasColorTrack ? sample.color : Get_Color();
        if (sample.hasFadeTrack)
            color.w = sample.color.w;
        Set_Color(color);
    }
}

HRESULT UI_Base::Render()
{
    return S_OK;
}

bool UI_Base::Is_EffectivelyOpen() const
{
    if (!_isOpen)
        return false;

    unordered_set<const UI_Base*> visitedParents;
    Shared<UI_Base> parent = _parent.lock();
    while (nullptr != parent)
    {
        if (!visitedParents.insert(parent.get()).second)
            return false;

        if (!parent->Is_Open())
            return false;

        parent = parent->_parent.lock();
    }

    return true;
}

Vec2 UI_Base::Get_WorldPosition() const
{
    if (nullptr == _transform)
        return Vec2(0.f, 0.f);

    return Transform_LocalPointToWorld(Get_ResolvedLocalPosition());
}

Vec2 UI_Base::Get_ParentAnchorReferenceSize() const
{
    Shared<UI_Base> parent = _parent.lock();
    if (nullptr != parent)
        return parent->Get_ResolvedLocalSize();

    return Vec2(
        static_cast<float>(max(1u, GAME->Get_ViewportWidth())),
        static_cast<float>(max(1u, GAME->Get_ViewportHeight())));
}

Vec2 UI_Base::Get_ResolvedLocalPosition() const
{
    if (nullptr == _transform)
        return Vec2(0.f, 0.f);

    const Vec2 parentReferenceSize = Get_ParentAnchorReferenceSize();
    const Vec2& position = _transform->Get_Position();
    const Vec2& anchorMin = _transform->Get_AnchorMin();

    return Vec2(
        position.x + parentReferenceSize.x * anchorMin.x,
        position.y + parentReferenceSize.y * anchorMin.y);
}

Vec2 UI_Base::Get_ResolvedLocalSize() const
{
    if (nullptr == _transform)
        return Vec2(0.f, 0.f);

    const Vec2 parentReferenceSize = Get_ParentAnchorReferenceSize();
    const Vec2& size = _transform->Get_Size();
    const Vec2& anchorMin = _transform->Get_AnchorMin();
    const Vec2& anchorMax = _transform->Get_AnchorMax();

    return Vec2(
        max(1.f, size.x + parentReferenceSize.x * max(0.f, anchorMax.x - anchorMin.x)),
        max(1.f, size.y + parentReferenceSize.y * max(0.f, anchorMax.y - anchorMin.y)));
}

Vec2 UI_Base::Get_ScaledLocalCenter(const Vec2& localScale) const
{
    if (nullptr == _transform)
        return Vec2(0.f, 0.f);

    const Vec2 localPosition = Get_ResolvedLocalPosition();
    const Vec2 localSize = Get_ResolvedLocalSize();
    const Vec2& pivot = _transform->Get_Pivot();
    const Vec2 pivotOffset(
        localSize.x * pivot.x,
        localSize.y * pivot.y);
    const Vec2 baseCenterOffset(
        localSize.x * 0.5f,
        localSize.y * 0.5f);

    return Vec2(
        localPosition.x + pivotOffset.x + (baseCenterOffset.x - pivotOffset.x) * localScale.x,
        localPosition.y + pivotOffset.y + (baseCenterOffset.y - pivotOffset.y) * localScale.y);
}

Vec2 UI_Base::Transform_LocalPointToWorld(const Vec2& localPoint) const
{
    Vec2 worldPoint = localPoint;
    unordered_set<const UI_Base*> visitedParents;
    Shared<UI_Base> parent = _parent.lock();
    while (nullptr != parent)
    {
        if (!visitedParents.insert(parent.get()).second)
            break;

        if (nullptr != parent->_transform)
            worldPoint = Transform_PointByParent(worldPoint, *parent);

        parent = parent->_parent.lock();
    }

    return worldPoint;
}

Vec2 UI_Base::Get_WorldScale() const
{
    Vec2 worldScale = nullptr != _transform ? _transform->Get_Scale() : Vec2(1.f, 1.f);

    unordered_set<const UI_Base*> visitedParents;
    Shared<UI_Base> parent = _parent.lock();
    while (nullptr != parent)
    {
        if (!visitedParents.insert(parent.get()).second)
            break;

        if (nullptr != parent->_transform)
        {
            const Vec2& parentScale = parent->_transform->Get_Scale();
            worldScale.x *= parentScale.x;
            worldScale.y *= parentScale.y;
        }

        parent = parent->_parent.lock();
    }

    return worldScale;
}

float UI_Base::Get_WorldRotationRadians() const
{
    float worldRotationRadians = nullptr != _transform ? _transform->Get_RotationRadians() : 0.f;

    unordered_set<const UI_Base*> visitedParents;
    Shared<UI_Base> parent = _parent.lock();
    while (nullptr != parent)
    {
        if (!visitedParents.insert(parent.get()).second)
            break;

        if (nullptr != parent->_transform)
            worldRotationRadians += parent->_transform->Get_RotationRadians();

        parent = parent->_parent.lock();
    }

    return worldRotationRadians;
}

void UI_Base::Set_Parent(const Shared<UI_Base>& parent)
{
    if (nullptr == parent)
    {
        Clear_Parent();
        return;
    }

    if (parent.get() == this)
    {
        LOG_WARN("UI_Base::Set_Parent rejected self-parent.");
        return;
    }

    for (Shared<UI_Base> cursor = parent; nullptr != cursor; cursor = cursor->_parent.lock())
    {
        if (cursor.get() == this)
        {
            LOG_WARN("UI_Base::Set_Parent rejected cyclic parent chain.");
            return;
        }
    }

    _parent = parent;
}

void UI_Base::Clear_Parent()
{
    _parent.reset();
}

void UI_Base::Set_Color(const Vec4&)
{
}

Vec4 UI_Base::Get_Color() const
{
    return Vec4(1.f, 1.f, 1.f, 1.f);
}

void UI_Base::Free()
{
    Clear_Parent();

    if (nullptr != _transform)
    {
        _transform->Free();
        _transform.reset();
    }

    __super::Free();
}
