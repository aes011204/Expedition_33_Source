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

    const float clamped = clamp(weight, 0.f, 1.f);
    if (_weights[targetIndex] == clamped)
        return;

    _weights[targetIndex] = clamped;
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
    UNREFERENCED_PARAMETER(modelPath);
    Clear();
    return S_OK;
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
