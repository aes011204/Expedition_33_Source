#include "HairChainPhysics.h"
#include "ModelCom.h"

void HairChainPhysics::Initialize(ModelCom* model, const HairChainPhysicsDesc& desc)
{
    // Strand_ bone들을 찾아서 체인으로 묶고, 초기 pose 저장

    _desc = desc;
    _chains.clear();
    _built = false;
    _hasLastOwnerPosition = false;

    if (model == nullptr || !model->Is_Skeletal())
        return;

    Initialize_Chains(model);
    Capture_CurrentPose(model);

    _built = !_chains.empty();
}

void HairChainPhysics::ResetPose(ModelCom* model)
{
    Capture_CurrentPose(model);
}

void HairChainPhysics::Simulate(ModelCom* model, float timeDelta, const Matrix& ownerWorld, const Vec3& ownerVelocity)
{
    // Hair Player에서 매 프레임 호출.
    if (!_built || model == nullptr || timeDelta <= 0.f)
        return;

    const Vec3 ownerPosition = Get_WorldPosition(ownerWorld);

    if (_hasLastOwnerPosition)
    {
        const float moved = Vector3::Distance(ownerPosition, _lastOwnerPosition);
        if (moved >= _desc.teleportDistance)
        {
            ResetPose(model);
            _lastOwnerPosition = ownerPosition;
            return;
        }
    }

    _lastOwnerPosition = ownerPosition;
    _hasLastOwnerPosition = true;

    // 현재 애니메이션 pose 기준으로 root는 매 프레임 고정
    for (Chain& chain : _chains)
    {
        if (chain.nodes.empty())
            continue;

        Node& root = chain.nodes[0];
        if (const Matrix* rootCombined = model->Get_BoneMatrixPtr(root.boneIndex))
        {
            root.position = Get_WorldPosition(*rootCombined);
            root.prevPosition = root.position;
        }
    }

    // MovementCom velocity는 world space 기준
    const Matrix ownerInv = ownerWorld.Invert();
    Vec3 localOwnerVelocity = Vector3::TransformNormal(ownerVelocity, ownerInv);
    localOwnerVelocity.y = 0.f;

    Solve_Verlet(model, timeDelta, localOwnerVelocity);
    Write_BackToModel(model);
}

void HairChainPhysics::Initialize_Chains(ModelCom* model)
{
    _chains.clear();

    umap<uint32, pair<uint32, uint32>> nodeLookup;

    const uint32 numBones = static_cast<uint32>(model->Get_NumBones());

    for (uint32 boneIndex = 0; boneIndex < numBones; ++boneIndex)
    {
        const string& boneName = model->Get_BoneName(boneIndex);
        if (!Is_HairBoneName(boneName, _desc.bonePrefix))
            continue;

        const int32 parentBoneIndex = model->Get_BoneParentIndex(boneIndex);

        auto parentNodeIt = parentBoneIndex >= 0
            ? nodeLookup.find(static_cast<uint32>(parentBoneIndex))
            : nodeLookup.end();

        uint32 chainIndex = 0;
        int32 parentNodeIndex = -1;

        if (parentNodeIt != nodeLookup.end())
        {
            // 부모가 이미 hair chain에 들어가 있으면 같은 체인에 붙인다.
            chainIndex = parentNodeIt->second.first;
            parentNodeIndex = static_cast<int32>(parentNodeIt->second.second);
        }
        else
        {
            // 부모가 hair bone이 아니면 새 체인의 root다.
            chainIndex = static_cast<uint32>(_chains.size());
            _chains.push_back(Chain{});
            _chains.back().rootBoneName = boneName;
            parentNodeIndex = -1;
        }

        Chain& chain = _chains[chainIndex];

        Node node{};
        node.boneIndex = boneIndex;
        node.parentNodeIndex = parentNodeIndex;
        node.bindLocal = model->Get_BoneLocalMatrix(boneIndex);

        const uint32 nodeIndex = static_cast<uint32>(chain.nodes.size());
        chain.nodes.push_back(node);

        nodeLookup.emplace(boneIndex, make_pair(chainIndex, nodeIndex));
    }

    // root 하나만 있는 체인은 시뮬레이션할 child가 없으므로 제거한다.
    _chains.erase(
        remove_if(_chains.begin(), _chains.end(),
            [](const Chain& chain)
            {
                return chain.nodes.size() < 2;
            }),
        _chains.end());

    for (Chain& chain : _chains)
    {
        const uint32 strandGroup = Get_StrandGroupNumber(chain.rootBoneName);

        switch (_desc.profile)
        {
        case HairPhysicsProfile::Lune:
            Apply_LuneChainTuning(chain, strandGroup);
            break;

        case HairPhysicsProfile::Maelle:
            Apply_MaelleChainTuning(chain, strandGroup);
            break;

        case HairPhysicsProfile::Gustave:
            Apply_GustaveChainTuning(chain, strandGroup);
            break;

        default:
            Apply_LuneChainTuning(chain, strandGroup);
            break;
        }
    }
}

