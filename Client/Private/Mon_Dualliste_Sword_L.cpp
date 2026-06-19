#include "Mon_Dualliste_Sword_L.h"

#include "EffectCom.h"

NS_BEGIN(Client)

//IMPLEMENT_REFLECTION(Mon_Dualliste_Sword_L)
//{
//    auto& info = GetStaticReflectionInfo();
//    info.properties.clear();
//
//    info.displayName = "Mon_Dualliste_Sword_L";
//    info.category = "Monster";
//    
//
//    return true;
//}


Mon_Dualliste_Sword_L::Mon_Dualliste_Sword_L(const ComPtr<Device>& device, const ComPtr<Context>& context)
    : Monster_Part(device, context)
{
}

Mon_Dualliste_Sword_L::Mon_Dualliste_Sword_L(const Mon_Dualliste_Sword_L& prototype)
    : Monster_Part(prototype)
{
}

HRESULT Mon_Dualliste_Sword_L::Initialize_Prototype()
{
    return Monster_Part::Initialize_Prototype();
}

HRESULT Mon_Dualliste_Sword_L::Initialize(void* arg)
{
    auto* desc = static_cast<SWORD_DESC*>(arg);
    SWORD_DESC fallbackDesc{};
    fallbackDesc.modelType = ModelType::NonAnim;
    fallbackDesc.modelTag = L"Model_Dualliste_Sword_L";

    if (desc == nullptr)
        desc = &fallbackDesc;

    _socketMatrix = desc->socketMatrix;

    _offsetMatrix = Matrix::CreateRotationX(XMConvertToRadians(-90.f));

    if (FAILED(Monster_Part::Initialize(desc)))
        return E_FAIL;

    return S_OK;
}

void Mon_Dualliste_Sword_L::Priority_Update(float timeDelta)
{
    Monster_Part::Priority_Update(timeDelta);
}

void Mon_Dualliste_Sword_L::Update(float timeDelta)
{
    Matrix socketMatrix = Matrix::Identity;
    if (_socketMatrix != nullptr)
        socketMatrix = *_socketMatrix;

    Update_CombinedWorldMatrix(_offsetMatrix * _transformCom->Get_WorldMatrix() * socketMatrix);

    if (_partEffectCom)
        _partEffectCom->Update(timeDelta);
}

void Mon_Dualliste_Sword_L::Late_Update(float timeDelta)
{
    Monster_Part::Late_Update(timeDelta);
}

HRESULT Mon_Dualliste_Sword_L::Render()
{
    return Monster_Part::Render();
}

HRESULT Mon_Dualliste_Sword_L::Render_Shadow()
{
    return Monster_Part::Render_Shadow();
}

uint32 Mon_Dualliste_Sword_L::Decide_PassIndex(Shared<Material> mat)
{
    const int opaquePass = 0;
    UNREFERENCED_PARAMETER(mat);
    return opaquePass;
}

Shared<Mon_Dualliste_Sword_L> Mon_Dualliste_Sword_L::Create(const ComPtr<Device>& device, const ComPtr<Context>& context)
{
    auto instance = make_shared<Mon_Dualliste_Sword_L>(device, context);

    if (FAILED(instance->Initialize_Prototype()))
        return nullptr;

    return instance;
}

Shared<GameObject> Mon_Dualliste_Sword_L::Clone(void* arg)
{
    auto clone = make_shared<Mon_Dualliste_Sword_L>(*this);

    if (FAILED(clone->Initialize(arg)))
        return nullptr;

    return clone;
}

void Mon_Dualliste_Sword_L::Free()
{
    Monster_Part::Free();
}

NS_END
