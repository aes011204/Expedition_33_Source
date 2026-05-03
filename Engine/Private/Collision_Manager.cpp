#include "Collision_Manager.h"
#include "ColliderCom.h"
#include "GameInstance.h"
#include "GameObject.h"

HRESULT Collision_Manager::Initialize()
{
    _colliders.clear();

    return S_OK;
}

void Collision_Manager::Update()
{
    const size_t iNumColliders = _colliders.size();

    for (size_t i = 0; i < iNumColliders; ++i)
    {
        Shared<ColliderCom> src = _colliders[i].lock();
        if (nullptr == src)
            continue;

        src->Set_IsColl(false);
    }

    for (size_t i = 0; i < iNumColliders; ++i)
    {
        Shared<ColliderCom> src = _colliders[i].lock();
        if (nullptr == src || false == src->Get_IsActive())
            continue;

        Shared<GameObject> srcOwner = src->Get_Owner();
        if (nullptr == srcOwner)
            continue;

        for (size_t j = i + 1; j < iNumColliders; ++j)
        {
            Shared<ColliderCom> dst = _colliders[j].lock();
            if (nullptr == dst || false == dst->Get_IsActive())
                continue;

            Shared<GameObject> dstOwner = dst->Get_Owner();
            if (nullptr == dstOwner)
                continue;

            if (srcOwner == dstOwner)
                continue;

            CollisionResponse eResponse = Calculate_ResponseResult(
                src->Get_Channel(), src->Get_OverlapMask(),
                src->Get_BlockMask(), dst->Get_Channel(),
                dst->Get_OverlapMask(), dst->Get_BlockMask());

            if (CollisionResponse::Ignore == eResponse)
                continue;

            const bool isIntersect = src->Intersect(dst);
            const bool wasOverlapping = src->Is_Overlapping(dst);

            if (true == isIntersect)
            {
                src->Set_IsColl(true);
                dst->Set_IsColl(true);

                if (false == wasOverlapping)
                {
                    src->Add_Overlap(dst);
                    dst->Add_Overlap(src);

                    if (CollisionResponse::Block == eResponse)
                    {
                        srcOwner->OnBlockBegin(src, dst);
                        dstOwner->OnBlockBegin(dst, src);
                    }
                    else
                    {
                        srcOwner->OnBeginOverlap(src, dst);
                        dstOwner->OnBeginOverlap(dst, src);
                    }
                }
                else
                {
                    if (CollisionResponse::Block == eResponse)
                    {
                        srcOwner->OnBlockStay(src, dst);
                        dstOwner->OnBlockStay(dst, src);
                    }
                    else
                    {
                        srcOwner->OnStayOverlap(src, dst);
                        dstOwner->OnStayOverlap(dst, src);
                    }
                }
            }
            else
            {
                if (true == wasOverlapping)
                {
                    src->Remove_Overlap(dst);
                    dst->Remove_Overlap(src);

                    srcOwner->OnEndOverlap(src, dst);
                    dstOwner->OnEndOverlap(dst, src);
                }
            }
        }
    }

#ifdef _DEBUG
    if (GAME->KeyDown(KEY_TYPE::F2))
        _isDebug = !_isDebug;
#endif
}

#ifdef _DEBUG
void Collision_Manager::Render_Debug()
{
    if (!_isDebug)
        return;

    for (const auto& colliderWeak : _colliders)
    {
        auto collider = colliderWeak.lock();

        if (collider)
        {
            collider->Render_Debug();
        }
    }
}
#endif

void Collision_Manager::Add_Collider(Shared<ColliderCom> collider)
{
    CHECK_NULL(collider);

    _colliders.push_back(collider);
}

void Collision_Manager::Clear_Colliders()
{
    _colliders.clear();
}

Unique<Collision_Manager> Collision_Manager::Create()
{
    auto instance = make_unique<Collision_Manager>();
    instance->Initialize();

    return instance;
}

void Collision_Manager::Free()
{
    Base::Free();

    _colliders.clear();
}
