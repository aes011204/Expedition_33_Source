#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Client)

class Mon_Dualliste_Body final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Dualliste_Body, ObjectType::PartObject)

public:
    using BODY_DESC = Monster_Part::MONSTER_PART_DESC;

public:
    Mon_Dualliste_Body(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Dualliste_Body(const Mon_Dualliste_Body& prototype);
    virtual ~Mon_Dualliste_Body() override = default;

public:
    virtual HRESULT Initialize_Prototype() override;
    virtual HRESULT Initialize(void* arg) override;
    virtual void Priority_Update(float timeDelta) override;
    virtual void Update(float timeDelta) override;
    virtual void Late_Update(float timeDelta) override;
    virtual HRESULT Render() override;
    virtual HRESULT Render_Shadow() override;

public:
    using Monster_Part::Get_Model;
    using Monster_Part::Get_RootTransformVelocity;
    using Monster_Part::Get_SocketBoneMatrixPtr;
    using Monster_Part::Play_Animation;
    using Monster_Part::Set_Animation;

private:
    int32 _rootIndex = -1;

public:
    static Shared<Mon_Dualliste_Body> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
