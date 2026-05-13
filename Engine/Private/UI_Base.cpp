#include "UI_Base.h"

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
    Vec2 worldPosition = nullptr != _transform ? _transform->Get_Position() : Vec2(0.f, 0.f);

    unordered_set<const UI_Base*> visitedParents;
    Shared<UI_Base> parent = _parent.lock();
    while (nullptr != parent)
    {
        if (!visitedParents.insert(parent.get()).second)
            break;

        if (nullptr != parent->_transform)
        {
            const Vec2& parentPosition = parent->_transform->Get_Position();
            worldPosition.x += parentPosition.x;
            worldPosition.y += parentPosition.y;
        }

        parent = parent->_parent.lock();
    }

    return worldPosition;
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
