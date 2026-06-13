#pragma once
#include "EditorBlock.h"

NS_BEGIN(Engine)
struct MorphData;
class ModelCom;
NS_END

NS_BEGIN(ModelEditor)

class ModelTestPanel final : public EditorBlock
{
public:
    ModelTestPanel();
    ~ModelTestPanel() override = default;

private:
    HRESULT Initialize() override;

public:
    void Update(float timeDelta) override;
    void Render() override;

private:
    Engine::ModelCom* Get_CurrentModelComponent() const;

    void Render_MorphTargetTest();
    void Render_SelectedMorphDebug(const Engine::MorphData& morphData, const vector<float>* weights) const;
    void Render_MotionWarpingTest();

private: /* Motion Warping Test */
    bool    _testWarpingActive = false;
    float   _testTargetDistance = 5.0f;
    float   _testSpeed = 1.0f;
    float   _testPrevRatio = 0.0f;
    Vec3    _testStartPos = {};

private: /* Morph Target Test */
    char    _morphFilter[128]{};
    int32   _selectedMorphIndex = -1;
    float   _selectedMorphWeight = 0.f;

public:
    static Shared<ModelTestPanel> Create();
};

NS_END
