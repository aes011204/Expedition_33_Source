#pragma once
#include "Base.h"

NS_BEGIN(Engine)

class ModelDeform final : public Base
{
public:
    ModelDeform() = default;
    ~ModelDeform() override = default;

public:
    bool Has_MorphData() const;
    const MorphData* Get_MorphData() const;

    uint32 Get_MorphTargetCount() const;
    const string* Get_MorphTargetName(uint32 index) const;
    int32 Find_MorphTargetIndex(const string& name) const;

    HRESULT Set_MorphData(MorphData&& data);
    void Set_MorphWeight(uint32 targetIndex, float weight);
    void Set_MorphWeight_Unclamped(uint32 targetIndex, float weight);
    void Reset_MorphWeights();
    const vector<float>* Get_MorphWeights() const;
    bool Consume_MorphDirty();

    HRESULT Try_LoadMorphData(const string& modelPath);
    void Clear();

private:
    bool _dirty{ false };
    MorphData _data{};
    vector<float> _weights{};
    umap<string, uint32> _targetLookup{};

public:
    static Shared<ModelDeform> Create();
    void Free() override;

};

NS_END
