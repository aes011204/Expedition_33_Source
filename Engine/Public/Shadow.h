#pragma once

#include "Base.h"

NS_BEGIN(Engine)

class ShaderCom;

class ENGINE_DLL Shadow : public Base
{
public:
    explicit Shadow() = default;
    virtual ~Shadow() = default;

public:
    HRESULT Update_LightDesc(const LIGHT_DESC& lightDesc);
    HRESULT Bind_TransformStateMatrix(Shared<ShaderCom> shader, const char* viewName, const char* projName);
    void    Clear();

public:
    const Matrix* Get_ViewMatrix() const;
    const Matrix* Get_ProjMatrix() const;
    const LIGHT_DESC* Get_LightDesc() const;
    bool Has_Light() const;

public:
    static Unique<Shadow> Create();
    void Free() override;

private:
    LIGHT_DESC   _lightDesc = {};
    bool         _hasLight = false;
    Matrix       _viewMatrix = Matrix::Identity;
    Matrix       _projMatrix = Matrix::Identity;
};

NS_END