void HairChainPhysics::Capture_CurrentPose(ModelCom* model)
{
    if (model == nullptr)
        return;

    for (Chain& chain : _chains)
    {
        for (uint32 i = 0; i < static_cast<uint32>(chain.nodes.size()); ++i)
        {
            Node& node = chain.nodes[i];

            const Matrix* combined = model->Get_BoneMatrixPtr(node.boneIndex);
            if (combined == nullptr)
                continue;

            node.position = Get_WorldPosition(*combined);
            node.prevPosition = node.position;

            if (node.parentNodeIndex >= 0)
            {
                const Node& parent = chain.nodes[node.parentNodeIndex];
                node.length = Vector3::Distance(node.position, parent.position);

                const Matrix* parentCombined = model->Get_BoneMatrixPtr(parent.boneIndex);
                if (parentCombined != nullptr)
                {
                    Vec3 bindWorldDir = node.position - parent.position;
                    if (bindWorldDir.LengthSquared() > 0.0001f)
                    {
                        bindWorldDir.Normalize();
                        node.bindLocalDir = Get_LocalDirection(*parentCombined, bindWorldDir);
                    }
                    else
                    {
                        node.bindLocalDir = Vec3::Zero;
                    }
                }
            }
            else
            {
                node.length = 0.f;
                node.bindLocalDir = Vec3::Zero;
            }
        }
    }
}

