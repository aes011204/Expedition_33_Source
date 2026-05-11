#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(MapEditor)

struct SceneContext;

class MEDebugRenderer
{
private:
    static constexpr float _lineLen = 0.4f;
    static constexpr Color _lineCol = { 0.9f, 0.3f, 0.9f, 1.f };

public:
    explicit MEDebugRenderer() = default;
    ~MEDebugRenderer() = default;

public:
    void Draw(const SceneContext& scene);

public:
    static Unique<MEDebugRenderer> Create();
};

NS_END
