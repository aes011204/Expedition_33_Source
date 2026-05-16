#pragma once

#include "Client_Defines.h"
#include "Monster_Part.h"

NS_BEGIN(Client)

class Mon_Lancelier_Body final : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Lancelier_Body, ObjectType::PartObject)

public:
    using BODY_DESC = Monster_Part::MONSTER_PART_DESC;

public:
    Mon_Lancelier_Body(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Lancelier_Body(const Mon_Lancelier_Body& prototype);
    virtual ~Mon_Lancelier_Body() override = default;

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
    static Shared<Mon_Lancelier_Body> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    virtual Shared<GameObject> Clone(void* arg) override;
    virtual void Free() override;
};

NS_END
