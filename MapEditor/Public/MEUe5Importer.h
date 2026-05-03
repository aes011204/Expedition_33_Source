#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(MapEditor)

struct SceneContext;
struct EditorObject;

class MEUe5Importer
{
private:
    static Shared<EditorObject> ParseInstanced(const json& comp, int instanceIndex);
    static  Shared<EditorObject> ParseSingle(const json& comp);

    static Vec3 ConvertTranslation(float ux, float uy, float uz);
    static Quat ConvertRotation(float qx, float qy, float qz, float qw);
    static Vec3 ConvertScale(float sx, float sy, float sz);

    static string ExtractMeshName(const string& objectPath);

public:
    // UE5 FModel JSON → SceneContext 배치
   // append: true -> 기존 레벨에 추가, false -> 레벨 초기화 후 추가
    static void Import(const string& filePath, SceneContext& ctx, bool append = false);



};

NS_END
