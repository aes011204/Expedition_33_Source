#include "PhysicsCharacterController.h"

#include "Debug_Profile.h"
#include "PhysX_Manager.h"
#include "PhysX_Filter.h"
#include "characterkinematic/PxController.h"
#include "characterkinematic/PxCapsuleController.h"
#include "characterkinematic/PxControllerManager.h"

namespace
{
    PxFilterData Build_ControllerResponseFilterData(Collision_Preset preset)
    {
        const Collision_Preset_Data& presetData = Get_PresetData(preset);
        return Build_FilterData(
            presetData.channel,
            presetData.overlapMask,
            presetData.blockMask);
    }

    PxFilterData Build_ControllerQueryFilterData(Collision_Preset preset)
    {
        const Collision_Preset_Data& presetData = Get_PresetData(preset);

        PxFilterData data{};
        // PxController::move() 내부 overlap의 기본 coarse filter는 word0끼리 AND를 본다.
        // query 쪽에는 "내가 관심 있는 대상 채널 mask"를 넣어서 월드 후보를 초기에 좁힌다.
        data.word0 = presetData.overlapMask | presetData.blockMask;
        data.word1 = 0;
        data.word2 = 0;
        data.word3 = 0;
        return data;
    }

    class Controller_QueryFilterCallback final : public PxQueryFilterCallback
    {
    public:
        explicit Controller_QueryFilterCallback(const PxFilterData& selfFilterData)
            : _selfFilterData(selfFilterData)
        {
        }

        PxQueryHitType::Enum preFilter(
            const PxFilterData& filterData,
            const PxShape* shape,
            const PxRigidActor* actor,
            PxHitFlags& queryFlags) override
        {
            UNREFERENCED_PARAMETER(filterData);
            UNREFERENCED_PARAMETER(actor);
            UNREFERENCED_PARAMETER(queryFlags);

            if (shape == nullptr)
                return PxQueryHitType::eNONE;

            const CollisionResponse response =
                Calculate_PhysXResponse(_selfFilterData, shape->getQueryFilterData());

            // CCT move()는 내부적으로 overlap 기반 필터를 사용하므로 eBLOCK을 직접 반환하지 않는다.
            // 월드와 충돌해야 하는 후보는 eTOUCH로 남겨서 CCT 내부 해결 단계로 넘긴다.
            return (response == CollisionResponse::Block) ? PxQueryHitType::eTOUCH : PxQueryHitType::eNONE;
        }

        PxQueryHitType::Enum postFilter(
            const PxFilterData& filterData,
            const PxQueryHit& hit,
            const PxShape* shape,
            const PxRigidActor* actor) override
        {
            UNREFERENCED_PARAMETER(filterData);
            UNREFERENCED_PARAMETER(hit);
            UNREFERENCED_PARAMETER(shape);
            UNREFERENCED_PARAMETER(actor);

            return PxQueryHitType::eBLOCK;
        }

    private:
        PxFilterData _selfFilterData{};
    };

    class Controller_PlayerWorldBodyFilterCallback final : public PxControllerFilterCallback
    {
    public:
        bool filter(const PxController& a, const PxController& b) override
        {
            const auto lhs = static_cast<const PhysicsCharacterController*>(a.getUserData());
            const auto rhs = static_cast<const PhysicsCharacterController*>(b.getUserData());

            if (lhs == nullptr || rhs == nullptr)
                return true;

            if (lhs->Get_CollisionPreset() == Collision_Preset::Player_WorldBody &&
                rhs->Get_CollisionPreset() == Collision_Preset::Player_WorldBody)
                return false;

            return true;
        }
    };
}

