#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Client)

class Mon_Simon_Hair final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Simon_Hair, ObjectType::PartObject)

public:
    struct HEAD_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const Matrix* socketMatrix = nullptr;
        ModelCom* collisionModel = nullptr;
        const Matrix* collisionOwnerWorldMatrix = nullptr;
    };

public:
    Mon_Simon_Hair(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Simon_Hair(const Mon_Simon_Hair& prototype);
    ~Mon_Simon_Hair() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

private:
    HRESULT Ready_Modifier();
    uint32 Decide_PassIndex(Shared<Material> mat) override;

private:
    const Matrix* _socketMatrix = nullptr;
    ModelCom* _collisionModel = nullptr;
    const Matrix* _collisionOwnerWorldMatrix = nullptr;
    Matrix _offsetMatrix = {};

public:
    static Shared<Mon_Simon_Hair> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
