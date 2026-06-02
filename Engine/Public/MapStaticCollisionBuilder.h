#pragma once

#include "Engine_Define.h"
#include "Engine_Model_Struct.h"
#include "CollisionProxy_Types.h"

NS_BEGIN(Engine)

class ModelCom;
struct StaticCollisionData;

class ENGINE_DLL MapStaticCollisionBuilder final
{
public:
    static Shared<StaticCollisionData> Build(const Shared<ModelCom>& model);
    static bool Build_TriangleMeshes_FromModelFile(
        const wstring& sourceModelPath,
        vector<ModelCollisionMesh>& outTriangleMeshes);

private:
    static bool Build_ConvexTriangleMeshes_FromSourceJson(
        const wstring& sourceModelPath,
        vector<ModelCollisionMesh>& outTriangleMeshes);

    static bool Build_PrimitiveProxies_FromSourceJson(
        const wstring& sourceModelPath,
        vector<ModelCollisionProxy>& outPrimitiveProxies);
};

NS_END