HRESULT PhysicsCharacterController::Initialize(
    PhysX_Manager* physXManager,
    const CharacterControllerDesc& desc,
    const Vec3& initialFootPosition)
{
    _physXManager = physXManager;
    _desc = desc;

    CHECK_NULL(_physXManager, E_FAIL);

    _controller = _physXManager->Create_CapsuleController(desc, initialFootPosition);
    CHECK_NULL(_controller, E_FAIL);
    _controller->setUserData(this);

    // 런타임에서는 발 위치 기준으로 상태를 다루므로 생성 직후에도 같은 기준으로 맞춘다.
    _controller->setFootPosition(PxExtendedVec3(initialFootPosition.x, initialFootPosition.y, initialFootPosition.z));

    return S_OK;
}

CharacterControllerMoveResult PhysicsCharacterController::Move(const Vec3& displacement, float timeDelta)
{
    ENGINE_PROFILE_SCOPE_DETAIL("PhysX::CCT::Move");

    CharacterControllerMoveResult result{};

    CHECK_NULL(_controller, result);

#ifdef _DEBUG
    const auto queryBegin = chrono::steady_clock::now();
#endif

    const PxFilterData responseFilterData = Build_ControllerResponseFilterData(_desc.preset);
    const PxFilterData queryFilterData = Build_ControllerQueryFilterData(_desc.preset);
    Controller_QueryFilterCallback queryFilterCallback(responseFilterData);
    Controller_PlayerWorldBodyFilterCallback playerWorldBodyFilterCallback;
    // coarse filter는 PhysX 기본 query AND 필터에 맡기고, 최종 Block/Ignore 판정만 callback에서 처리한다.
    PxControllerFilters filters(&queryFilterData, &queryFilterCallback, &playerWorldBodyFilterCallback);
    // SL 캐릭터/몬스터 몸체는 kinematic dynamic collider라 이동 차단 후보에 포함해야 한다.
    filters.mFilterFlags = PxQueryFlag::eSTATIC | PxQueryFlag::eDYNAMIC | PxQueryFlag::ePREFILTER;
    const PxControllerCollisionFlags flags =
        _controller->move(PxVec3(displacement.x, displacement.y, displacement.z), 0.001f, timeDelta, filters);

    result.footPosition = Get_FootPosition();
    result.collisionDown = flags.isSet(PxControllerCollisionFlag::eCOLLISION_DOWN);
    result.collisionSide = flags.isSet(PxControllerCollisionFlag::eCOLLISION_SIDES);
    result.collisionUp = flags.isSet(PxControllerCollisionFlag::eCOLLISION_UP);

#ifdef _DEBUG
    if (_physXManager != nullptr)
    {
        const double elapsedMs =
            chrono::duration<double, milli>(chrono::steady_clock::now() - queryBegin).count();

        const bool hit =
            result.collisionDown ||
            result.collisionSide ||
            result.collisionUp;

        _physXManager->Record_CCTMovePerf(hit, elapsedMs);
    }
#endif

    return result;
}

void PhysicsCharacterController::Set_FootPosition(const Vec3& footPosition)
{
    CHECK_NULL(_controller);
    _controller->setFootPosition(PxExtendedVec3(footPosition.x, footPosition.y, footPosition.z));
}

Vec3 PhysicsCharacterController::Get_FootPosition() const
{
    CHECK_NULL(_controller, Vec3::Zero);

    const PxExtendedVec3 foot = _controller->getFootPosition();
    return Vec3(static_cast<float>(foot.x), static_cast<float>(foot.y), static_cast<float>(foot.z));
}

Shared<PhysicsCharacterController> PhysicsCharacterController::Create(
    PhysX_Manager* physXManager,
    const CharacterControllerDesc& desc,
    const Vec3& initialFootPosition)
{
    auto instance = make_shared<PhysicsCharacterController>();
    if (FAILED(instance->Initialize(physXManager, desc, initialFootPosition)))
        return nullptr;
    return instance;
}

void PhysicsCharacterController::Free()
{
    if (_physXManager != nullptr && _controller != nullptr)
    {
        _physXManager->Release_Controller(_controller);
        _controller = nullptr;
    }
    Base::Free();
}