void HairChainPhysics::Solve_Verlet(ModelCom* model, float timeDelta, const Vec3& ownerVelocity)
{
    // Verlet 기본:
    // newPos = pos + (pos - prevPos) * damping + accel * timeDelta^2
    if (model == nullptr)
        return;

    const float timeDeltaClamped = ::min(timeDelta, 1.f / 30.f);

    // idle 상태에서도 너무 정적으로 보이지 않도록 약한 procedural wind 사용.
    const float t = static_cast<float>(GetTickCount64() % 100000) * 0.001f;

    for (Chain& chain : _chains)
    {
        const float keepVelocity = ::clamp(1.f - (_desc.damping * chain.dampingMul), 0.f, 1.f);
        const float windScale = _desc.windScale * chain.windMul;
        const float movementInertiaScale = _desc.movementInertiaScale * chain.movementInertiaMul;
        const float poseRestoreStrength = ::clamp(_desc.poseRestoreStrength * chain.poseRestoreMul, 0.f, 1.f);
        const float maxBendAngleDegree = _desc.maxBendAngleDegree * chain.maxBendAngleMul;
        const float stiffness = _desc.stiffness * chain.stiffnessMul;
        const float windPhase = static_cast<float>(Get_StrandGroupNumber(chain.rootBoneName)) * 0.43f;

        // chain마다 phase를 다르게 둬서 모든 머리카락이 한 덩어리처럼 흔들리지 않게 한다.
        const Vec3 wind = Vec3(
            sinf(t * 1.7f + windPhase) * windScale,
            0.f,
            cosf(t * 1.3f + windPhase * 0.7f) * windScale
        );

        // ownerVelocity는 Simulate에서 hair local 기준으로 변환되어 들어온다.
        // 캐릭터가 앞으로 움직이면 머리카락은 반대 방향으로 살짝 밀린다.
        Vec3 movementInertia = -ownerVelocity * movementInertiaScale;
        movementInertia.y = 0.f;

        const float inertiaLength = movementInertia.Length();
        if (inertiaLength > _desc.maxMovementInertia && inertiaLength > 0.0001f)
        {
            movementInertia /= inertiaLength;
            movementInertia *= _desc.maxMovementInertia;
        }

        const Vec3 accel = (_desc.gravity * chain.gravityMul) + wind;

        Vec3 movementOffset = movementInertia * timeDeltaClamped;
        const float movementOffsetLength = movementOffset.Length();
        if (movementOffsetLength > _desc.maxMovementOffset && movementOffsetLength > 0.0001f)
        {
            movementOffset /= movementOffsetLength;
            movementOffset *= _desc.maxMovementOffset;
        }

        // wind를 accel * dt^2로만 넣으면 idle 상태에서는 거의 보이지 않는다.
        // 작은 직접 offset을 같이 넣고, 아래 pose restore / bend limit에서 다시 잡아준다.
        const Vec3 idleWindOffset = wind * timeDeltaClamped * 1.5f;

        // 1. Verlet integration
        for (uint32 i = 1; i < static_cast<uint32>(chain.nodes.size()); ++i)
        {
            Node& node = chain.nodes[i];

            const Vec3 current = node.position;
            const Vec3 velocity = (node.position - node.prevPosition) * keepVelocity;

            node.position += velocity
                + accel * (timeDeltaClamped * timeDeltaClamped)
                + movementOffset
                + idleWindOffset;

            node.prevPosition = current;
        }

        // 1.5. Pose restore
        // 현재 방향이 아니라 bindLocalDir 기준으로 되돌린다.
        for (uint32 i = 1; i < static_cast<uint32>(chain.nodes.size()); ++i)
        {
            Node& node = chain.nodes[i];

            if (node.parentNodeIndex < 0)
                continue;

            Node& parent = chain.nodes[node.parentNodeIndex];

            const Matrix* parentWorld = model->Get_BoneMatrixPtr(parent.boneIndex);
            if (parentWorld == nullptr)
                continue;

            if (node.bindLocalDir.LengthSquared() <= 0.0001f)
                continue;

            const Vec3 bindWorldDir = Get_WorldDirection(*parentWorld, node.bindLocalDir);
            if (bindWorldDir.LengthSquared() <= 0.0001f)
                continue;

            const Vec3 restTarget = parent.position + bindWorldDir * node.length;

            node.position = Vector3::Lerp(
                node.position,
                restTarget,
                poseRestoreStrength
            );
        }

        // 2. Bend limit을 particle position에도 적용
        // Write_Back에서 본 회전만 제한하면 solver 상태는 계속 밖에 남으므로,
        // 여기서 node.position 자체를 제한된 방향으로 되돌린다.
        for (uint32 i = 1; i < static_cast<uint32>(chain.nodes.size()); ++i)
        {
            Node& node = chain.nodes[i];

            if (node.parentNodeIndex < 0)
                continue;

            Node& parent = chain.nodes[node.parentNodeIndex];

            const Matrix* parentWorld = model->Get_BoneMatrixPtr(parent.boneIndex);
            if (parentWorld == nullptr)
                continue;

            Vec3 simulatedWorldDir = node.position - parent.position;
            if (simulatedWorldDir.LengthSquared() <= 0.0001f)
                continue;

            simulatedWorldDir.Normalize();

            const Vec3 simulatedLocalDir = Get_LocalDirection(*parentWorld, simulatedWorldDir);
            if (node.bindLocalDir.LengthSquared() <= 0.0001f || simulatedLocalDir.LengthSquared() <= 0.0001f)
                continue;

            const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(
                node.bindLocalDir,
                simulatedLocalDir,
                maxBendAngleDegree
            );

            const Vec3 limitedWorldDir = Get_WorldDirection(*parentWorld, limitedLocalDir);
            if (limitedWorldDir.LengthSquared() <= 0.0001f)
                continue;

            node.position = parent.position + limitedWorldDir * node.length;
        }

        // 3. Length constraint
        for (uint32 iter = 0; iter < _desc.solverIterations; ++iter)
        {
            for (uint32 i = 1; i < static_cast<uint32>(chain.nodes.size()); ++i)
            {
                Node& node = chain.nodes[i];
                Node& parent = chain.nodes[node.parentNodeIndex];

                Vec3 delta = node.position - parent.position;
                float dist = delta.Length();

                if (dist <= 0.0001f)
                    continue;

                Vec3 dir = delta / dist;
                Vec3 target = parent.position + dir * node.length;

                node.position = Vector3::Lerp(
                    node.position,
                    target,
                    ::clamp(stiffness * 20.f, 0.f, 1.f)
                );
            }
        }
    }
}


