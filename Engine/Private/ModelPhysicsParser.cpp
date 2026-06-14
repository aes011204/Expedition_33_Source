#include "pch.h"
#include "ModelPhysicsParser.h"

vector<ChainModifier::CapsuleColliderDesc> ModelPhysicsParser::LoadCapsulesFromJSON(const wstring& filePath)
{
    vector<ChainModifier::CapsuleColliderDesc> colliders;
    filesystem::path resolvedPath = filePath;

    if (!filesystem::exists(resolvedPath))
    {
        const filesystem::path fileName = filesystem::path(filePath).filename();
        filesystem::path cursor = filesystem::current_path();
        for (int32 i = 0; i < 6 && !cursor.empty(); ++i)
        {
            const filesystem::path clientCandidate = cursor / L"Client" / L"Bin" / L"Resources" / L"ModelPhysics" / fileName;
            if (filesystem::exists(clientCandidate))
            {
                resolvedPath = clientCandidate;
                break;
            }

            const filesystem::path binCandidate = cursor / L"Bin" / L"Resources" / L"ModelPhysics" / fileName;
            if (filesystem::exists(binCandidate))
            {
                resolvedPath = binCandidate;
                break;
            }

            cursor = cursor.parent_path();
        }
    }

    if (!filesystem::exists(resolvedPath))
    {
        return colliders;
    }

    std::ifstream file(resolvedPath);
    if (!file.is_open())
        return colliders;

    nlohmann::json data{};
    try {
        file >> data;
    }
    catch (...) {
        return colliders;
    }

    if (!data.is_array())
        return colliders;

    for (const auto& item : data)
    {
        ChainModifier::CapsuleColliderDesc desc;
        desc.boneName = item.value("boneName", "");

        auto center = item["center"];
        desc.center = Vec3(center.value("x", 0.f), center.value("y", 0.f), center.value("z", 0.f));

        auto rotation = item["rotation"];
        float halfPitch = XMConvertToRadians(rotation.value("pitch", 0.f)) * 0.5f;
        float halfYaw = XMConvertToRadians(rotation.value("yaw", 0.f)) * 0.5f;
        float halfRoll = XMConvertToRadians(rotation.value("roll", 0.f)) * 0.5f;

        float pitchSin, pitchCos, yawSin, yawCos, rollSin, rollCos;
        // 3차원 회전을 쿼터니언으로 만들려면, 
        // X축 회전 쿼터니언, Y축 회전 쿼터니언, Z축 회전 쿼터니언을 각각 만든 다음 세 개를 서로 곱해줘야
        // ($Q_{yaw} * Q_{pitch} * Q_{roll}$) 합니다.
        XMScalarSinCos(&pitchSin, &pitchCos, halfPitch);
        XMScalarSinCos(&yawSin, &yawCos, halfYaw);
        XMScalarSinCos(&rollSin, &rollCos, halfRoll);

        desc.rotationQuat.x = (rollCos * pitchSin * yawSin) - (rollSin * pitchCos * yawCos);
        desc.rotationQuat.y = (-rollCos * pitchSin * yawCos) - (rollSin * pitchCos * yawSin);
        desc.rotationQuat.z = (rollCos * pitchCos * yawSin) - (rollSin * pitchSin * yawCos);
        desc.rotationQuat.w = (rollCos * pitchCos * yawCos) + (rollSin * pitchSin * yawSin);

        desc.radius = item.value("radius", 0.f);
        desc.halfLength = item.value("length", 0.f) * 0.5f;

        colliders.push_back(desc);
    }

    return colliders;
}
