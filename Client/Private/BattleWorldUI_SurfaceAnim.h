#pragma once

NS_BEGIN(Client)

struct BattleWorldSurfaceAnimState
{
    float elapsedSeconds = 0.f;
    float delaySeconds = 0.f;
    bool playing = false;
};

struct BattleWorldSurfaceAnimRenderState
{
    Matrix worldMatrix = Matrix::Identity;
    Vec4 color = Vec4(1.f, 1.f, 1.f, 1.f);
};

constexpr float kBattleWorldSurfaceAnimIntroDuration = 0.18f;
constexpr float kBattleWorldSurfaceAnimStaggerDelay = 0.025f;
constexpr float kBattleWorldSurfaceAnimStartScale = 0.92f;

inline float Saturate_BattleWorldSurfaceAnim(float value)
{
    if (value <= 0.f)
        return 0.f;

    if (value >= 1.f)
        return 1.f;

    return value;
}

inline float EaseOutCubic_BattleWorldSurfaceAnim(float value)
{
    const float t = Saturate_BattleWorldSurfaceAnim(value);
    const float inv = 1.f - t;
    return 1.f - inv * inv * inv;
}

inline void Restart_BattleWorldSurfaceAnim(BattleWorldSurfaceAnimState& state, uint32 surfaceIndex)
{
    state.elapsedSeconds = 0.f;
    state.delaySeconds = static_cast<float>(surfaceIndex) * kBattleWorldSurfaceAnimStaggerDelay;
    state.playing = true;
}

inline void Advance_BattleWorldSurfaceAnim(BattleWorldSurfaceAnimState& state, float timeDelta)
{
    if (!state.playing)
        return;

    if (timeDelta > 0.f)
        state.elapsedSeconds += timeDelta;

    const float endTime = state.delaySeconds + kBattleWorldSurfaceAnimIntroDuration;
    if (state.elapsedSeconds >= endTime)
    {
        state.elapsedSeconds = endTime;
        state.playing = false;
    }
}

inline BattleWorldSurfaceAnimRenderState Apply_BattleWorldSurfaceAnim(
    const BattleWorldSurfaceAnimState& state,
    const Matrix& baseWorldMatrix,
    const Vec4& baseColor,
    const Vec3& startWorldOffset)
{
    BattleWorldSurfaceAnimRenderState renderState{};
    renderState.worldMatrix = baseWorldMatrix;
    renderState.color = baseColor;

    if (!state.playing)
        return renderState;

    const float localTime = state.elapsedSeconds - state.delaySeconds;
    const float progress = EaseOutCubic_BattleWorldSurfaceAnim(
        localTime / kBattleWorldSurfaceAnimIntroDuration);
    const float scale = kBattleWorldSurfaceAnimStartScale +
        (1.f - kBattleWorldSurfaceAnimStartScale) * progress;
    const Vec3 offset = startWorldOffset * (1.f - progress);

    renderState.worldMatrix =
        Matrix::CreateScale(scale, scale, 1.f) *
        baseWorldMatrix *
        Matrix::CreateTranslation(offset.x, offset.y, offset.z);
    renderState.color.w *= progress;
    return renderState;
}

NS_END