void HairChainPhysics::Write_BackToModel(ModelCom* model)
{
    // 머리카락 본은 parent -> child 방향을 기준으로 local rotation만 보정
    if (model == nullptr)
        return;

    for (Chain& chain : _chains)
    {
        const float maxBendAngleDegree = _desc.maxBendAngleDegree * chain.maxBendAngleMul;
        const uint32 strandGroup = Get_StrandGroupNumber(chain.rootBoneName);
        const bool useParentWriteBack = false;
        vector<bool> rotatedParents(chain.nodes.size(), false);

        for (uint32 i = 1; i < static_cast<uint32>(chain.nodes.size()); ++i)
        {
            Node& node = chain.nodes[i];

            if (node.parentNodeIndex < 0)
                continue;

            Node& parent = chain.nodes[node.parentNodeIndex];
            const uint32 parentNodeIndex = static_cast<uint32>(node.parentNodeIndex);

            if (useParentWriteBack && rotatedParents[parentNodeIndex])
                continue;

            const Matrix* parentWorld = model->Get_BoneMatrixPtr(parent.boneIndex);
            if (parentWorld == nullptr)
                continue;

            Vec3 simulatedWorldDir = node.position - parent.position;
            if (simulatedWorldDir.LengthSquared() <= 0.0001f)
                continue;

            simulatedWorldDir.Normalize();

            const Vec3 simulatedLocalDir = Get_LocalDirection(*parentWorld, simulatedWorldDir);

            if (node.bindLocalDir.LengthSquared() <= 0.0001f || simulatedLocalDir.LengthSquared() <= 0.0001f)
                continue;

            const Vec3 limitedLocalDir = Limit_LocalDirectionByAngle(
                node.bindLocalDir,
                simulatedLocalDir,
                maxBendAngleDegree
            );

            const Quaternion deltaRotation = Build_RotationDelta(node.bindLocalDir, limitedLocalDir);

            const Matrix local = Build_LocalMatrix_RotationOnly(
                useParentWriteBack ? parent.bindLocal : node.bindLocal,
                deltaRotation
            );

            model->Set_BoneLocalMatrix(useParentWriteBack ? parent.boneIndex : node.boneIndex, local);

            if (useParentWriteBack)
                rotatedParents[parentNodeIndex] = true;
        }
    }

    model->Rebuild_BoneCombinedMatrices();
}


bool HairChainPhysics::Is_HairBoneName(const string& name, const string& prefix)
{
    // Strand_로 시작하는 bone만 hair physcis 대상으로 처리
    return name.find(prefix, 0) == 0;
}

uint32 HairChainPhysics::Get_StrandGroupNumber(const string& boneName)
{
    constexpr char prefix[] = "Strand_";
    constexpr size_t prefixLength = sizeof(prefix) - 1;

    if (boneName.find(prefix, 0) != 0)
        return 0;

    uint32 groupNumber = 0;
    for (size_t i = prefixLength; i < boneName.size(); ++i)
    {
        const char c = boneName[i];
        if (c < '0' || c > '9')
            break;

        groupNumber = groupNumber * 10 + static_cast<uint32>(c - '0');
    }

    return groupNumber;
}

void HairChainPhysics::Apply_LuneChainTuning(Chain& chain, uint32 strandGroup)
{
    // Hair_Lune_2_skeleton root translation 기준:
    // Strand_1/2/12/13/14 = X+ 얼굴/앞머리 쪽
    // Strand_5~11 = X- 뒷머리 쪽
    // Strand_3/4 = 중앙/정수리 쪽

    if (strandGroup == 1 || strandGroup == 2 || strandGroup == 12 || strandGroup == 13 || strandGroup == 14)
    {
        // 앞머리/얼굴 주변:
        // 너무 앞으로 튀거나 더듬이처럼 날리면 어색하므로 강하게 잡는다.
        chain.stiffnessMul = 1.15f;
        chain.dampingMul = 1.08f;
        chain.windMul = 0.55f;
        chain.gravityMul = 0.9f;
        chain.movementInertiaMul = 0.65f;
        chain.poseRestoreMul = 1.35f;
        chain.maxBendAngleMul = 0.75f;
    }
    else if (strandGroup >= 5 && strandGroup <= 11)
    {
        // 뒷머리:
        // idle 상태에서도 살짝 살아나야 하므로 wind와 movement inertia를 더 허용한다.
        chain.stiffnessMul = 1.2f;
        chain.dampingMul = 1.18f;
        chain.windMul = 0.35f;
        chain.gravityMul = 1.f;
        chain.movementInertiaMul = 0.42f;
        chain.poseRestoreMul = 1.35f;
        chain.maxBendAngleMul = 0.55f;
    }
    else
    {
        // 중앙/정수리:
        // 앞머리처럼 빡세게 묶지도, 뒷머리처럼 크게 풀지도 않는 중간값.
        chain.stiffnessMul = 1.f;
        chain.dampingMul = 1.f;
        chain.windMul = 0.9f;
        chain.gravityMul = 1.f;
        chain.movementInertiaMul = 0.9f;
        chain.poseRestoreMul = 1.05f;
        chain.maxBendAngleMul = 0.95f;
    }
}

