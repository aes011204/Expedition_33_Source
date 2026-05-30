#pragma once

#include "PartObject.h"

NS_BEGIN(Engine)
class ShaderCom;
class ModelCom;
NS_END

NS_BEGIN(Client)

// 구스타브 턱수염, 콧수염 등 NonAnim 파츠로 사용할 공통 클래스
class FaceAccessory_Player final : public PartObject
{
    GENERATED_GAMEOBJECT(FaceAccessory_Player, ObjectType::PartObject)

public:
    typedef struct tagFaceAccessoryPlayerDesc final : public PartObject::PARTOBJECT_DESC
    {
        ModelType       modelType{ ModelType::NonAnim };
        const wchar_t*  modelTag{ nullptr };      
        const Matrix*   socketMatrixPointer{ nullptr };
    } FACE_ACCESSORY_DESC;

public:
    FaceAccessory_Player(const ComPtr<Device>& device, const ComPtr<Context>& context);
    FaceAccessory_Player(const FaceAccessory_Player& prototype);
    ~FaceAccessory_Player() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;
    Matrix  Get_GizmoWorldMatrix() const override;
    bool    Apply_GizmoWorldMatrix(const Matrix& gizmoWorldMatrix) override;

private:
    HRESULT Ready_Components(const FACE_ACCESSORY_DESC& desc);
    HRESULT Bind_ShaderResources();

private:
    Shared<ShaderCom>    _shader{};
    Shared<ModelCom>     _model{};

    const Matrix*        _socketMatrix = nullptr;

public:
    static Shared<FaceAccessory_Player> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
