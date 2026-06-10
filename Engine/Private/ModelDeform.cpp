#include "pch.h"
#include "ModelDeform.h"

bool ModelDeform::Has_MorphData() const
{
    return !_data.targets.empty() && !_data.meshes.empty();
}

const MorphData* ModelDeform::Get_MorphData() const
{
    return Has_MorphData() ? &_data : nullptr;
}

uint32 ModelDeform::Get_MorphTargetCount() const
{
    return static_cast<uint32>(_data.targets.size());
}

const string* ModelDeform::Get_MorphTargetName(uint32 index) const
{
    if (index >= _data.targets.size())
        return nullptr;

    return &_data.targets[index];
}

int32 ModelDeform::Find_MorphTargetIndex(const string& name) const
{
    const auto iter = _targetLookup.find(name);
    if (iter != _targetLookup.end())
        return static_cast<int32>(iter->second);

    return -1;
}

HRESULT ModelDeform::Set_MorphData(MorphData&& data)
{
    Clear();

    _data = std::move(data);
    if (!Has_MorphData())
        return S_OK;

    _weights.assign(_data.targets.size(), 0.f);

    for (uint32 i = 0; i < _data.targets.size(); ++i)
        _targetLookup.emplace(_data.targets[i], i);

    _dirty = true;
    return S_OK;
}

void ModelDeform::Set_MorphWeight(uint32 targetIndex, float weight)
{
    if (targetIndex >= _weights.size())
        return;

    const float clamped = clamp(weight, -1.f, 1.f);
    if (_weights[targetIndex] == clamped)
        return;

    _weights[targetIndex] = clamped;
    _dirty = true;
}

void ModelDeform::Set_MorphWeight_Unclamped(uint32 targetIndex, float weight)
{
    if (targetIndex >= _weights.size())
        return;

    if (_weights[targetIndex] == weight)
        return;

    _weights[targetIndex] = weight;
    _dirty = true;
}

void ModelDeform::Reset_MorphWeights()
{
    if (_weights.empty())
        return;

    ranges::fill(_weights, 0.f);
    _dirty = true;
}

const vector<float>* ModelDeform::Get_MorphWeights() const
{
    return !_weights.empty() ? &_weights : nullptr;
}

bool ModelDeform::Consume_MorphDirty()
{
    const bool dirty = _dirty;
    _dirty = false;
    return dirty;
}

HRESULT ModelDeform::Try_LoadMorphData(const string& modelPath)
{
    Clear();
    
    fs::path morphPath = String::ToWString(modelPath);
    morphPath.replace_extension(L".morph");
    if (!fs::exists(morphPath))
        return S_OK;

    ifstream in(morphPath, ios::binary);
    if (!in.is_open())
        return E_FAIL;

    auto ReadString = [](ifstream& stream)
    {
        uint32 length{};
        stream.read(reinterpret_cast<char*>(&length), sizeof(uint32));

        string value;
        value.resize(length);
        if (length > 0)
            stream.read(value.data(), length);

        return value;
    };

    uint32 magic{}, version{}, targetCount{}, meshCount{};
    in.read(reinterpret_cast<char*>(&magic), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&version), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&targetCount), sizeof(uint32));
    in.read(reinterpret_cast<char*>(&meshCount), sizeof(uint32));

    constexpr uint32 MORPH_MAGIC = 0x50524F4D;
    constexpr uint32 MORPH_VERSION = 1;
    if (magic != MORPH_MAGIC || version != MORPH_VERSION)
    {
        LOG_ERROR("[MORPH] Invalid morph file header: {}", morphPath.string());
        return E_FAIL;
    }

    MorphData data{};
    data.sourceName = ReadString(in);
    data.targets.reserve(targetCount);
    for (uint32 i = 0; i < targetCount; ++i)
        data.targets.push_back(ReadString(in));

    data.meshes.reserve(meshCount);
    for (uint32 meshIndex = 0; meshIndex < meshCount; ++meshIndex)
    {
        MorphMeshData mesh{};
        uint32 rangeCount{}, deltaCount{};

        in.read(reinterpret_cast<char*>(&mesh.meshIndex), sizeof(uint32));
        mesh.meshName = ReadString(in);
        in.read(reinterpret_cast<char*>(&mesh.vertexCount), sizeof(uint32));
        in.read(reinterpret_cast<char*>(&rangeCount), sizeof(uint32));
        in.read(reinterpret_cast<char*>(&deltaCount), sizeof(uint32));

        mesh.vertexRanges.resize(rangeCount);
        mesh.vertexDeltas.resize(deltaCount);

        if (rangeCount > 0)
        {
            in.read(
                reinterpret_cast<char*>(mesh.vertexRanges.data()),
                static_cast<streamsize>(sizeof(MorphVertexRange) * rangeCount));
        }

        if (deltaCount > 0)
        {
            in.read(
                reinterpret_cast<char*>(mesh.vertexDeltas.data()),
                static_cast<streamsize>(sizeof(MorphVertexDelta) * deltaCount));
        }

        data.meshes.push_back(std::move(mesh));
    }

    if (!in.good() && !in.eof())
    {
        LOG_ERROR("[MORPH] Failed while reading morph file: {}", morphPath.string());
        return E_FAIL;
    }

    LOG_INFO("[MORPH] Loaded morph file: {}, targets={}, meshes={}",
        morphPath.string(), targetCount, meshCount);

    return Set_MorphData(std::move(data));
}

void ModelDeform::Clear()
{
    _dirty = false;
    _data = {};
    _weights.clear();
    _targetLookup.clear();
}

Shared<ModelDeform> ModelDeform::Create()
{
    return make_shared<ModelDeform>();
}

void ModelDeform::Free()
{
    Clear();
    Base::Free();
}
