# Lune Coat PoseFollow Data

Date: 2026-06-20

## Current Feedback

- Idle ticking: none
- Run / rotation response: intended feel, accepted
- Upper coat propagation: follows slightly up the coat, accepted
- Remaining issue: Sprint movement can still feel too soft / wobbly
- Note: reducing `ownerMotionScale` to `0.062f` and `maxExternalOffset` to `0.72f` did not fix Sprint wobble and made normal movement worse. Reverted to the better previous motion values.
- Note: later broad damping/follow reduction also felt worse. Restored the earlier movement scalar set and applied per-group control for shorter coat chains.

## Active Path

- Runtime attach: `Client/Private/PW_Lune.cpp`
- Preset builder: `Client/Default/ChainModifierPreset.cpp`
- Function: `ChainModifierPreset::BuildPoseFollowCoatDesc`
- Character branch: `PlayerCharacterType::Lune`
- Runtime modifier: `PoseFollowChainModifier`

## Current Lune Coat Values

```cpp
desc.enableCollision = false;
desc.collisionRadiusPadding = 0.f;
desc.particleCollisionRadius = 0.f;

desc.ownerMotionScale = 0.075f;
desc.animatedMotionScale = 0.55f;
desc.ownerMotionDeadzone = 0.030f;
desc.ownerRotationDeadzoneDegree = 0.40f;
desc.animatedMotionDeadzone = 0.120f;
desc.maxExternalOffset = 0.85f;

desc.damping = 0.82f;
desc.tipDampingScale = 1.12f;

desc.poseFollowStrength = 0.30f;
desc.tipFollowScale = 0.38f;
desc.maxBendAngleDegree = 54.f;
desc.tipBendAngleScale = 2.05f;
desc.bendLimitStrength = 0.48f;
desc.surfaceConstraintStrength = 0.015f;

desc.rootOutputAlpha = 0.12f;
desc.tipOutputAlpha = 0.96f;
desc.idleOutputAlphaScale = 0.55f;
desc.outputSmoothAlpha = 0.36f;
desc.idleOutputSmoothAlpha = 0.90f;
desc.outputDeadzoneDegree = 3.0f;
desc.idleSettleStrength = 0.98f;
desc.sleepVelocityThreshold = 0.055f;
desc.sleepPoseDistance = 0.26f;
```

## Groups

Each group uses:

- Wind multiplier: `0.34f`
- Short chain motion/follow is reduced/stabilized because `01~04` are shorter than `05~07`.
- Fourth value is `rootOutputMul`; it reduces only the root-side write-back for short chains.
- Fifth value is `tipOutputMul`; it reduces only the tip-side write-back for short chains.

```cpp
{ "Lune_V3_Coat_01_01", 0.34f, 1.15f, 0.64f, 0.08f, 0.45f },
{ "Lune_V3_Coat_02_01", 0.34f, 1.15f, 0.64f, 0.08f, 0.45f },
{ "Lune_V3_Coat_03_01", 0.34f, 1.20f, 0.62f, 0.15f, 0.50f },
{ "Lune_V3_Coat_04_01", 0.34f, 1.20f, 0.62f, 0.15f, 0.50f },
{ "Lune_V3_Coat_05_01", 0.34f, 1.45f, 0.56f, 0.40f, 0.68f },
{ "Lune_V3_Coat_06_01", 0.34f, 1.75f, 0.50f, 1.00f, 1.00f },
{ "Lune_V3_Coat_07_01", 0.34f, 1.75f, 0.50f, 1.00f, 1.00f }
```

## Tuning Notes

- Idle stability is currently handled by `outputDeadzoneDegree`, `idleOutputAlphaScale`, `idleSettleStrength`, `sleepVelocityThreshold`, and `sleepPoseDistance`.
- Upper propagation is mainly from `rootOutputAlpha = 0.12f` and group follow `0.54f`.
- Sprint wobble is mainly affected by high-speed owner translation input:
  - `ownerMotionScale`
  - `maxExternalOffset`
  - short-chain group motion/follow, especially `01~04`
- Current scalar-only Sprint attempt rejected:
  - `ownerMotionScale 0.075f -> 0.062f`
  - `maxExternalOffset 0.80f -> 0.72f`
  - Result: Sprint wobble remained, and normal movement became worse.
- Keep `animatedMotionScale`, `rootOutputAlpha`, `poseFollowStrength`, and group follow stable unless run / rotation feel needs to change.
- Next Sprint-only fix should avoid reducing the accepted normal movement values; it likely needs speed-specific input shaping instead of another broad scalar reduction.
- Latest adjustment: reducing return terms did not reduce wobble; it only made the coat softer. Return terms were restored and Tip velocity retention was reduced:
  - `tipDampingScale 1.28f -> 1.12f`
  - `poseFollowStrength 0.285f -> 0.30f`
  - `tipFollowScale 0.37f -> 0.38f`
  - `bendLimitStrength 0.46f -> 0.48f`
- Latest group adjustment for short-chain wobble:
  - stronger pass because previous group adjustment looked unchanged
  - `01~02`: motion `1.35f -> 1.05f`, follow `0.66f -> 0.74f`
  - `03~04`: motion `1.40f -> 1.12f`, follow `0.64f -> 0.70f`
  - `05`: motion `1.62f -> 1.50f`, follow `0.58f -> 0.62f`
  - `06~07`: unchanged