void HairChainPhysics::Apply_MaelleChainTuning(Chain& chain, uint32 strandGroup)
{
    // Maelle export 기준:
    // Maelle_Hair_skeleton:
    //   Strand_0_1 total 46 = 큰 메인 헤어 덩어리
    //   Strand_5_1 total 18 = 긴 사이드/후면 계열
    //   Strand_6_1 total 14 = 긴 사이드/후면 계열
    //
    // Maelle은 Lune보다 체인 수는 적지만 한 체인 덩어리가 길다.
    // 그래서 전체적으로 더 부드럽게, 하지만 너무 무너져 보이지 않게 restore는 중간 이상 유지.

    if (strandGroup == 0)
    {
        // 메인 긴 덩어리:
        // Strand_1~4 계열이 Strand_0_1 아래에 붙어 있어서,
        // 화면에서 보이는 큰 머리 덩어리는 사실상 이 그룹 값의 영향을 받는다.
        // 너무 빡세게 잡으면 Maelle 머리가 거의 안 흔들려 보이므로,
        // Strand_5/6보다는 얌전하지만 Lune보다 흐르게 둔다.
        chain.stiffnessMul = 0.48f;
        chain.dampingMul = 0.58f;
        chain.windMul = 2.4f;
        chain.gravityMul = 0.85f;
        chain.movementInertiaMul = 1.95f;
        chain.poseRestoreMul = 0.45f;
        chain.maxBendAngleMul = 1.55f;
    }
    else if (strandGroup == 5 || strandGroup == 6)
    {
        // 사이드/앞쪽 체인:
        // Maelle skeleton 기준 Strand_5/6은 X+ 쪽이라 앞/옆머리로 보고 강하게 잡는다.
        chain.stiffnessMul = 1.25f;
        chain.dampingMul = 1.22f;
        chain.windMul = 0.25f;
        chain.gravityMul = 0.9f;
        chain.movementInertiaMul = 0.32f;
        chain.poseRestoreMul = 1.55f;
        chain.maxBendAngleMul = 0.45f;
    }
    else
    {
        // 예외 체인:
        // 데이터가 추가되더라도 과하게 터지지 않도록 보수적인 중간값.
        chain.stiffnessMul = 0.9f;
        chain.dampingMul = 1.f;
        chain.windMul = 0.7f;
        chain.gravityMul = 1.f;
        chain.movementInertiaMul = 0.75f;
        chain.poseRestoreMul = 1.f;
        chain.maxBendAngleMul = 0.9f;
    }
}

void HairChainPhysics::Apply_GustaveChainTuning(Chain& chain, uint32 strandGroup)
{
    // Gustave export 기준:
    // Strand_1~11이 있지만 대부분 total 3~8 정도의 짧은 체인.
    // 짧은 머리는 과하게 흔들리면 털처럼 보이므로 Lune/Maelle보다 단단하게 잡는다.

    if (strandGroup == 1 || strandGroup == 2 || strandGroup == 10 || strandGroup == 11)
    {
        // 얼굴/앞쪽에 가까운 짧은 체인:
        // 이동 관성과 wind를 강하게 줄여서 앞쪽 튐을 방지한다.
        chain.stiffnessMul = 1.25f;
        chain.dampingMul = 1.12f;
        chain.windMul = 0.45f;
        chain.gravityMul = 1.f;
        chain.movementInertiaMul = 0.55f;
        chain.poseRestoreMul = 1.25f;
        chain.maxBendAngleMul = 0.75f;
    }
    else if (strandGroup == 7 || strandGroup == 8 || strandGroup == 9)
    {
        // 뒤쪽/후면 체인:
        // Gustave도 뒤쪽은 살짝 움직여야 하지만, Lune처럼 크게 풀면 어색하다.
        chain.stiffnessMul = 1.08f;
        chain.dampingMul = 1.05f;
        chain.windMul = 0.8f;
        chain.gravityMul = 1.f;
        chain.movementInertiaMul = 0.8f;
        chain.poseRestoreMul = 1.1f;
        chain.maxBendAngleMul = 0.9f;
    }
    else
    {
        // 측면/중앙 짧은 체인:
        // 전체적으로 단단한 짧은 머리 기준값.
        chain.stiffnessMul = 1.15f;
        chain.dampingMul = 1.08f;
        chain.windMul = 0.65f;
        chain.gravityMul = 1.f;
        chain.movementInertiaMul = 0.7f;
        chain.poseRestoreMul = 1.18f;
        chain.maxBendAngleMul = 0.85f;
    }
}

