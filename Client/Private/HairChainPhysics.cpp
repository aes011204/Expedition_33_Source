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

void HairChainPhysics::Simulate(ModelCom* model, float timeDelta, const Matrix& ownerWorld)
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

    // 현재 애니메이션 pose 기준으로 root는 매 프레임 고정한다.
    // child의 parent가 root면 constraint가 root를 따라가면서 자연스럽게 흔들린다.
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

    Solve_Verlet(timeDelta);
    Write_BackToModel(model);
}

void HairChainPhysics::Initialize_Chains(ModelCom* model)
{
    // ModelCom의 bone 목록을 순회하면서 Strand_* 수집

    umap<string, uint32> chainIndexByKey;
    umap<uint32, pair<uint32, uint32>> nodeLookup;

    const uint32 numBones = static_cast<uint32>(model->Get_NumBones());

    for (uint32 boneIndex = 0; boneIndex < numBones; ++boneIndex)
    {
        const string& boneName = model->Get_BoneName(boneIndex);
        if (!Is_HairBoneName(boneName, _desc.bonePrefix))
            continue;

        const string chainKey = Get_ChainKey(boneName);

        uint32 chainIndex = 0;
        const auto found = chainIndexByKey.find(chainKey);
        if (found == chainIndexByKey.end())
        {
            chainIndex = static_cast<uint32>(_chains.size());
            chainIndexByKey.emplace(chainKey, chainIndex);
            _chains.push_back(Chain{});
        }
        else
        {
            chainIndex = found->second;
        }

        Chain& chain = _chains[chainIndex];

        Node node{};
        node.boneIndex = boneIndex;
        node.bindLocal = model->Get_BoneLocalMatrix(boneIndex);

        const int32 parentBoneIndex = model->Get_BoneParentIndex(boneIndex);
        auto parentNodeIt = parentBoneIndex >= 0
            ? nodeLookup.find(static_cast<uint32>(parentBoneIndex)) : nodeLookup.end();

        if (parentNodeIt != nodeLookup.end())
            node.parentNodeIndex = static_cast<int32>(parentNodeIt->second.second);
        else
            node.parentNodeIndex = -1;

        const uint32 nodeIndex = static_cast<uint32>(chain.nodes.size());
        chain.nodes.push_back(node);
        nodeLookup.emplace(boneIndex, make_pair(chainIndex, nodeIndex));
    }

    // 너무 은 체인은 시뮬레이션 가치가 낮아서 제거
    _chains.erase(
        remove_if(_chains.begin(), _chains.end(),
            [](const Chain& chain)
            {
                return chain.nodes.size() < 2;
            }),
        _chains.end());
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
            }
            else
            {
                node.length = 0.f;
            }
        }
    }
}

void HairChainPhysics::Solve_Verlet(float timeDelta)
{
    // Verlet 기본:
    // newPos = pos + (pos - prevPos) * damping + accel * timeDelta^2
    const float timeDeltaClamped = ::min(timeDelta, 1.f / 30.f);
    const float keepVelocity = ::clamp(1.f - _desc.damping, 0.f, 1.f);

    // v1 wind.
    // 나중에 player velocity나 noise를 넣어도 됨.
    const float t = static_cast<float>(GetTickCount64() % 100000) * 0.001f;
    const Vec3 wind = Vec3(
        sinf(t * 1.7f) * _desc.windScale,
        0.f,
        cosf(t * 1.3f) * _desc.windScale
    );

    const Vec3 accel = _desc.gravity + wind;

    for (Chain& chain : _chains)
    {
        // --------------------------------------------------------
        // 1. Verlet integration
        // --------------------------------------------------------
        for (uint32 i = 1; i < static_cast<uint32>(chain.nodes.size()); ++i)
        {
            Node& node = chain.nodes[i];

            const Vec3 current = node.position;
            const Vec3 velocity = (node.position - node.prevPosition) * keepVelocity;

            node.position += velocity + accel * (timeDeltaClamped * timeDeltaClamped);
            node.prevPosition = current;
        }

        // --------------------------------------------------------
        // 2. Length constraint
        // --------------------------------------------------------
        // parent-child 거리를 bind length로 유지한다.
        // stiffness는 원래 위치로 끌어당기는 강도처럼 사용.
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

                // stiffness가 낮을수록 부드럽게 따라감.
                node.position = Vector3::Lerp(node.position, target, ::clamp(_desc.stiffness * 20.f, 0.f, 1.f));
            }
        }
    }

}

void HairChainPhysics::Write_BackToModel(ModelCom* model)
{
    // Verlet 결과 world position을 ModelCom의 bone local matrix로 반영
    if (model == nullptr)
        return;

    for (Chain& chain : _chains)
    {
        for (uint32 i = 1; i < static_cast<uint32>(chain.nodes.size()); ++i)
        {
            Node& node = chain.nodes[i];
            Node& parent = chain.nodes[node.parentNodeIndex];

            const Matrix* parentWorld = model->Get_BoneMatrixPtr(parent.boneIndex);
            if (parentWorld == nullptr)
                continue;

            Vec3 dir = node.position - parent.position;
            if (dir.LengthSquared() > 0.0001f)
                dir.Normalize();
            else
                dir = Vec3(0.f, -1.f, 0.f);

            Matrix simulatedParentWorld = *parentWorld;
            simulatedParentWorld._41 = parent.position.x;
            simulatedParentWorld._42 = parent.position.y;
            simulatedParentWorld._43 = parent.position.z;

            const Matrix local = Build_LocalMatrix_FromDirection(
                node.bindLocal,
                simulatedParentWorld,
                node.position,
                dir
            );

            model->Set_BoneLocalMatrix(node.boneIndex, local);
        }
    }

    model->Rebuild_BoneCombinedMatrices();
}

bool HairChainPhysics::Is_HairBoneName(const string& name, const string& prefix)
{
    // Strand_로 시작하는 bone만 hair physcis 대상으로 처리
    return name.find(prefix, 0) == 0;
}

string HairChainPhysics::Get_ChainKey(const string& boneName)
{
    // Strand_1_1, Strand_1_1_1 이렇게 돼있어서 같은 체인으로 처리
    vector<string> parts;
    size_t start = 0;

    while (start < boneName.size())
    {
        const size_t pos = boneName.find('_', start);
        if (pos == string::npos)
        {
            parts.push_back(boneName.substr(start));
            break;
        }

        parts.push_back(boneName.substr(start, pos - start));
        start = pos + 1;
    }

    if (parts.size() < 3)
        return boneName;

    return parts[0] + "_" + parts[1] + "_" + parts[2];
}

Vec3 HairChainPhysics::Get_WorldPosition(const Matrix& m)
{
    return Vec3(m._41, m._42, m._43);
}

Matrix HairChainPhysics::Build_LocalMatrix_FromDirection(const Matrix& bindLocal, const Matrix& parentWorld,
    const Vec3& worldPosition, const Vec3& childDirection)
{
    Matrix parentInv = parentWorld;
    parentInv.Invert();

    const Vec3 localPosition = Vector3::Transform(worldPosition, parentInv);

    Matrix local = bindLocal;
    local._41 = localPosition.x;
    local._42 = localPosition.y;
    local._43 = localPosition.z;

    return local;
}