- Latest group adjustment for more natural motion after tip jitter remained:
  - `tipOutputMul` was lowered a bit more on short chains to reduce visible tip shake.
  - motion was partially restored so the coat receives more movement input again.
  - follow was moved back toward the previous/original values because the stronger follow made the coat snap back too quickly.
  - `01~02`: motion `1.05f -> 1.25f`, follow `0.74f -> 0.66f`, tip output `0.55f -> 0.50f`
  - `03~04`: motion `1.12f -> 1.30f`, follow `0.70f -> 0.64f`, tip output `0.65f -> 0.60f`
  - `05`: motion `1.50f -> 1.60f`, follow `0.62f -> 0.58f`, tip output `0.85f -> 0.80f`
  - `06~07`: unchanged
- Follow-up adjustment:
  - `03~04`: tip output `0.60f -> 0.55f`
- Follow-up adjustment:
  - `05`: tip output `0.80f -> 0.75f`
- Follow-up adjustment for `05` chain wobble:
  - `05` has one more bone than `01` (`05_01~05_05` vs `01_01~01_04`) and had much more motion/root/tip output open.
  - `05`: motion `1.60f -> 1.45f`, follow `0.58f -> 0.62f`, root output `0.55f -> 0.40f`, tip output `0.75f -> 0.68f`
- Follow-up adjustment to match `01~04` with the `05` stabilization direction:
  - `01~02`: motion `1.25f -> 1.15f`, follow `0.66f -> 0.70f`, root output `0.10f -> 0.08f`, tip output `0.50f -> 0.45f`
  - `03~04`: motion `1.30f -> 1.20f`, follow `0.64f -> 0.68f`, root output `0.20f -> 0.15f`, tip output `0.55f -> 0.50f`
- Follow opened to reduce stiff / fast pose-follow feel:
  - Lune `01~02`: follow `0.70f -> 0.64f`
  - Lune `03~04`: follow `0.68f -> 0.62f`
  - Lune `05`: follow `0.62f -> 0.56f`
  - Lune `06~07`: follow `0.54f -> 0.50f`
  - Maelle `02`: follow `0.70f -> 0.64f`
  - Gustave `08`: follow `0.62f -> 0.56f`
- Maelle / Gustave root opened to reduce overly locked upper-coat feel:
  - Maelle `02`: root output `0.08f -> 0.14f`
  - Gustave `08`: root output `0.40f -> 0.48f`
- Length-compensated Maelle / Gustave coat pass:
  - Maelle `02` is shorter than Lune, so motion and output were opened while follow was lowered:
    - motion `1.15f -> 1.25f`
    - follow `0.64f -> 0.58f`
    - root output `0.14f -> 0.20f`
    - tip output `0.45f -> 0.58f`
  - Gustave `08` is longer than Lune-short chains, so motion was reduced while output was opened:
    - motion `1.45f -> 1.35f`
    - follow `0.56f -> 0.52f`
    - root output `0.48f -> 0.55f`
    - tip output `0.68f -> 0.74f`
- Maelle / Gustave opened a little more:
  - Maelle `02`: motion `1.25f -> 1.32f`, root output `0.20f -> 0.26f`, tip output `0.58f -> 0.66f`, follow kept `0.58f`
  - Gustave `08`: motion `1.35f -> 1.40f`, root output `0.55f -> 0.64f`, tip output `0.74f -> 0.82f`, follow kept `0.52f`
- Maelle `02` opened further because the 3-bone chain still looked under-driven:
  - motion `1.32f -> 1.45f`
  - follow `0.58f -> 0.54f`
  - root output `0.26f -> 0.34f`
  - tip output `0.66f -> 0.78f`
- Root output group control added for short-chain upper jitter:
  - `01~02`: root output multiplier `0.10f`
  - `03~04`: root output multiplier `0.20f`
  - `05`: root output multiplier `0.55f`
  - `06~07`: root output multiplier `1.00f`
- Tip output group control added for remaining short-chain tip jitter:
  - `01~02`: tip output multiplier `0.55f`
  - `03~04`: tip output multiplier `0.65f`
  - `05`: tip output multiplier `0.85f`
  - `06~07`: tip output multiplier `1.00f`
- Previous group adjustment for short-chain wobble:
  - `01~02`: motion `1.55f -> 1.35f`, follow `0.60f -> 0.66f`
  - `03~04`: motion `1.58f -> 1.40f`, follow `0.59f -> 0.64f`
  - `05`: motion `1.68f -> 1.62f`, follow `0.56f -> 0.58f`
  - `06~07`: unchanged
- Previous midpoint adjustment:
  - `poseFollowStrength 0.27f -> 0.285f`
  - `tipFollowScale 0.36f -> 0.37f`
  - `bendLimitStrength 0.44f -> 0.46f`
- Previous too-soft adjustment:
  - `poseFollowStrength 0.30f -> 0.27f`
  - `tipFollowScale 0.38f -> 0.36f`
  - `bendLimitStrength 0.48f -> 0.44f`
- Measured Lune coat chain lengths:
  - `01~04`: about `46.95~48.69`
  - `05`: about `59.99`
  - `06`: about `65.95`
  - `07`: about `71.49`