Vec3 HairChainPhysics::Get_WorldPosition(const Matrix& m)
{
    return Vec3(m._41, m._42, m._43);
}

Vec3 HairChainPhysics::Get_LocalDirection(const Matrix& parentWorld, const Vec3& worldDirection)
{
    const Matrix parentInv = parentWorld.Invert();

    Vec3 localDirection = Vector3::TransformNormal(worldDirection, parentInv);

    if (localDirection.LengthSquared() > 0.0001f)
        localDirection.Normalize();

    return localDirection;
}

Vec3 HairChainPhysics::Get_WorldDirection(const Matrix& parentWorld, const Vec3& localDirection)
{
    Vec3 worldDirection = Vector3::TransformNormal(localDirection, parentWorld);

    if (worldDirection.LengthSquared() > 0.0001f)
        worldDirection.Normalize();

    return worldDirection;
}

Vec3 HairChainPhysics::Limit_LocalDirectionByAngle(const Vec3& bindLocalDirection, const Vec3& simulatedLocalDirection,
    float maxAngleDegree)
{
    Vec3 bindDir = bindLocalDirection;
    Vec3 simDir = simulatedLocalDirection;

    if (bindDir.LengthSquared() <= 0.0001f || simDir.LengthSquared() <= 0.0001f)
        return bindLocalDirection;

    bindDir.Normalize();
    simDir.Normalize();

    const float maxAngle = XMConvertToRadians(maxAngleDegree);

    float dot = bindDir.Dot(simDir);
    dot = ::clamp(dot, -1.f, 1.f);

    const float angle = acosf(dot);
    if (angle <= maxAngle)
        return simDir;

    Vec3 axis = bindDir.Cross(simDir);
    if (axis.LengthSquared() <= 0.0001f)
        return bindDir;

    axis.Normalize();

    const Quaternion limitedRotation = Quaternion::CreateFromAxisAngle(axis, maxAngle);
    Vec3 limitedDir = Vector3::Transform(bindDir, Matrix::CreateFromQuaternion(limitedRotation));

    if (limitedDir.LengthSquared() > 0.0001f)
        limitedDir.Normalize();

    return limitedDir;
}

Quaternion HairChainPhysics::Build_RotationDelta(const Vec3& fromDirection, const Vec3& toDirection)
{
    Vec3 from = fromDirection;
    Vec3 to = toDirection;

    if (from.LengthSquared() <= 0.0001f || to.LengthSquared() <= 0.0001f)
        return Quaternion::Identity;

    from.Normalize();
    to.Normalize();

    float dot = from.Dot(to);
    dot = ::clamp(dot, -1.f, 1.f);

    if (dot > 0.9995f)
        return Quaternion::Identity;

    if (dot < -0.9995f)
    {
        Vec3 axis = Vec3::UnitX.Cross(from);
        if (axis.LengthSquared() <= 0.0001f)
            axis = Vec3::UnitY.Cross(from);

        if (axis.LengthSquared() <= 0.0001f)
            return Quaternion::Identity;

        axis.Normalize();
        return Quaternion::CreateFromAxisAngle(axis, XM_PI);
    }

    Vec3 axis = from.Cross(to);
    if (axis.LengthSquared() <= 0.0001f)
        return Quaternion::Identity;

    axis.Normalize();

    const float angle = acosf(dot);
    Quaternion q = Quaternion::CreateFromAxisAngle(axis, angle);
    q.Normalize();

    return q;
}

Matrix HairChainPhysics::Build_LocalMatrix_RotationOnly(const Matrix& bindLocal,
    const Quaternion& deltaRotation)
{
    Matrix tempMatrix = bindLocal;

    Vec3 scale{};
    Quaternion bindRotation{};
    Vec3 translation{};

    tempMatrix.Decompose(scale, bindRotation, translation);

    Quaternion finalRotation = deltaRotation * bindRotation;
    finalRotation.Normalize();

    Matrix local = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(finalRotation);

    // 중요:
    // simulated position을 local translation에 쓰지 않는다.
    // 원래 bindLocal translation을 유지해야 mesh가 길게 찢어지지 않는다.
    local._41 = bindLocal._41;
    local._42 = bindLocal._42;
    local._43 = bindLocal._43;
    local._44 = bindLocal._44;

    return local;
}
