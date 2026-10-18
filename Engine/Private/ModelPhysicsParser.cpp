#include "pch.h"
#include "ModelPhysicsParser.h"

namespace
{
    Vec3 Convert_UePointToEngine(const Vec3& uePoint)
    {
        return Vec3(uePoint.x, uePoint.z, -uePoint.y);
    }

    Quaternion Convert_UeRotationToEngine(const Quaternion& ueRotation)
    {
        const Matrix basisToEngine = Matrix::CreateRotationX(-XM_PIDIV2);
        const Matrix basisToUe = Matrix::CreateRotationX(XM_PIDIV2);
        const Matrix ueRotationMatrix = Matrix::CreateFromQuaternion(ueRotation);
        const Matrix engineRotationMatrix = basisToEngine * ueRotationMatrix * basisToUe;

        Vec3 scale = Vec3::One;
        Quaternion rotation = Quaternion::Identity;
        Vec3 translation = Vec3::Zero;

        Matrix tempMat = engineRotationMatrix;
        tempMat.Decompose(scale, rotation, translation);
        return rotation;
    }

    Quaternion Parse_UePrimitiveRotation(const nlohmann::json& rotationNode)
    {
        return Quaternion::CreateFromYawPitchRoll(
            XMConvertToRadians(rotationNode.value("yaw", 0.f)),
            XMConvertToRadians(rotationNode.value("pitch", 0.f)),
            XMConvertToRadians(rotationNode.value("roll", 0.f)));
    }
}

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
        const Vec3 ueCenter(center.value("x", 0.f), center.value("y", 0.f), center.value("z", 0.f));
        desc.center = Convert_UePointToEngine(ueCenter);

        auto rotation = item["rotation"];
        desc.rotationQuat = Convert_UeRotationToEngine(Parse_UePrimitiveRotation(rotation));

        desc.radius = item.value("radius", 0.f);
        desc.halfLength = std::max(0.f, item.value("length", 0.f) * 0.5f - desc.radius);

        colliders.push_back(desc);
    }

    return colliders;
}
