#pragma once

#include "GameObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(MainEditor)

class Animation_PreviewObject final : public GameObject
{
public:
    Animation_PreviewObject(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Animation_PreviewObject(const Animation_PreviewObject& prototype);
    ~Animation_PreviewObject() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;

public:
    bool Set_ModelPath(const wstring& modelPath);
    Shared<ModelCom> Get_ModelComponent() const;

private:
    HRESULT Ready_Components();
    HRESULT Bind_ShaderResources();

private:
    Shared<ShaderCom>   _staticShaderCom{};
    Shared<ShaderCom>   _animShaderCom{};
    Shared<ModelCom>    _modelCom{};

public:
    static Shared<Animation_PreviewObject> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
