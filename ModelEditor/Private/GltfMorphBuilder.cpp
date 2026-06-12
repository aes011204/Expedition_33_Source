#include "pch.h"
#include "GltfMorphBuilder.h"

#include <commdlg.h>

#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define STB_IMAGE_IMPLEMENTATION
#include <tiny_gltf.h>

#pragma comment(lib, "Comdlg32.lib")

NS_BEGIN(ModelEditor)

namespace
{
    constexpr uint32 MORPH_MAGIC = 0x50524F4D; // 'MORP'
    constexpr uint32 MORPH_VERSION = 1;
    constexpr float POSITION_EPSILON = 1e-7f;
    constexpr float NORMAL_EPSILON = 1e-6f;
}

GltfMorphBuilder::GltfMorphBuilder() : EditorBlock{}
{
}

HRESULT GltfMorphBuilder::Initialize()
{
    return S_OK;
}

void GltfMorphBuilder::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);
}

void GltfMorphBuilder::Render()
{
    if (!_showBlock)
        return;

    ImGui::Begin("GLB Morph Builder");

    Render_PathRow("Model", _modelPathBuffer, sizeof(_modelPathBuffer), L"Model Files (*.model)\0*.model\0All Files (*.*)\0*.*\0");
    Render_PathRow("GLB", _glbPathBuffer, sizeof(_glbPathBuffer), L"glTF Binary (*.glb)\0*.glb\0All Files (*.*)\0*.*\0");

    ImGui::Separator();
    if (ImGui::Button("Build .morph", ImVec2(-1.f, 30.f)) && !_isBuilding)
    {
        _isBuilding = true;
        if (FAILED(Build_MorphFile()))
            LOG_ERROR("[GLTF MORPH] Build failed: {}", _status);
        _isBuilding = false;
    }

    ImGui::TextWrapped("%s", _status.c_str());

    ImGui::End();
}

void GltfMorphBuilder::Render_PathRow(const char* label, char* buffer, size_t bufferSize, const wchar_t* filter)
{
    ImGui::Text("%s", label);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-85.f);
    ImGui::InputText((string("##") + label).c_str(), buffer, bufferSize);
    ImGui::SameLine();
    if (ImGui::Button((string("Browse##") + label).c_str(), ImVec2(75.f, 0.f)))
        Browse_File(filter, buffer, bufferSize);
}

void GltfMorphBuilder::Set_Status(string status) const
{
    _status = std::move(status);
    LOG_INFO("[GLTF MORPH] {}", _status);
}

bool GltfMorphBuilder::Browse_File(const wchar_t* filter, char* outBuffer, size_t bufferSize) const
{
    wchar_t filePath[MAX_PATH]{};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameW(&ofn))
        return false;

    strcpy_s(outBuffer, bufferSize, ToString(filePath).c_str());
    return true;
}

HRESULT GltfMorphBuilder::Build_MorphFile()
{
    CHECK_FAILED(Validate_InputPaths(), E_FAIL);

    const fs::path modelPath = String::ToWString(_modelPathBuffer);
    const fs::path glbPath = String::ToWString(_glbPathBuffer);

    vector<ModelMeshHeader> modelMeshes;
    vector<string> targetNames;
    vector<PrimitiveMorphSource> glbSources;

    CHECK_FAILED(Load_ModelMeshHeaders(modelPath, modelMeshes), E_FAIL);
    CHECK_FAILED(Load_GlbMorphSources(glbPath, targetNames, glbSources), E_FAIL);

    MorphData morphData{};
    CHECK_FAILED(Match_AndBuildMorphData(modelMeshes, targetNames, glbSources, morphData), E_FAIL);

    const fs::path outputPath = fs::path(modelPath).replace_extension(L".morph");
    CHECK_FAILED(Export_MorphBinary(outputPath, morphData), E_FAIL);

    uint32 deltaCount = 0;
    for (const MorphMeshData& mesh : morphData.meshes)
        deltaCount += static_cast<uint32>(mesh.vertexDeltas.size());

    Set_Status("Built: " + outputPath.string() +
        " | targets=" + to_string(morphData.targets.size()) +
        ", meshes=" + to_string(morphData.meshes.size()) +
        ", deltas=" + to_string(deltaCount));
    return S_OK;
}

