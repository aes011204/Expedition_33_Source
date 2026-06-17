#pragma once

#include "Monster_Part.h"

NS_BEGIN(Client)

class Mon_Dualliste_Body : public Monster_Part
{
    GENERATED_GAMEOBJECT(Mon_Dualliste_Body, ObjectType::PartObject)

public:
    using Body_Desc = Monster_Part::MONSTER_PART_DESC;

public:
    Mon_Dualliste_Body(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Mon_Dualliste_Body(const Mon_Dualliste_Body& prototype);
    ~Mon_Dualliste_Body() override = default;

public:
    HRESULT Initialize_Prototype() override;
    HRESULT Initialize(void* arg) override;
    void    Priority_Update(float timeDelta) override;
    void    Update(float timeDelta) override;
    void    Late_Update(float timeDelta) override;
    HRESULT Render() override;
    HRESULT Render_Shadow() override;

private:
    int32 _rootIndex = -1;

public:
    static Shared<Mon_Dualliste_Body> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
    Shared<GameObject> Clone(void* arg) override;
    void Free() override;

};

NS_END
