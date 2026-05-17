#include "UI_Transform.h"

UI_Transform::UI_Transform()
{
}

UI_Transform::UI_Transform(const UI_Transform& Prototype)
    : _position{ Prototype._position }
    , _size{ Prototype._size }
    , _scale{ Prototype._scale }
    , _pivot{ Prototype._pivot }
    , _anchorMin{ Prototype._anchorMin }
    , _anchorMax{ Prototype._anchorMax }
    , _rotationRadians{ Prototype._rotationRadians }
    , _worldMatrix{ Prototype._worldMatrix }
{
}

HRESULT UI_Transform::Initialize(const UI_TRANSFORM_DESC* Desc)
{
    if (nullptr != Desc)
    {
        _position = Desc->position;
        _size = Desc->size;
        _scale = Desc->scale;
        _pivot = Desc->pivot;
        _anchorMin = Desc->anchorMin;
        _anchorMax = Desc->anchorMax;
        _rotationRadians = Desc->rotationRadians;
    }

    Update_Matrix();

    return S_OK;
}

void UI_Transform::Set_Position(const Vec2& Position)
{
    _position = Position;
    Update_Matrix();
}

void UI_Transform::Set_Size(const Vec2& Size)
{
    _size = Size;
    Update_Matrix();
}

void UI_Transform::Set_Scale(const Vec2& Scale)
{
    _scale = Scale;
    Update_Matrix();
}

void UI_Transform::Set_Pivot(const Vec2& Pivot)
{
    _pivot = Pivot;
    Update_Matrix();
}

void UI_Transform::Set_AnchorMin(const Vec2& AnchorMin)
{
    _anchorMin = AnchorMin;
    Update_Matrix();
}

void UI_Transform::Set_AnchorMax(const Vec2& AnchorMax)
{
    _anchorMax = AnchorMax;
    Update_Matrix();
}

void UI_Transform::Set_Anchors(const Vec2& AnchorMin, const Vec2& AnchorMax)
{
    _anchorMin = AnchorMin;
    _anchorMax = AnchorMax;
    Update_Matrix();
}

void UI_Transform::Set_Anchor(const Vec2& Anchor)
{
    _anchorMin = Anchor;
    _anchorMax = Anchor;
    Update_Matrix();
}

void UI_Transform::Set_RotationRadians(float RotationRadians)
{
    _rotationRadians = RotationRadians;
    Update_Matrix();
}

void UI_Transform::Update_Matrix()
{
    const Vec2 FinalSize{
        _size.x * _scale.x,
        _size.y * _scale.y
    };

    _worldMatrix =
        Matrix::CreateScale(FinalSize.x, FinalSize.y, 1.f) *
        Matrix::CreateRotationZ(_rotationRadians) *
        Matrix::CreateTranslation(_position.x, _position.y, 0.f);
}

Unique<UI_Transform> UI_Transform::Create(const UI_TRANSFORM_DESC* Desc)
{
    Unique<UI_Transform> Instance{ new UI_Transform() };

    if (FAILED(Instance->Initialize(Desc)))
    {
        MSG_BOX("Failed to Created : UI_Transform");
        Instance->Free();
        return nullptr;
    }

    return Instance;
}

void UI_Transform::Free()
{
    __super::Free();
}
