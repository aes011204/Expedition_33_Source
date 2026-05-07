#include "UI_Transform.h"

UI_Transform::UI_Transform()
{
}

UI_Transform::UI_Transform(const UI_Transform& Prototype)
    : _position{ Prototype._position }
    , _size{ Prototype._size }
    , _scale{ Prototype._scale }
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
