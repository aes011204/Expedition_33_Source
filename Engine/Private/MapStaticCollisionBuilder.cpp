#include "MapStaticCollisionBuilder.h"
#include "StaticCollisionData.h"
#include "ModelCom.h"
#include <fstream>

namespace
{
    Vec3 Convert_UePointToEngine(const Vec3& uePoint)
    {
        // UE source json is Z-up. Runtime world/collision code here treats Y as up.
        return Vec3(uePoint.x, uePoint.z, -uePoint.y);
    }

    Vec3 Convert_UeExtentsToEngine(const Vec3& ueExtents)
    {
        return Vec3(
            fabsf(ueExtents.x),
            fabsf(ueExtents.z),
            fabsf(ueExtents.y));
    }

    Quat Convert_UeRotationToEngine(const Quat& ueRotation)
    {
        const Matrix basisToEngine = Matrix::CreateRotationX(-XM_PIDIV2);
        const Matrix basisToUe = Matrix::CreateRotationX(XM_PIDIV2);
        const Matrix ueRotationMatrix = Matrix::CreateFromQuaternion(ueRotation);
        const Matrix engineRotationMatrix = basisToEngine * ueRotationMatrix * basisToUe;

        Vec3 scale = Vec3::One;
        Quat rotation = Quat::Identity;
        Vec3 translation = Vec3::Zero;

        Matrix tempMat = engineRotationMatrix;
        tempMat.Decompose(scale, rotation, translation);
        return rotation;
    }

    Quat Parse_UePrimitiveRotation(const json& rotationNode)
    {
        return Quat::CreateFromYawPitchRoll(
            XMConvertToRadians(rotationNode.value("Yaw", 0.f)),
            XMConvertToRadians(rotationNode.value("Pitch", 0.f)),
            XMConvertToRadians(rotationNode.value("Roll", 0.f)));
    }
}

NS_BEGIN(Engine)

