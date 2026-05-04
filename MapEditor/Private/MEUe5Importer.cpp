#include "MEUe5Importer.h"
#include "SceneContext.h"

Shared<EditorObject> MEUe5Importer::ParseInstanced(const json& comp, int instanceIndex)
{
    const auto& instance = comp["PerInstanceSMData"][instanceIndex];
    if (!instance.contains("TransformData"))
    {
        return nullptr;
    }

    const auto& td = instance["TransformData"];
    const auto& rot = td["Rotation"];
    const auto& tr = td["Translation"];
    const auto& sc = td["Scale3D"];

    auto obj = make_shared<EditorObject>();
    obj->_position = ConvertTranslation(tr["X"], tr["Y"], tr["Z"]);
    obj->_rotation = ConvertRotation(rot["X"], rot["Y"], rot["Z"], rot["W"]);
    obj->_scale = ConvertScale(sc["X"], sc["Y"], sc["Z"]);

    if (comp.contains("Properties") && comp["Properties"].contains("StaticMesh"))
    {
        obj->_modelPath = String::ToWString(ExtractMeshName(comp["Properties"]["StaticMesh"]["ObjectPath"].get<string>()));
    }

    obj->_name = obj->_modelPath + L"_" + to_wstring(instanceIndex);
    obj->UpdateWorldMatrix();
    return obj;
}

Shared<EditorObject> MEUe5Importer::ParseSingle(const json& comp)
{
    if (!comp.contains("Properties"))
    {
        return nullptr;
    }

    const auto& props = comp["Properties"];

    auto obj = make_shared<EditorObject>();

    if (props.contains("RelativeLocation"))
    {
        const auto& loc = props["RelativeLocation"];
        obj->_position = ConvertTranslation(loc["X"], loc["Y"], loc["Z"]);
    }

    if (props.contains("RelativeRotation"))
    {
        const auto& rot = props["RelativeRotation"];
        float pitch = rot["Pitch"].get<float>();
        float yaw = rot["Yaw"].get<float>();
        float roll = rot["Roll"].get<float>();

        Quat qp = Quat::CreateFromAxisAngle(Vec3::Right, XMConvertToRadians(-pitch));
        Quat qy = Quat::CreateFromAxisAngle(Vec3::Up, XMConvertToRadians(-yaw));
        Quat qr = Quat::CreateFromAxisAngle(Vec3::Forward, XMConvertToRadians(roll));
        obj->_rotation = qr * qp * qy;
    }

    if (props.contains("RelativeScale3D"))
    {
        const auto& sc = props["RelativeScale3D"];
        obj->_scale = ConvertScale(sc["X"], sc["Y"], sc["Z"]);
    }

    if (props.contains("StaticMesh"))
    {
        obj->_modelPath = String::ToWString(ExtractMeshName(props["StaticMesh"]["ObjectPath"].get<string>()));
    }

    obj->_name = obj->_modelPath + L"_0";
    obj->UpdateWorldMatrix();
    return obj;
}

Vec3 MEUe5Importer::ConvertTranslation(float ux, float uy, float uz)
{
    return Vec3(ux / 100.f, uz / 100.f, uy / 100.f);
}

Quat MEUe5Importer::ConvertRotation(float qx, float qy, float qz, float qw)
{
    return Quat(qx, qz, qy, -qw);
}

Vec3 MEUe5Importer::ConvertScale(float sx, float sy, float sz)
{
    return Vec3(sx, sz, sy);
}

string MEUe5Importer::ExtractMeshName(const string& objectPath)
{
    // "/Game/EnviroPacks/.../SM_Small_Rock_4.2" → "SM_Small_Rock_4"
    size_t slash = objectPath.rfind('/');
    string filename = (slash != string::npos) ? objectPath.substr(slash + 1) : objectPath;

    size_t dot = filename.rfind('.');
    if (dot != string::npos)
    {
        filename = filename.substr(0, dot);
    }
    return filename;
}

void MEUe5Importer::Import(const string& filePath, SceneContext& ctx, bool append)
{
    ifstream ifs(filePath);
    if (!ifs.is_open())
    {
        return;
    }

    json root;
    try
    {
        root = json::parse(ifs);
    }
    catch(...)
    {
        return;
    }

    if (!append)
    {
        ctx._objects.clear();
        ctx._selected = nullptr;
    }

    if (!root.is_array())
    {
        return;
    }

    for (auto& comp : root)
    {
        if (!comp.contains("Type"))
        {
            continue;
        }

        const string type = comp["Type"].get<string>();

        if (type == "InstancedStaticMeshComponent")
        {
            if (!comp.contains("perInstanceSMData"))
            {
                continue;
            }

            int index = {};
            for (auto& instance : comp["PerInstanceSMData"])
            {
                auto obj = ParseInstanced(comp, index++);
                if (obj)
                {
                    ctx.Add(obj);
                }
            }
        }
        else if (type == "StaticMeshComponent")
        {
            auto obj = ParseSingle(comp);
            if (obj)
            {
                ctx.Add(obj);
            }
        }
    }
}
