#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ENGINE_DLL UI_Transform final : public Base
{
public:
    struct UI_TRANSFORM_DESC
    {
        Vec2 position{ 0.f, 0.f };
        Vec2 size{ 100.f, 100.f };
        Vec2 scale{ 1.f, 1.f };
        float rotationRadians{ 0.f };
    };

public:
    UI_Transform();
    UI_Transform(const UI_Transform& Prototype);
    ~UI_Transform() override = default;

public:
    HRESULT Initialize(const UI_TRANSFORM_DESC* Desc = nullptr);

    void Set_Position(const Vec2& Position);
    void Set_Size(const Vec2& Size);
    void Set_Scale(const Vec2& Scale);
    void Set_RotationRadians(float RotationRadians);

    const Vec2& Get_Position() const { return _position; }
    const Vec2& Get_Size() const { return _size; }
    const Vec2& Get_Scale() const { return _scale; }
    float Get_RotationRadians() const { return _rotationRadians; }

    const Matrix& Get_WorldMatrix() const { return _worldMatrix; }

private:
    void Update_Matrix();

private:
    Vec2 _position{ 0.f, 0.f };
    Vec2 _size{ 100.f, 100.f };
    Vec2 _scale{ 1.f, 1.f };
    float _rotationRadians{ 0.f };

    Matrix _worldMatrix{ Matrix::Identity };

public:
    static Unique<UI_Transform> Create(const UI_TRANSFORM_DESC* Desc = nullptr);
    void Free() override;
};

NS_END
