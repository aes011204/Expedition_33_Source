#pragma once

#include "Client_Defines.h"
#include "Engine_Model_Struct.h"
#include "CollisionProxy_Types.h"

NS_BEGIN(Engine)
class ModelCom;
struct StaticCollisionData;
NS_END

NS_BEGIN(Client)

class MapStaticCollisionBuilder final
{
public:
    static Shared<StaticCollisionData> Build(const Shared<ModelCom>& model);

private:
    static bool Build_TriangleMeshes_FromModelFile(
        const wstring& sourceModelPath,
        vector<ModelCollisionMesh>& outTriangleMeshes);

    static bool Build_PrimitiveProxies_FromSourceJson(
        const wstring& sourceModelPath,
        vector<ModelCollisionProxy>& outPrimitiveProxies);
};

NS_END