Shared<StaticCollisionData> MapStaticCollisionBuilder::Build(const Shared<ModelCom>& model)
{
    if (model == nullptr)
        return nullptr;

    auto data = make_shared<StaticCollisionData>();
    data->cacheKey = model->Get_SourceModelPath();
    data->preLocalTransform = model->Get_PreLocalTransformMatrix();

    // 1. JSON 파일이 존재하는지 확인하고 바디셋업 정보를 파싱합니다.
    bool hasBodySetup = false;
    bool useComplexAsSimple = false;

    const fs::path jsonPath = fs::path(data->cacheKey).replace_extension(".json");
    if (fs::exists(jsonPath))
    {
        ifstream ifs(jsonPath);
        if (ifs.is_open())
        {
            json root = json::parse(ifs, nullptr, false);
            if (!root.is_discarded() && root.is_array())
            {
                for (const json& entry : root)
                {
                    if (entry.value("Type", "") == "BodySetup")
                    {
                        hasBodySetup = true;
                        const json* props = entry.contains("Properties") ? &entry["Properties"] : nullptr;
                        if (props != nullptr && props->is_object())
                        {
                            const string traceFlag = props->value("CollisionTraceFlag", "");
                            if (traceFlag == "ECollisionTraceFlag::CTF_UseComplexAsSimple")
                            {
                                useComplexAsSimple = true;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    // 2. 바디셋업이 명시적으로 존재하지 않는다면, 이 모델은 충돌체가 필요 없는 데코레이션 프롭(풀, 꽃, 자갈돌 등)으로 간주하여 충돌체를 생성하지 않습니다.
    if (!hasBodySetup)
    {
        Build_TriangleMeshes_FromModelFile(data->cacheKey, data->triangleMeshes);
    }

    // 3. 바디셋업이 존재할 때만 설정에 따라 충돌체를 조건부 생성합니다.
    else if (useComplexAsSimple)
    {
        Build_TriangleMeshes_FromModelFile(data->cacheKey, data->triangleMeshes);
    }
    else
    {
        Build_PrimitiveProxies_FromSourceJson(data->cacheKey, data->primitiveProxies);
    }

    if (data->primitiveProxies.empty() && data->triangleMeshes.empty())
        return nullptr;

    return data;
}

bool MapStaticCollisionBuilder::Build_TriangleMeshes_FromModelFile(const wstring& sourceModelPath,
    vector<ModelCollisionMesh>& outTriangleMeshes)
{
    outTriangleMeshes.clear();

    const string pathA = String::ToString(sourceModelPath);
    ifstream stream(pathA, ios::binary);
    if (!stream.is_open())
        return false;

    auto ReadString = [](ifstream& in)
    {
        uint32 len = 0;
        in.read(reinterpret_cast<char*>(&len), sizeof(uint32));

        string str;
        if (len > 0)
        {
            str.resize(len);
            in.read(&str[0], len);
        }
        return str;
    };

    bool isSkeletal = false;
    uint32 maxTextureType = 0;
    uint32 numMaterials = 0;
    uint32 numMeshes = 0;
    uint32 numBones = 0;

    stream.read(reinterpret_cast<char*>(&isSkeletal), sizeof(bool));
    stream.read(reinterpret_cast<char*>(&maxTextureType), sizeof(uint32));
    stream.read(reinterpret_cast<char*>(&numMaterials), sizeof(uint32));
    stream.read(reinterpret_cast<char*>(&numMeshes), sizeof(uint32));
    stream.read(reinterpret_cast<char*>(&numBones), sizeof(uint32));

    UNREFERENCED_PARAMETER(maxTextureType);
    UNREFERENCED_PARAMETER(numBones);

    if (isSkeletal)
        return false;

    // material block skip
    for (uint32 i = 0; i < numMaterials; ++i)
    {
        ReadString(stream); // material name

        Vec4 dummyVec4[4]{};
        float dummyFloat[4]{};
        uint32 numSlots = 0;

        stream.read(reinterpret_cast<char*>(dummyVec4), sizeof(Vec4) * 4);
        stream.read(reinterpret_cast<char*>(dummyFloat), sizeof(float) * 4);
        stream.read(reinterpret_cast<char*>(&numSlots), sizeof(uint32));

        for (uint32 j = 0; j < numSlots; ++j)
        {
            ReadString(stream); // texture path

            uint32 dummyTypeIndex[2]{};
            Vec2 dummyUvVec2[2]{};
            float dummyUvRot[2]{};
            uint32 dummyUInt[3]{};

            stream.read(reinterpret_cast<char*>(dummyTypeIndex), sizeof(uint32) * 2);
            stream.read(reinterpret_cast<char*>(dummyUvVec2), sizeof(Vec2) * 2);
            stream.read(reinterpret_cast<char*>(dummyUvRot), sizeof(float) * 2);
            stream.read(reinterpret_cast<char*>(dummyUInt), sizeof(uint32) * 3);
        }
    }

    for (uint32 i = 0; i < numMeshes; ++i)
    {
        ModelCollisionMesh collisionMesh{};
        collisionMesh.name = ReadString(stream);

        uint32 materialIndex = 0;
        uint32 numVertices = 0;
        uint32 numIndices = 0;
        uint32 numBoneRefs = 0;

        stream.read(reinterpret_cast<char*>(&materialIndex), sizeof(uint32));
        stream.read(reinterpret_cast<char*>(&numVertices), sizeof(uint32));
        stream.read(reinterpret_cast<char*>(&numIndices), sizeof(uint32));
        stream.read(reinterpret_cast<char*>(&numBoneRefs), sizeof(uint32));
        UNREFERENCED_PARAMETER(materialIndex);

        vector<VTXMESH> vertices(numVertices);
        stream.read(reinterpret_cast<char*>(vertices.data()), sizeof(VTXMESH) * numVertices);

        vector<uint32> indices(numIndices);
        stream.read(reinterpret_cast<char*>(indices.data()), sizeof(uint32) * numIndices);

        if (numBoneRefs > 0)
        {
            vector<uint32> boneIndices(numBoneRefs);
            vector<Matrix> offsetMatrices(numBoneRefs);
            stream.read(reinterpret_cast<char*>(boneIndices.data()), sizeof(uint32) * numBoneRefs);
            stream.read(reinterpret_cast<char*>(offsetMatrices.data()), sizeof(Matrix) * numBoneRefs);
        }

        collisionMesh.positions.reserve(numVertices);

        for (const VTXMESH& vertex : vertices)
            collisionMesh.positions.push_back(vertex.position);

        collisionMesh.indices = std::move(indices);

        outTriangleMeshes.push_back(std::move(collisionMesh));
    }

    return !outTriangleMeshes.empty();
}

bool MapStaticCollisionBuilder::Build_PrimitiveProxies_FromSourceJson(const wstring& sourceModelPath,
    vector<ModelCollisionProxy>& outPrimitiveProxies)
{
    outPrimitiveProxies.clear();

    const fs::path jsonPath = fs::path(sourceModelPath).replace_extension(".json");
    if (!fs::exists(jsonPath))
        return false;

    ifstream ifs(jsonPath);
    if (!ifs.is_open())
        return false;

    json root = json::parse(ifs, nullptr, false);
    if (root.is_discarded() || !root.is_array())
        return false;

    bool addedAny = false;
    bool handledSimpleOnlyWithoutFallback = false;

    for (const json& entry : root)
    {
        if (entry.value("Type", "") != "BodySetup")
            continue;

        const json* props = entry.contains("Properties") ? &entry["Properties"] : nullptr;
        if (props == nullptr || !props->is_object())
            continue;

        const string traceFlag = props->value("CollisionTraceFlag", "");

        // complex-as-simple은 primitive를 만들지 않고 triangle fallback 쪽으로 넘긴다.
        if (traceFlag == "ECollisionTraceFlag::CTF_UseComplexAsSimple")
            continue;

        const bool simpleOnlyTrace =
            (traceFlag == "ECollisionTraceFlag::CTF_UseSimpleAsComplex");

        if (!props->contains("AggGeom") || !(*props)["AggGeom"].is_object())
            continue;

        const json& aggGeom = (*props)["AggGeom"];
        if (simpleOnlyTrace)
            handledSimpleOnlyWithoutFallback = true;

        auto appendBox = [&](const json& boxElem)
        {
            ModelCollisionProxy proxy{};
            proxy.type = ModelCollisionProxyType::Box;
            const Vec3 ueCenter(
                boxElem["Center"].value("X", 0.f),
                boxElem["Center"].value("Y", 0.f),
                boxElem["Center"].value("Z", 0.f));
            const Quat ueRotation = Parse_UePrimitiveRotation(boxElem["Rotation"]);
            const Vec3 ueExtents(
                boxElem.value("X", 0.f) * 0.5f,
                boxElem.value("Y", 0.f) * 0.5f,
                boxElem.value("Z", 0.f) * 0.5f);
            proxy.center = Convert_UePointToEngine(ueCenter);
            proxy.rotation = Convert_UeRotationToEngine(ueRotation);
            proxy.extents = Convert_UeExtentsToEngine(ueExtents);
            outPrimitiveProxies.push_back(std::move(proxy));
            addedAny = true;
        };

        auto appendSphere = [&](const json& sphereElem)
        {
            ModelCollisionProxy proxy{};
            proxy.type = ModelCollisionProxyType::Sphere;
            const Vec3 ueCenter(
                sphereElem["Center"].value("X", 0.f),
                sphereElem["Center"].value("Y", 0.f),
                sphereElem["Center"].value("Z", 0.f));
            proxy.center = Convert_UePointToEngine(ueCenter);
            proxy.radius = sphereElem.value("Radius", 0.f);
            outPrimitiveProxies.push_back(std::move(proxy));
            addedAny = true;
        };

        auto appendCapsule = [&](const json& sphylElem)
        {
            ModelCollisionProxy proxy{};
            proxy.type = ModelCollisionProxyType::Capsule;
            const Vec3 ueCenter(
                sphylElem["Center"].value("X", 0.f),
                sphylElem["Center"].value("Y", 0.f),
                sphylElem["Center"].value("Z", 0.f));
            const Quat ueRotation = Parse_UePrimitiveRotation(sphylElem["Rotation"]);
            proxy.center = Convert_UePointToEngine(ueCenter);
            proxy.rotation = Convert_UeRotationToEngine(ueRotation);
            proxy.radius = sphylElem.value("Radius", 0.f);
            proxy.halfHeight = max(0.f, sphylElem.value("Length", 0.f) * 0.5f - proxy.radius);
            outPrimitiveProxies.push_back(std::move(proxy));
            addedAny = true;
        };

        if (aggGeom.contains("BoxElems"))
        {
            for (const json& boxElem : aggGeom["BoxElems"])
                appendBox(boxElem);
        }

        if (aggGeom.contains("SphereElems"))
        {
            for (const json& sphereElem : aggGeom["SphereElems"])
                appendSphere(sphereElem);
        }

        if (aggGeom.contains("SphylElems"))
        {
            for (const json& sphylElem : aggGeom["SphylElems"])
                appendCapsule(sphylElem);
        }
    }

    if (addedAny)
        return true;

    // simple-only인데 primitive를 못 읽었으면 triangle fallback 안 하고 충돌 생략
    return handledSimpleOnlyWithoutFallback;
}

NS_END
