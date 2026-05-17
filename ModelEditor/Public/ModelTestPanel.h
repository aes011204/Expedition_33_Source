#pragma once
#include "EditorBlock.h"

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
    void Render_MotionWarpingTest();

private: /* Motion Warping Test 관련 */
    bool    _testWarpingActive = false;
    float   _testTargetDistance = 5.0f;
    float   _testSpeed = 1.0f;
    float   _testPrevRatio = 0.0f;
    Vec3    _testStartPos = {};

    int32   _selectedAnimIndex = -1;

public:
	static Shared<ModelTestPanel> Create();
};

NS_END
