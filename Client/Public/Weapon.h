#pragma once

#include "Client_Defines.h"
#include "PartObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
class AnimComputeRuntime;
NS_END

NS_BEGIN(Client)

class Weapon final : public PartObject
{
    GENERATED_GAMEOBJECT(Weapon, ObjectType::Weapon)

public:
    typedef struct tagWeaponDesc final : public PartObject::PARTOBJECT_DESC
    {
        ModelType       modelType{ ModelType::NonAnim };
        const wchar_t*  modelTag{ nullptr };
        const Matrix*   socketMatrix{ nullptr };

        // TODO : 각 무기별 크기, 위치는 프리펩에서 조절하십쇼


    }WEAPON_DESC;

public:
    Weapon(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Weapon(const Weapon& prototype);
    virtual ~Weapon() = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

    void    From_Json(const json& data) override;

public:
    const Matrix* Get_SocketBoneMatrixPtr(const string& boneName) const;
    Shared<ModelCom> Get_Model() const { return _model; }

private:
    HRESULT Ready_Components(const WEAPON_DESC& desc);
    HRESULT Ready_DefaultComponents(); // desc 없이 생성

    HRESULT Bind_ShaderResources();

    HRESULT Ensure_RuntimeComponentsFromJson(const json& data);

private:
    Shared<ShaderCom>    _shader = { nullptr };
    Shared<ModelCom>     _model = { nullptr };

private:
    const Matrix*       _socketMatrix = { nullptr };

public:
    static Shared<Weapon> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* pArg) override;
    virtual void Free() override;

};

NS_END