HRESULT GltfMorphBuilder::Validate_InputPaths() const
{
    const array<pair<const char*, fs::path>, 2> paths =
    {
        pair{ "model", fs::path(String::ToWString(_modelPathBuffer)) },
        pair{ "glb", fs::path(String::ToWString(_glbPathBuffer)) }
    };

    for (const auto& [label, path] : paths)
    {
        if (path.empty() || !fs::exists(path))
        {
            Set_Status(string("Missing ") + label + " path.");
            return E_FAIL;
        }
    }

    return S_OK;
}

HRESULT GltfMorphBuilder::Load_ModelMeshHeaders(const fs::path& modelPath, vector<ModelMeshHeader>& outMeshes) const
{
    ifstream in(modelPath, ios::binary);
    if (!in.is_open())
    {
        Set_Status("Failed to open model: " + modelPath.string());
        return E_FAIL;
    }

    bool isSkeletal{};
    uint32 texSize{}, materialCount{}, meshCount{}, boneCount{};
    Read_Raw(in, isSkeletal);
    Read_Raw(in, texSize);
    Read_Raw(in, materialCount);
    Read_Raw(in, meshCount);
    Read_Raw(in, boneCount);

    // Skip material payload exactly as ModelConverter writes it.
    for (uint32 i = 0; i < materialCount; ++i)
    {
        Read_String(in);
        in.seekg(sizeof(Vec4) * 4, ios::cur);
        in.seekg(sizeof(float) * 4, ios::cur);

        uint32 slotCount{};
        Read_Raw(in, slotCount);
        for (uint32 slotIndex = 0; slotIndex < slotCount; ++slotIndex)
        {
            Read_String(in);
            in.seekg(sizeof(uint32) * 2, ios::cur);
            in.seekg(sizeof(Vec2) * 2, ios::cur);
            in.seekg(sizeof(float) * 2, ios::cur);
            in.seekg(sizeof(uint32) * 3, ios::cur);
        }
    }

    outMeshes.clear();
    outMeshes.reserve(meshCount);

    for (uint32 meshIndex = 0; meshIndex < meshCount; ++meshIndex)
    {
        ModelMeshData meshHeader{};
        meshHeader.name = Read_String(in);
        Read_Raw(in, meshHeader.materialIndex);
        Read_Raw(in, meshHeader.numVertices);
        Read_Raw(in, meshHeader.numIndices);
        Read_Raw(in, meshHeader.numBoneRefs);

        const size_t vertexStride = isSkeletal ? sizeof(VTXANIMMESH) : sizeof(VTXMESH);
        vector<Vec3> positions(meshHeader.numVertices);
        for (uint32 vertexIndex = 0; vertexIndex < meshHeader.numVertices; ++vertexIndex)
        {
            Read_Raw(in, positions[vertexIndex]);
            in.seekg(static_cast<streamoff>(vertexStride - sizeof(Vec3)), ios::cur);
        }

        in.seekg(static_cast<streamoff>(meshHeader.numIndices) * sizeof(uint32), ios::cur);

        if (isSkeletal && meshHeader.numBoneRefs > 0)
        {
            in.seekg(static_cast<streamoff>(meshHeader.numBoneRefs) * sizeof(uint32), ios::cur);
            in.seekg(static_cast<streamoff>(meshHeader.numBoneRefs) * sizeof(Matrix), ios::cur);
        }

        outMeshes.push_back({ meshHeader.name, meshHeader.materialIndex, meshHeader.numVertices, std::move(positions) });
    }

    if (outMeshes.empty())
    {
        Set_Status("Model has no mesh data.");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT GltfMorphBuilder::Load_GlbMorphSources(
    const fs::path& glbPath,
    vector<string>& outTargetNames,
    vector<PrimitiveMorphSource>& outSources) const
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    string error;
    string warning;

    if (!loader.LoadBinaryFromFile(&model, &error, &warning, glbPath.string()))
    {
        Set_Status("Failed to load glb: " + error);
        return E_FAIL;
    }

    if (!warning.empty())
        LOG_WARN("[GLTF MORPH] tinygltf warning: {}", warning);

    if (model.meshes.empty())
    {
        Set_Status("GLB has no mesh.");
        return E_FAIL;
    }

    const tinygltf::Mesh& mesh = model.meshes.front();
    Extract_GlbTargetNames(model, outTargetNames);
    outSources.clear();

    for (uint32 primitiveIndex = 0; primitiveIndex < mesh.primitives.size(); ++primitiveIndex)
    {
        const tinygltf::Primitive& primitive = mesh.primitives[primitiveIndex];
        if (primitive.targets.empty())
            continue;

        const auto posAttr = primitive.attributes.find("POSITION");
        if (posAttr == primitive.attributes.end())
            continue;

        const tinygltf::Accessor& basePosAccessor = model.accessors[posAttr->second];
        const uint32 vertexCount = static_cast<uint32>(basePosAccessor.count);

        PrimitiveMorphSource source{};
        source.primitiveIndex = primitiveIndex;
        source.materialIndex = primitive.material >= 0 ? static_cast<uint32>(primitive.material) : primitiveIndex;
        source.vertexCount = vertexCount;
        CHECK_FAILED(Read_GltfVec3Accessor(model, posAttr->second, source.basePositions), E_FAIL);
        for (Vec3& position : source.basePositions)
            position = Transform_GlbPosition(position);

        // FModel GLB stores morph deltas in primitive target order.
        // The target slot index is the only stable key we can rely on here.
        for (uint32 localIndex = 0; localIndex < primitive.targets.size(); ++localIndex)
        {
            const auto& target = primitive.targets[localIndex];
            const auto positionIter = target.find("POSITION");
            if (positionIter == target.end())
            {
                Set_Status("Morph target missing POSITION accessor.");
                return E_FAIL;
            }

            source.positionDeltas.push_back({});
            CHECK_FAILED(Read_GltfVec3Accessor(model, positionIter->second, source.positionDeltas.back()), E_FAIL);
            if (source.positionDeltas.back().size() != vertexCount)
            {
                Set_Status("Morph POSITION count mismatch at primitive " + to_string(primitiveIndex) + ", target " + to_string(localIndex));
                return E_FAIL;
            }
            for (Vec3& delta : source.positionDeltas.back())
                delta = Transform_GlbDeltaPosition(delta);

            const auto normalIter = target.find("NORMAL");
            source.normalDeltas.push_back({});
            if (normalIter != target.end())
            {
                CHECK_FAILED(Read_GltfVec3Accessor(model, normalIter->second, source.normalDeltas.back()), E_FAIL);
                if (source.normalDeltas.back().size() != vertexCount)
                {
                    Set_Status("Morph NORMAL count mismatch at primitive " + to_string(primitiveIndex) + ", target " + to_string(localIndex));
                    return E_FAIL;
                }
                for (Vec3& delta : source.normalDeltas.back())
                    delta = Transform_GlbNormal(delta);
            }
            else
                source.normalDeltas.back().assign(source.positionDeltas.back().size(), Vec3::Zero);

            source.targetIndices.push_back(localIndex);
        }

        outSources.push_back(std::move(source));
    }

    if (outSources.empty())
    {
        Set_Status("No usable GLB morph primitive found.");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT GltfMorphBuilder::Match_AndBuildMorphData(
    const vector<ModelMeshHeader>& modelMeshes,
    const vector<string>& targetNames,
    const vector<PrimitiveMorphSource>& glbSources,
    MorphData& outData) const
{
    outData = {};
    outData.sourceName = fs::path(String::ToWString(_modelPathBuffer)).stem().string();
    outData.targets = targetNames;

    uint32 maxTargetIndex = 0;
    for (const PrimitiveMorphSource& source : glbSources)
    {
        if (!source.targetIndices.empty())
            maxTargetIndex = max(maxTargetIndex, *max_element(source.targetIndices.begin(), source.targetIndices.end()));
    }

    while (outData.targets.size() <= maxTargetIndex)
        outData.targets.push_back("glb_target_" + to_string(outData.targets.size()));

    if (outData.targets.size() > maxTargetIndex + 1)
    {
        LOG_WARN("[GLTF MORPH] targetNames count is larger than GLB target slots: names={}, slots={}",
            outData.targets.size(), maxTargetIndex + 1);
    }

    struct PendingMorphMesh
    {
        MorphMeshData mesh{};
        vector<vector<MorphVertexDelta>> deltasByTarget;
    };

    vector<PendingMorphMesh> pendingMeshes;
    unordered_map<uint32, uint32> pendingIndexByMesh;

    for (const PrimitiveMorphSource& source : glbSources)
    {
        uint32 modelMeshIndex = uint32(-1);
        for (uint32 meshIndex = 0; meshIndex < modelMeshes.size(); ++meshIndex)
        {
            if (modelMeshes[meshIndex].materialIndex == source.materialIndex)
            {
                modelMeshIndex = meshIndex;
                break;
            }
        }

        if (modelMeshIndex == uint32(-1))
        {
            Set_Status("No model mesh for GLB material " + to_string(source.materialIndex) +
                " at primitive " + to_string(source.primitiveIndex));
            return E_FAIL;
        }

        const ModelMeshHeader& modelMesh = modelMeshes[modelMeshIndex];

        vector<vector<uint32>> vertexMap;
        uint32 mappedVertexCount = 0;
        const array<float, 3> keyScales{ 1000.f, 100.f, 10.f };

        for (float scale : keyScales)
        {
            unordered_map<string, vector<uint32>> modelVertexBuckets;
            modelVertexBuckets.reserve(modelMesh.positions.size());
            for (uint32 vertexIndex = 0; vertexIndex < modelMesh.positions.size(); ++vertexIndex)
                modelVertexBuckets[Make_PositionKey(modelMesh.positions[vertexIndex], scale)].push_back(vertexIndex);

            vector<vector<uint32>> candidateMap(source.basePositions.size());
            uint32 candidateMapped = 0;
            for (uint32 vertexIndex = 0; vertexIndex < source.basePositions.size(); ++vertexIndex)
            {
                const auto iter = modelVertexBuckets.find(Make_PositionKey(source.basePositions[vertexIndex], scale));
                if (iter == modelVertexBuckets.end())
                    continue;

                candidateMap[vertexIndex] = iter->second;
                ++candidateMapped;
            }

            if (candidateMapped > mappedVertexCount)
            {
                mappedVertexCount = candidateMapped;
                vertexMap = std::move(candidateMap);
            }

            if (candidateMapped == source.vertexCount)
                break;
        }

        if (mappedVertexCount == 0)
        {
            Set_Status("Failed to remap GLB primitive " + to_string(source.primitiveIndex) +
                " to model mesh " + to_string(modelMeshIndex));
            return E_FAIL;
        }

        if (mappedVertexCount != source.vertexCount)
        {
            LOG_WARN("[GLTF MORPH] primitive {} remap partial: mapped={}/{} to model mesh {}",
                source.primitiveIndex, mappedVertexCount, source.vertexCount, modelMeshIndex);
        }

        uint32 pendingIndex = 0;
        const auto pendingIter = pendingIndexByMesh.find(modelMeshIndex);
        if (pendingIter == pendingIndexByMesh.end())
        {
            PendingMorphMesh pending{};
            pending.mesh.meshIndex = modelMeshIndex;
            pending.mesh.meshName = modelMesh.name;
            pending.mesh.vertexCount = modelMesh.vertexCount;
            pending.mesh.vertexRanges.resize(outData.targets.size());
            pending.deltasByTarget.resize(outData.targets.size());

            pendingIndex = static_cast<uint32>(pendingMeshes.size());
            pendingIndexByMesh.emplace(modelMeshIndex, pendingIndex);
            pendingMeshes.push_back(std::move(pending));
        }
        else
        {
            pendingIndex = pendingIter->second;
        }

        PendingMorphMesh& pending = pendingMeshes[pendingIndex];
        vector<uint32> writeStamps(modelMesh.vertexCount, 0u);
        uint32 stamp = 1u;

        for (uint32 localTargetIndex = 0; localTargetIndex < source.targetIndices.size(); ++localTargetIndex)
        {
            const uint32 globalTargetIndex = source.targetIndices[localTargetIndex];
            if (globalTargetIndex >= pending.deltasByTarget.size())
                continue;

            const vector<Vec3>& positions = source.positionDeltas[localTargetIndex];
            const vector<Vec3>& normals = source.normalDeltas[localTargetIndex];
            ++stamp;

            for (uint32 vertexIndex = 0; vertexIndex < positions.size(); ++vertexIndex)
            {
                const Vec3 normalDelta = vertexIndex < normals.size() ? normals[vertexIndex] : Vec3::Zero;
                if (Is_NearlyZero(positions[vertexIndex], POSITION_EPSILON) &&
                    Is_NearlyZero(normalDelta, NORMAL_EPSILON))
                {
                    continue;
                }

                if (vertexIndex >= vertexMap.size())
                    continue;

                for (uint32 mappedVertexIndex : vertexMap[vertexIndex])
                {
                    if (mappedVertexIndex >= modelMesh.vertexCount || writeStamps[mappedVertexIndex] == stamp)
                        continue;

                    writeStamps[mappedVertexIndex] = stamp;

                    MorphVertexDelta delta{};
                    delta.vertexIndex = mappedVertexIndex;
                    delta.weightScale = 1.f;
                    delta.deltaPosition = positions[vertexIndex];
                    delta.deltaNormal = normalDelta;
                    pending.deltasByTarget[globalTargetIndex].push_back(delta);
                }
            }
        }
    }

    for (PendingMorphMesh& pending : pendingMeshes)
    {
        for (uint32 targetIndex = 0; targetIndex < pending.deltasByTarget.size(); ++targetIndex)
        {
            vector<MorphVertexDelta>& deltas = pending.deltasByTarget[targetIndex];
            MorphVertexRange& range = pending.mesh.vertexRanges[targetIndex];
            range.start = static_cast<uint32>(pending.mesh.vertexDeltas.size());
            range.count = static_cast<uint32>(deltas.size());

            pending.mesh.vertexDeltas.insert(
                pending.mesh.vertexDeltas.end(),
                make_move_iterator(deltas.begin()),
                make_move_iterator(deltas.end()));
        }

        if (!pending.mesh.vertexDeltas.empty())
            outData.meshes.push_back(std::move(pending.mesh));
    }

    if (outData.meshes.empty())
    {
        Set_Status("Morph data has no non-zero deltas.");
        return E_FAIL;
    }

    return S_OK;
}

HRESULT GltfMorphBuilder::Export_MorphBinary(const fs::path& outputPath, const MorphData& data) const
{
    ofstream out(outputPath, ios::binary);
    if (!out.is_open())
    {
        Set_Status("Failed to open output: " + outputPath.string());
        return E_FAIL;
    }

    const uint32 targetCount = static_cast<uint32>(data.targets.size());
    const uint32 meshCount = static_cast<uint32>(data.meshes.size());

    Write_Raw(out, MORPH_MAGIC);
    Write_Raw(out, MORPH_VERSION);
    Write_Raw(out, targetCount);
    Write_Raw(out, meshCount);

    Write_String(out, data.sourceName);
    for (const string& target : data.targets)
        Write_String(out, target);

    for (const MorphMeshData& mesh : data.meshes)
    {
        const uint32 rangeCount = static_cast<uint32>(mesh.vertexRanges.size());
        const uint32 deltaCount = static_cast<uint32>(mesh.vertexDeltas.size());

        Write_Raw(out, mesh.meshIndex);
        Write_String(out, mesh.meshName);
        Write_Raw(out, mesh.vertexCount);
        Write_Raw(out, rangeCount);
        Write_Raw(out, deltaCount);

        if (rangeCount > 0)
            out.write(reinterpret_cast<const char*>(mesh.vertexRanges.data()), sizeof(MorphVertexRange) * rangeCount);
        if (deltaCount > 0)
            out.write(reinterpret_cast<const char*>(mesh.vertexDeltas.data()), sizeof(MorphVertexDelta) * deltaCount);
    }

    return S_OK;
}

string GltfMorphBuilder::Read_String(ifstream& in)
{
    uint32 length{};
    Read_Raw(in, length);

    string value;
    value.resize(length);
    if (length > 0)
        in.read(value.data(), length);

    return value;
}

void GltfMorphBuilder::Write_String(ofstream& out, const string& value)
{
    const uint32 length = static_cast<uint32>(value.size());
    Write_Raw(out, length);
    if (length > 0)
        out.write(value.data(), length);
}

bool GltfMorphBuilder::Is_NearlyZero(const Vec3& value, float epsilon)
{
    return fabsf(value.x) <= epsilon && fabsf(value.y) <= epsilon && fabsf(value.z) <= epsilon;
}

Vec3 GltfMorphBuilder::Transform_GlbPosition(const Vec3& value)
{
    return Vec3(value.x * 100.f, -value.z * 100.f, -value.y * 100.f);
}

Vec3 GltfMorphBuilder::Transform_GlbDeltaPosition(const Vec3& value)
{
    return Vec3(value.x * 100.f, -value.z * 100.f, -value.y * 100.f);
}

Vec3 GltfMorphBuilder::Transform_GlbNormal(const Vec3& value)
{
    return Vec3(value.x, -value.z, -value.y);
}

string GltfMorphBuilder::Make_PositionKey(const Vec3& value, float scale)
{
    const int x = static_cast<int>(roundf(value.x * scale));
    const int y = static_cast<int>(roundf(value.y * scale));
    const int z = static_cast<int>(roundf(value.z * scale));
    return to_string(x) + "," + to_string(y) + "," + to_string(z);
}

void GltfMorphBuilder::Extract_GlbTargetNames(
    const tinygltf::Model& model,
    vector<string>& outTargetNames)
{
    outTargetNames.clear();

    if (!model.meshes.empty())
    {
        const tinygltf::Value& extras = model.meshes.front().extras;

        // FModel writes targetNames into mesh.extras as a JSON string in some GLB exports.
        if (extras.IsString())
        {
            try
            {
                const json extraJson = json::parse(extras.Get<string>());
                if (extraJson.contains("targetNames") && extraJson["targetNames"].is_array())
                {
                    for (const json& name : extraJson["targetNames"])
                    {
                        if (name.is_string())
                            outTargetNames.push_back(name.get<string>());
                    }
                }
            }
            catch (...)
            {
                outTargetNames.clear();
            }
        }
        else if (extras.IsObject() && extras.Has("targetNames"))
        {
            const tinygltf::Value& names = extras.Get("targetNames");
            if (names.IsArray())
            {
                for (size_t i = 0; i < names.ArrayLen(); ++i)
                {
                    const tinygltf::Value& name = names.Get(i);
                    if (name.IsString())
                        outTargetNames.push_back(name.Get<string>());
                }
            }
        }
    }

    if (!outTargetNames.empty())
    {
        LOG_INFO("[GLTF MORPH] target names loaded from GLB extras: {}", outTargetNames.size());
        return;
    }

    LOG_WARN("[GLTF MORPH] GLB targetNames not found. Fallback names will be generated.");
}

uint32 GltfMorphBuilder::Get_GltfTypeElementCount(int type)
{
    switch (type)
    {
    case TINYGLTF_TYPE_SCALAR: return 1;
    case TINYGLTF_TYPE_VEC2: return 2;
    case TINYGLTF_TYPE_VEC3: return 3;
    case TINYGLTF_TYPE_VEC4: return 4;
    default: return 0;
    }
}

HRESULT GltfMorphBuilder::Read_GltfVec3Accessor(
    const tinygltf::Model& model,
    int accessorIndex,
    vector<Vec3>& outValues) const
{
    if (accessorIndex < 0 || accessorIndex >= static_cast<int>(model.accessors.size()))
        return E_FAIL;

    const tinygltf::Accessor& accessor = model.accessors[accessorIndex];
    if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        accessor.type != TINYGLTF_TYPE_VEC3 ||
        accessor.bufferView < 0)
    {
        Set_Status("Only float VEC3 GLB morph accessors are supported.");
        return E_FAIL;
    }

    const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
    const tinygltf::Buffer& buffer = model.buffers[view.buffer];
    const uint32 elementCount = Get_GltfTypeElementCount(accessor.type);
    const size_t elementSize = sizeof(float) * elementCount;
    const size_t stride = accessor.ByteStride(view) > 0 ? accessor.ByteStride(view) : elementSize;
    const size_t baseOffset = view.byteOffset + accessor.byteOffset;

    outValues.resize(accessor.count);
    for (size_t i = 0; i < accessor.count; ++i)
    {
        const size_t offset = baseOffset + i * stride;
        if (offset + elementSize > buffer.data.size())
            return E_FAIL;

        const float* values = reinterpret_cast<const float*>(buffer.data.data() + offset);
        outValues[i] = Vec3(values[0], values[1], values[2]);
    }

    return S_OK;
}

Shared<GltfMorphBuilder> GltfMorphBuilder::Create()
{
    auto instance = make_shared<GltfMorphBuilder>();
    if (FAILED(instance->Initialize()))
        return nullptr;

    return instance;
}

NS_END
