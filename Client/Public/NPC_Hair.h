#pragma once
#include "Monster_Part.h"

NS_BEGIN(Client)

class NPC_Hair final : public Monster_Part
{
    GENERATED_GAMEOBJECT(NPC_Hair, ObjectType::PartObject)

public:
    struct HEAD_DESC : Monster_Part::MONSTER_PART_DESC
    {
        const Matrix* socketMatrix = nullptr;
    };

public:
    NPC_Hair(const ComPtr<Device>& device, const ComPtr<Context>& context);
    NPC_Hair(const NPC_Hair& prototype);
    ~NPC_Hair() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void Priority_Update(float timeDelta) override;
    void Update(float timeDelta) override;
    void Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

private:
    uint32 Decide_PassIndex(Shared<Material> mat) override;

private:
    const Matrix* _socketMatrix = nullptr;
    Matrix _offsetMatrix = {};

public:
    static Shared<NPC_Hair> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;
};

NS_END
