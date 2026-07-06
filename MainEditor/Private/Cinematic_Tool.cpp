#include "Cinematic_Tool.h"
#include "BattleCameraClipIO.h"
#include "BattleClipSequencerAdapter.h"
#include "Cinematic_Utils.h"
#include "FreeCamera.h"
#include "BattleTurnCamera.h"
#include "BattleController.h"
#include "ClientInstance.h"
#include "Character.h"
#include "ContainerObject.h"
#include "GameInstance.h"
#include "Helper_String.h"
#include "../Public/ImSequencer.h"
#include "ModelCom.h"
#include "PartObject.h"
#include "TransformCom.h"
#include <algorithm>
#include <cmath>
#include "CinematicSequencerAdapter.h"
#include "Cinematic_SequenceIO.h"
#include "EditorInstance.h"
#include "magic_enum/magic_enum.hpp"
#include "BattleAction.h"
#include "CinematicCamera.h"

namespace
{
constexpr float kBattleClipPreviewFps = 30.f;

template <typename TValue>
void Append_UniqueValue(vector<TValue>& values, const TValue& candidate)
{
    if constexpr (std::is_same_v<TValue, string>)
    {
        if (candidate.empty())
            return;
    }

    if (std::find(values.begin(), values.end(), candidate) == values.end())
        values.push_back(candidate);
}

vector<string> Collect_TargetIdSuggestions(const CINEMATIC_SEQUENCE& sequence)
{
    vector<string> values{};

    for (const auto& event : sequence.eventTrack)
        Append_UniqueValue(values, event.targetId);

    for (const auto& track : sequence.actorRelativeCameraTracks)
        Append_UniqueValue(values, track.targetId);

    for (const auto& track : sequence.actorTransformTracks)
        Append_UniqueValue(values, track.targetId);

    for (const auto& track : sequence.partRotationTracks)
        Append_UniqueValue(values, track.targetId);

    return values;
}

vector<string> Collect_SpawnPrefabSuggestions(const CINEMATIC_SEQUENCE& sequence)
{
    vector<string> values{};
    for (const auto& event : sequence.eventTrack)
    {
        if (const auto* spawn = get_if<CinematicSpawnEventData>(&event.payload))
            Append_UniqueValue(values, spawn->prefabName);
    }

    return values;
}

vector<string> Collect_PartTagSuggestions(const CINEMATIC_SEQUENCE& sequence)
{
    vector<string> values{};
    for (const auto& event : sequence.eventTrack)
    {
        if (const auto* partVisibility = get_if<CinematicPartVisibilityEventData>(&event.payload))
            Append_UniqueValue(values, partVisibility->partTag);
    }

    for (const auto& track : sequence.partRotationTracks)
        Append_UniqueValue(values, track.partTag);

    return values;
}

vector<string> Collect_LayerTagSuggestions(const CINEMATIC_SEQUENCE& sequence)
{
    vector<string> values{};
    for (const auto& event : sequence.eventTrack)
    {
        if (const auto* spawn = get_if<CinematicSpawnEventData>(&event.payload))
            Append_UniqueValue(values, String::ToString(spawn->layerTag));
    }

    return values;
}

vector<string> Collect_SoundCueSuggestions(const CINEMATIC_SEQUENCE& sequence)
{
    vector<string> values{};
    for (const auto& event : sequence.eventTrack)
    {
        if (const auto* playSound = get_if<CinematicPlaySoundEventData>(&event.payload))
            Append_UniqueValue(values, String::ToString(playSound->soundCue));
    }

    return values;
}

bool Draw_StringSuggestionCombo(const char* label, string& value, const vector<string>& suggestions)
{
    if (suggestions.empty())
        return false;

    const char* previewLabel = value.empty() ? "<select>" : value.c_str();
    if (!ImGui::BeginCombo(label, previewLabel))
        return false;

    bool changed = false;
    for (const string& suggestion : suggestions)
    {
        const bool isSelected = (suggestion == value);
        if (ImGui::Selectable(suggestion.c_str(), isSelected))
        {
            value = suggestion;
            changed = true;
        }

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndCombo();
    return changed;
}

bool Are_EventPayloadTypesEqual(const CINEMATIC_EVENT& lhs, const CINEMATIC_EVENT& rhs)
{
    return lhs.payload.index() == rhs.payload.index();
}

bool Event_AffectsCurrentPreview(
    const CINEMATIC_EVENT& event,
    int currentFrame,
    bool previewApplyEventTrack)
{
    return previewApplyEventTrack && event.frame <= currentFrame;
}

bool Should_RebuildPreviewForEventEdit(
    const CINEMATIC_EVENT& before,
    const CINEMATIC_EVENT& after,
    int currentFrame,
    bool previewApplyEventTrack)
{
    if (!Event_AffectsCurrentPreview(before, currentFrame, previewApplyEventTrack) &&
        !Event_AffectsCurrentPreview(after, currentFrame, previewApplyEventTrack))
    {
        return false;
    }

    if (before.frame != after.frame ||
        before.targetId != after.targetId ||
        !Are_EventPayloadTypesEqual(before, after))
    {
        return true;
    }

    if (const auto* beforeSpawn = get_if<CinematicSpawnEventData>(&before.payload))
    {
        const auto* afterSpawn = get_if<CinematicSpawnEventData>(&after.payload);
        if (afterSpawn == nullptr)
            return true;

        return beforeSpawn->prefabName != afterSpawn->prefabName ||
            beforeSpawn->layerTag != afterSpawn->layerTag ||
            beforeSpawn->position.x != afterSpawn->position.x ||
            beforeSpawn->position.y != afterSpawn->position.y ||
            beforeSpawn->position.z != afterSpawn->position.z ||
            beforeSpawn->rotation.x != afterSpawn->rotation.x ||
            beforeSpawn->rotation.y != afterSpawn->rotation.y ||
            beforeSpawn->rotation.z != afterSpawn->rotation.z ||
            beforeSpawn->cleanupPolicy != afterSpawn->cleanupPolicy;
    }

    if (const auto* beforePlayAnim = get_if<CinematicPlayAnimEventData>(&before.payload))
    {
        const auto* afterPlayAnim = get_if<CinematicPlayAnimEventData>(&after.payload);
        if (afterPlayAnim == nullptr)
            return true;

        return beforePlayAnim->animName != afterPlayAnim->animName ||
            beforePlayAnim->blendDuration != afterPlayAnim->blendDuration ||
            beforePlayAnim->loop != afterPlayAnim->loop ||
            beforePlayAnim->startFrame != afterPlayAnim->startFrame ||
            beforePlayAnim->speed != afterPlayAnim->speed ||
            beforePlayAnim->holdFrame != afterPlayAnim->holdFrame;
    }

    if (const auto* beforePlayEffect = get_if<CinematicPlayEffectEventData>(&before.payload))
    {
        const auto* afterPlayEffect = get_if<CinematicPlayEffectEventData>(&after.payload);
        if (afterPlayEffect == nullptr)
            return true;

        return beforePlayEffect->effectName != afterPlayEffect->effectName ||
            beforePlayEffect->target != afterPlayEffect->target ||
            beforePlayEffect->partTag != afterPlayEffect->partTag ||
            beforePlayEffect->socketName != afterPlayEffect->socketName ||
            beforePlayEffect->layerTag != afterPlayEffect->layerTag ||
            beforePlayEffect->localOffset.x != afterPlayEffect->localOffset.x ||
            beforePlayEffect->localOffset.y != afterPlayEffect->localOffset.y ||
            beforePlayEffect->localOffset.z != afterPlayEffect->localOffset.z ||
            beforePlayEffect->localRotation.x != afterPlayEffect->localRotation.x ||
            beforePlayEffect->localRotation.y != afterPlayEffect->localRotation.y ||
            beforePlayEffect->localRotation.z != afterPlayEffect->localRotation.z ||
            beforePlayEffect->scale.x != afterPlayEffect->scale.x ||
            beforePlayEffect->scale.y != afterPlayEffect->scale.y ||
            beforePlayEffect->scale.z != afterPlayEffect->scale.z ||
            beforePlayEffect->playbackSpeed != afterPlayEffect->playbackSpeed;
    }

    if (const auto* beforePlaySound = get_if<CinematicPlaySoundEventData>(&before.payload))
    {
        const auto* afterPlaySound = get_if<CinematicPlaySoundEventData>(&after.payload);
        if (afterPlaySound == nullptr)
            return true;

        return beforePlaySound->soundCue != afterPlaySound->soundCue ||
            beforePlaySound->useTargetPosition != afterPlaySound->useTargetPosition ||
            beforePlaySound->volume != afterPlaySound->volume ||
            beforePlaySound->speed != afterPlaySound->speed ||
            beforePlaySound->startRatio != afterPlaySound->startRatio;
    }

    if (const auto* beforePartVisibility = get_if<CinematicPartVisibilityEventData>(&before.payload))
    {
        const auto* afterPartVisibility = get_if<CinematicPartVisibilityEventData>(&after.payload);
        if (afterPartVisibility == nullptr)
            return true;

        return beforePartVisibility->partTag != afterPartVisibility->partTag ||
            beforePartVisibility->visible != afterPartVisibility->visible;
    }

    if (const auto* beforeWorldTimeScale = get_if<CinematicWorldTimeScaleEventData>(&before.payload))
    {
        const auto* afterWorldTimeScale = get_if<CinematicWorldTimeScaleEventData>(&after.payload);
        if (afterWorldTimeScale == nullptr)
            return true;

        return beforeWorldTimeScale->worldTimeScale != afterWorldTimeScale->worldTimeScale;
    }

    return get_if<CinematicDespawnEventData>(&before.payload) != nullptr;
}

int Get_BattleClipFrameMax(const BattleCameraClip& clip)
{
    return std::max(0, static_cast<int>(std::lround(clip.duration * kBattleClipPreviewFps)));
}

void Draw_SectionHeader(const char* title)
{
    ImGui::Spacing();
    ImGui::TextDisabled("%s", title);
    ImGui::Separator();
}

    template <typename TEnum>
void Select_PresetEnum(const char* label, TEnum& currentValue, const float width = 180.f)
{

    ImGui::SetNextItemWidth(width);

    const char* currentLabel = magic_enum::enum_name(currentValue).data();
    if (!ImGui::BeginCombo(label, currentLabel))
        return;

    for (TEnum value : magic_enum::enum_values<TEnum>())
    {
        const bool isSelected = (currentValue == value);
        if (ImGui::Selectable(magic_enum::enum_name(value).data(), isSelected))
            currentValue = value;

        if (isSelected)
            ImGui::SetItemDefaultFocus();
    }

    ImGui::EndCombo();
}

const char* Get_CinematicEventTypeLabel(const CINEMATIC_EVENT& event)
{
    if (get_if<CinematicSpawnEventData>(&event.payload) != nullptr)
        return "Spawn";

    if (get_if<CinematicPlayAnimEventData>(&event.payload) != nullptr)
        return "PlayAnim";

    if (get_if<CinematicPlayEffectEventData>(&event.payload) != nullptr)
        return "PlayEffect";

    if (get_if<CinematicPlaySoundEventData>(&event.payload) != nullptr)
        return "PlaySound";

    if (get_if<CinematicDespawnEventData>(&event.payload) != nullptr)
        return "Despawn";

    if (get_if<CinematicPartVisibilityEventData>(&event.payload) != nullptr)
        return "PartVisibility";

    if (get_if<CinematicWorldTimeScaleEventData>(&event.payload) != nullptr)
        return "WorldTimeScale";

    return "Unknown";
}

bool Evaluate_ActorTransformTrackPose(
    const CINEMATIC_ACTOR_TRANSFORM_TRACK& track,
    int currentFrame,
    Vec3& outPosition,
    Vec3& outRotation)
{
    if (!track.enabled || track.keyframes.empty())
        return false;

    const auto& keyframes = track.keyframes;
    if (currentFrame <= keyframes.front().frame)
    {
        outPosition = keyframes.front().position;
        outRotation = keyframes.front().rotation;
        return true;
    }

    if (currentFrame >= keyframes.back().frame)
    {
        outPosition = keyframes.back().position;
        outRotation = keyframes.back().rotation;
        return true;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& left = keyframes[i];
        const auto& right = keyframes[i + 1];
        if (currentFrame < left.frame || currentFrame > right.frame)
            continue;

        if (left.frame == right.frame)
        {
            outPosition = right.position;
            outRotation = right.rotation;
            return true;
        }

        const float t = static_cast<float>(currentFrame - left.frame) /
            static_cast<float>(right.frame - left.frame);
        outPosition = Vec3::Lerp(left.position, right.position, t);
        outRotation = Vec3::Lerp(left.rotation, right.rotation, t);
        return true;
    }

    return false;
}

bool Evaluate_PartRotationTrackPose(
    const CINEMATIC_PART_ROTATION_TRACK& track,
    int currentFrame,
    Vec3& outRotation)
{
    if (!track.enabled || track.keyframes.empty())
        return false;

    const auto& keyframes = track.keyframes;
    if (currentFrame <= keyframes.front().frame)
    {
        outRotation = keyframes.front().rotation;
        return true;
    }

    if (currentFrame >= keyframes.back().frame)
    {
        outRotation = keyframes.back().rotation;
        return true;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& left = keyframes[i];
        const auto& right = keyframes[i + 1];
        if (currentFrame < left.frame || currentFrame > right.frame)
            continue;

        if (left.frame == right.frame)
        {
            outRotation = right.rotation;
            return true;
        }

        const float t = static_cast<float>(currentFrame - left.frame) /
            static_cast<float>(right.frame - left.frame);
        outRotation = LerpCinematicRotationDegreesShortest(left.rotation, right.rotation, t);
        return true;
    }

    return false;
}

bool Evaluate_ActorRelativeCameraTrackPose(
    const CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK& track,
    int currentFrame,
    CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& outKeyframe)
{
    if (!track.enabled || track.keyframes.empty())
        return false;

    const auto& keyframes = track.keyframes;
    if (currentFrame <= keyframes.front().frame)
    {
        outKeyframe = keyframes.front();
        return true;
    }

    if (currentFrame >= keyframes.back().frame)
    {
        outKeyframe = keyframes.back();
        return true;
    }

    for (size_t i = 0; i + 1 < keyframes.size(); ++i)
    {
        const auto& left = keyframes[i];
        const auto& right = keyframes[i + 1];
        if (currentFrame < left.frame || currentFrame > right.frame)
            continue;

        if (left.frame == right.frame)
        {
            outKeyframe = right;
            return true;
        }

        const float t = static_cast<float>(currentFrame - left.frame) /
            static_cast<float>(right.frame - left.frame);
        outKeyframe.frame = currentFrame;
        outKeyframe.distance = std::lerp(left.distance, right.distance, t);
        outKeyframe.height = std::lerp(left.height, right.height, t);
        outKeyframe.sideOffset = std::lerp(left.sideOffset, right.sideOffset, t);
        outKeyframe.yawOffset = std::lerp(left.yawOffset, right.yawOffset, t);
        outKeyframe.pitch = std::lerp(left.pitch, right.pitch, t);
        outKeyframe.lookHeight = std::lerp(left.lookHeight, right.lookHeight, t);
        outKeyframe.fovY = std::lerp(left.fovY, right.fovY, t);
        outKeyframe.nearPlane = std::lerp(left.nearPlane, right.nearPlane, t);
        outKeyframe.farPlane = std::lerp(left.farPlane, right.farPlane, t);
        outKeyframe.interpolation = right.interpolation;
        return true;
    }

    return false;
}

Vec3 Build_LookAtRotation(const Vec3& fromWorldPos, const Vec3& toWorldPos, const Vec3& fallbackRotation)
{
    const Vec3 direction = toWorldPos - fromWorldPos;
    const float horizontalLength = sqrtf(direction.x * direction.x + direction.z * direction.z);
    if (direction.LengthSquared() <= 0.0001f || horizontalLength <= 0.0001f)
        return fallbackRotation;

    Vec3 rotation = fallbackRotation;
    rotation.y = XMConvertToDegrees(atan2f(direction.x, direction.z));
    rotation.x = -XMConvertToDegrees(atan2f(direction.y, horizontalLength));
    rotation.z = 0.f;
    return rotation;
}

bool Build_ActorRelativeCameraPose(
    const CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK& track,
    int currentFrame,
    const Shared<GameObject>& actor,
    CAMERAPOSE& outPose)
{
    if (actor == nullptr)
        return false;

    Shared<TransformCom> transform = actor->Get_Transform();
    if (transform == nullptr)
        return false;

    CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME sample{};
    if (!Evaluate_ActorRelativeCameraTrackPose(track, currentFrame, sample))
        return false;

    const Vec3 actorPos = transform->Get_WorldPosition();
    const float actorYaw = transform->Get_WorldRotationEuler().y;
    const float finalYaw = actorYaw + sample.yawOffset;
    const float yawRad = XMConvertToRadians(finalYaw);
    const float pitchRad = XMConvertToRadians(sample.pitch);

    Vec3 backDir{};
    backDir.x = -sinf(yawRad);
    backDir.y = 0.f;
    backDir.z = -cosf(yawRad);
    backDir.Normalize();

    Vec3 rightDir{};
    rightDir.x = cosf(yawRad);
    rightDir.y = 0.f;
    rightDir.z = -sinf(yawRad);
    rightDir.Normalize();

    const Vec3 baseOffset = backDir * sample.distance;
    const Matrix pitchRotation = XMMatrixRotationAxis(rightDir, pitchRad);
    const Vec3 orbitOffset = XMVector3TransformNormal(baseOffset, pitchRotation);

    const Vec3 cameraPos = actorPos + orbitOffset + Vec3::Up * sample.height + rightDir * sample.sideOffset;
    const Vec3 lookTarget = actorPos + Vec3::Up * sample.lookHeight;

    outPose.position = cameraPos;
    outPose.rotation = Build_LookAtRotation(cameraPos, lookTarget, outPose.rotation);
    outPose.fovY = sample.fovY;
    outPose.nearPlane = sample.nearPlane;
    outPose.farPlane = sample.farPlane;
    return true;
}

int Get_ActorRelativeTrackStartIndex(const CINEMATIC_SEQUENCE& sequence)
{
    return static_cast<int>(sequence.cinematicTracks.size());
}

int Get_CinematicCameraTrackCount(const CINEMATIC_SEQUENCE& sequence)
{
    return static_cast<int>(sequence.cinematicTracks.size() + sequence.actorRelativeCameraTracks.size());
}

int Get_ModifierTrackStartIndex(const CINEMATIC_SEQUENCE& sequence)
{
    return Get_ActorRelativeTrackStartIndex(sequence) + static_cast<int>(sequence.actorRelativeCameraTracks.size());
}

int Get_EventTrackIndex(const CINEMATIC_SEQUENCE& sequence)
{
    return Get_ModifierTrackStartIndex(sequence) + CinematicSequencerAdapter::kModifierTrackCount;
}

int Get_ActorTransformTrackStartIndex(const CINEMATIC_SEQUENCE& sequence)
{
    return Get_EventTrackIndex(sequence) + CinematicSequencerAdapter::kEventTrackCount;
}

int Get_PartRotationTrackStartIndex(const CINEMATIC_SEQUENCE& sequence)
{
    return Get_ActorTransformTrackStartIndex(sequence) + static_cast<int>(sequence.actorTransformTracks.size());
}

string Build_CameraTrackLabel(const CINEMATIC_SEQUENCE& sequence, int trackIndex)
{
    if (trackIndex >= 0 && trackIndex < static_cast<int>(sequence.cinematicTracks.size()))
    {
        const auto& track = sequence.cinematicTracks[trackIndex];
        return track.name.empty() ? ("World Track " + std::to_string(trackIndex)) : track.name;
    }

    const int actorRelativeTrackIndex = trackIndex - static_cast<int>(sequence.cinematicTracks.size());
    if (actorRelativeTrackIndex >= 0 &&
        actorRelativeTrackIndex < static_cast<int>(sequence.actorRelativeCameraTracks.size()))
    {
        const auto& track = sequence.actorRelativeCameraTracks[actorRelativeTrackIndex];
        return track.name.empty() ? ("Actor Relative Track " + std::to_string(actorRelativeTrackIndex)) : track.name;
    }

    return "Invalid Track";
}

bool Resolve_CameraShotTrack(
    const CINEMATIC_SEQUENCE& sequence,
    float frameExact,
    CINEMATIC_TRACK*& outTrack,
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK*& outActorRelativeTrack)
{
    outTrack = nullptr;
    outActorRelativeTrack = nullptr;

    if (sequence.cameraShots.empty())
        return false;

    const int frame = std::clamp(
        static_cast<int>(std::floor(frameExact)),
        sequence.startFrame,
        sequence.endFrame);

    int resolvedTrackIndex = -1;
    for (const auto& shot : sequence.cameraShots)
    {
        if (!shot.enabled)
            continue;

        if (frame < shot.startFrame || frame > shot.endFrame)
            continue;

        resolvedTrackIndex = shot.trackIndex;
        break;
    }

    if (resolvedTrackIndex < 0)
        return false;

    if (resolvedTrackIndex < static_cast<int>(sequence.cinematicTracks.size()))
    {
        outTrack = const_cast<CINEMATIC_TRACK*>(&sequence.cinematicTracks[resolvedTrackIndex]);
        return true;
    }

    const int actorRelativeTrackIndex = resolvedTrackIndex - static_cast<int>(sequence.cinematicTracks.size());
    if (actorRelativeTrackIndex >= 0 &&
        actorRelativeTrackIndex < static_cast<int>(sequence.actorRelativeCameraTracks.size()))
    {
        outActorRelativeTrack = const_cast<CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK*>(
            &sequence.actorRelativeCameraTracks[actorRelativeTrackIndex]);
        return true;
    }

    return false;
}

}

Cinematic_Tool::Cinematic_Tool()
    : Editor_Window{ L"Cinematic Tool" }
{
    Ensure_DefaultTrack();
}

HRESULT Cinematic_Tool::Initialize()
{
    return S_OK;
}

void Cinematic_Tool::Update(float timeDelta)
{
    UNREFERENCED_PARAMETER(timeDelta);

    bool wasPlaying = _wasCinematicPlaying;
    bool isPlaying = CLIENT->Is_CinematicPlaying();

    if (isPlaying && !CLIENT->Is_CinematicPaused())
    {
        const int playbackFrame = CLIENT->Get_CurrentSequenceFrame();
        const float playbackFrameExact = CLIENT->Get_CurrentSequenceFrameExact();
        if (_currentFrame != playbackFrame ||
            std::fabs(_currentFrameExact - playbackFrameExact) > 0.0001f)
        {
            _currentFrame = playbackFrame;
            _currentFrameExact = playbackFrameExact;
            _isPreviewDirty = true;
        }
    }

    if (_isBattleClipPreviewDirty)
    {
        Apply_CurrentBattleClipPreview();
        _isBattleClipPreviewDirty = false;

        return;
    }

    if(_isPreviewDirty && !_hasPendingInsertRequest)
    {
        Apply_CurrentFramePose();
        _isPreviewDirty = false;
    }

    _wasCinematicPlaying = isPlaying;

    if (wasPlaying && !isPlaying)
    {
        _isPreviewStoppedForEditing = false;
        EDITOR->Set_CinematicPreviewFreeCameraOverride(false);
        GAME->Set_PreviewLetterboxEnabled(false);
        _isPreviewDirty = true;
    }


}

void Cinematic_Tool::Render()
{
    if (!Is_Open())
        return;

    bool isOpen = Is_Open();
    const string windowName = String::ToString(Get_WindowName());

    if (ImGui::Begin(windowName.c_str(), &isOpen))
    {
        _isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
        _isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows);
        _isCinematicTabActive = false;

        if (ImGui::BeginTabBar("##CinematicToolTabs"))
        {
            if (ImGui::BeginTabItem("Cinematic"))
            {
                _isCinematicTabActive = true;
                Render_CinematicTab();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Battle Clip"))
            {
                Render_BattleClipTab();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        ImGuiIO& io = ImGui::GetIO();
        if (_isFocused &&
            _isCinematicTabActive &&
            !io.WantTextInput &&
            !ImGui::IsAnyItemActive())
        {
            if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_S, false))
                _requestOpenSaveSequenceConfirmPopup = true;
            else if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Space, false))
                Toggle_PreviewPlayback();
            else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
                Step_SelectedTrackKeyframe(-1);
            else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
                Step_SelectedTrackKeyframe(1);
            else if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
                Step_PreviewFrame(-5);
            else if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
                Step_PreviewFrame(5);
            else if (!io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false))
                Step_PreviewFrame(-1);
            else if (!io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_RightArrow, false))
                Step_PreviewFrame(1);
        }
    }

    Set_Open(isOpen);
    ImGui::End();
}

void Cinematic_Tool::Toggle_PreviewPlayback()
{
    const bool isPlaying = CLIENT->Is_CinematicPlaying();
    const bool isPaused = CLIENT->Is_CinematicPaused();
    if (!isPlaying)
    {
        if (_sequence.cinematicTracks.empty() && _sequence.actorRelativeCameraTracks.empty())
            return;

        int previewTrackIndex = 0;
        if (_selectedTrackIndex >= 0 &&
            _selectedTrackIndex < Get_ModifierTrackStartIndex(_sequence))
        {
            previewTrackIndex = _selectedTrackIndex;
        }

        Sort_AllActorTransformTrackKeyframes();
        _isPreviewStoppedForEditing = false;
        EDITOR->Set_CinematicPreviewFreeCameraOverride(false);
        GAME->Set_PreviewLetterboxEnabled(true);
        CLIENT->Play_Sequence(
            _sequence,
            previewTrackIndex,
            _previewApplyCameraTrack,
            _previewApplyEventTrack,
            _previewApplyActorTransformTrack);

        const float previewStartFrameExact = _useExactFramePreview
            ? _currentFrameExact
            : static_cast<float>(_currentFrame);
        if (previewStartFrameExact > static_cast<float>(_sequence.startFrame))
        {
            CLIENT->Preview_CurrentSequenceFrameExact(
                previewStartFrameExact,
                _previewApplyCameraTrack,
                _previewApplyEventTrack,
                _previewApplyActorTransformTrack,
                false);
        }

        if (!_previewApplyCameraTrack)
        {
            EDITOR->Set_CinematicPreviewFreeCameraOverride(true);
            EDITOR->Activate_FreeCamera();
        }
    }
    else if (isPaused)
    {
        if (!Sync_PreviewSequenceToRunner())
            return;

        if (_previewApplyCameraTrack &&
            !GAME->Is_ActiveCameraType(ObjectType::CinematicCamera))
        {
            CLIENT->Request_Cinematic();
        }

        CLIENT->Preview_CurrentSequenceFrame(
            _currentFrame,
            _previewApplyCameraTrack,
            _previewApplyEventTrack,
            _previewApplyActorTransformTrack,
            false);

        CLIENT->Resume_Sequence(
            _previewApplyCameraTrack,
            _previewApplyEventTrack,
            _previewApplyActorTransformTrack);
        _isPreviewStoppedForEditing = false;
        EDITOR->Set_CinematicPreviewFreeCameraOverride(!_previewApplyCameraTrack);
    }
    else
    {
        CLIENT->Pause_Sequence();
        Enter_FreeCameraEditPreview();
    }

    _isPreviewDirty = true;
}

void Cinematic_Tool::Render_CinematicTab()
{
    Clear_BattleClipPreviewState();
    Ensure_DefaultTrack();
    Clamp_Selection();

    if (ImGui::BeginTable("##CinematicToolLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch, 1.0f);

        ImGui::TableNextColumn();
        ImGui::BeginChild("##SequencerPanel", ImVec2(0.f, 0.f), true);
        Draw_SectionHeader("Timeline");
        ImGui::TextDisabled("Left click : select / drag keyframe");
        ImGui::TextDisabled("Double click : expand selected key editor");
        ImGui::TextDisabled("Right click row : insert keyframe at clicked frame");
        ImGui::Spacing();
        Render_ImSequencer();
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("##CinematicRightPanel", ImVec2(0.f, 0.f), false);
        Render_RightPanel();
        ImGui::EndChild();

        ImGui::EndTable();
    }

    Render_InsertKeyframePopup();
}

void Cinematic_Tool::Render_BattleClipTab()
{
    const string selectedSkillId = Get_SelectedBattleClipSkillId();

    if (!_hasLoadedBattleClip ||
        _loadedBattleClipMode != _selectedBattleClipMode ||
        _loadedBattleClipSkillId != selectedSkillId)
    {
        BattleCameraClipIO::LoadBattleCameraClipFromFile(_selectedBattleClipMode, _editingBattleClip, selectedSkillId);
        _loadedBattleClipMode = _selectedBattleClipMode;
        _loadedBattleClipSkillId = selectedSkillId;
        _hasLoadedBattleClip = true;
        _isBattleClipPreviewDirty = true;
    }

    Render_BattleClipHeader();
    ImGui::Separator();

    if (ImGui::BeginTable("##BattleClipLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Timeline", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Controls", ImGuiTableColumnFlags_WidthStretch, 1.1f);

        ImGui::TableNextColumn();
        ImGui::BeginChild("##BattleClipSequencerPanel", ImVec2(0.f, 0.f), true);
        Draw_SectionHeader("Battle Clip Timeline");
        ImGui::TextDisabled("Left click : select / drag keyframe");
        ImGui::TextDisabled("Right click row : insert keyframe at clicked time");
        ImGui::TextDisabled("Right click key : remove selected keyframe");
        ImGui::Spacing();
        Render_BattleClipSequencer();
        ImGui::EndChild();

        ImGui::TableNextColumn();
        ImGui::BeginChild("##BattleClipRightPanel", ImVec2(0.f, 0.f), false);
        ImGui::SetNextItemWidth(160.f);
        if (ImGui::DragFloat("Duration", &_editingBattleClip.duration, 0.01f, 0.f, 30.f))
        {
            _editingBattleClip.duration = std::max(0.f, _editingBattleClip.duration);
            _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipFrameMax(_editingBattleClip));
            _isBattleClipPreviewDirty = true;
        }
        else
        {
            _editingBattleClip.duration = std::max(0.f, _editingBattleClip.duration);
        }

        ImGui::TextDisabled("Current Frame : %d", _battleClipCurrentFrame);
        ImGui::TextDisabled("Current Time : %.3f s", static_cast<float>(_battleClipCurrentFrame) / kBattleClipPreviewFps);

        Render_SelectedBattleClipKeyframe();
        Render_BattleClipTrackEditor("Distance Offset", _editingBattleClip.distanceOffset);
        Render_BattleClipTrackEditor("FOV Offset", _editingBattleClip.fovOffset);
        Render_BattleClipTrackEditor("Shake Amplitude", _editingBattleClip.shakeAmplitude);
        Render_BattleClipTrackEditor("Side Offset", _editingBattleClip.sideOffset);
        Render_BattleClipTrackEditor("Yaw Offset", _editingBattleClip.yawOffset);
        ImGui::EndChild();

        ImGui::EndTable();
    }

    Render_BattleClipInsertKeyframePopup();
    Render_BattleClipRemoveKeyframePopup();
}

void Cinematic_Tool::Render_BattleClipHeader()
{
    Ensure_BattleClipSkillListLoaded();

    const BattleCameraMode previousMode = _selectedBattleClipMode;
    Select_PresetEnum("Battle Mode", _selectedBattleClipMode, 100.f);

    if (previousMode != _selectedBattleClipMode)
    {
        _hasLoadedBattleClip = false;
        _battleClipSelectedTrackIndex = 0;
        _battleClipSelectedKeyIndex = -1;
        _battleClipCurrentFrame = 0;
        _isBattleClipPreviewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("Use Skill Override", &_useBattleClipSkillOverride))
    {
        _hasLoadedBattleClip = false;
        _battleClipCurrentFrame = 0;
        _isBattleClipPreviewDirty = true;
    }

    if (_useBattleClipSkillOverride)
    {
        const char* currentSkillLabel = "None";

        if (_selectedBattleClipSkillIndex >= 0 &&
            _selectedBattleClipSkillIndex < static_cast<int>(_battleClipSkillIds.size()))
        {
            currentSkillLabel = _battleClipSkillIds[_selectedBattleClipSkillIndex].c_str();
        }

        ImGui::SameLine();
        ImGui::SetNextItemWidth(280.f);

        if (ImGui::BeginCombo("Skill", currentSkillLabel))
        {
            for (int i = 0; i < static_cast<int>(_battleClipSkillIds.size()); ++i)
            {
                const bool isSelected = (_selectedBattleClipSkillIndex == i);

                if (ImGui::Selectable(_battleClipSkillIds[i].c_str(), isSelected))
                {
                    _selectedBattleClipSkillIndex = i;
                    _hasLoadedBattleClip = false;
                    _battleClipCurrentFrame = 0;
                    _isBattleClipPreviewDirty = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }
    }

    const string selectedSkillId = Get_SelectedBattleClipSkillId();

    ImGui::SameLine();
    if (ImGui::Button("Load Clip"))
    {
        BattleCameraClipIO::LoadBattleCameraClipFromFile(_selectedBattleClipMode, _editingBattleClip, selectedSkillId);
        _loadedBattleClipMode = _selectedBattleClipMode;
        _loadedBattleClipSkillId = selectedSkillId;
        _hasLoadedBattleClip = true;
        _battleClipSelectedTrackIndex = 0;
        _battleClipSelectedKeyIndex = -1;
        _battleClipCurrentFrame = 0;
        _isBattleClipPreviewDirty = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Save Clip"))
        BattleCameraClipIO::SaveBattleCameraClipToFile(_selectedBattleClipMode, _editingBattleClip, selectedSkillId);

    ImGui::SameLine();
    if (ImGui::Button("Preview Clip"))
        _isBattleClipPreviewDirty = true;

    ImGui::SameLine();
    if (ImGui::Button("Go To Start"))
    {
        _battleClipCurrentFrame = 0;
        _isBattleClipPreviewDirty = true;
    }
}

void Cinematic_Tool::Ensure_BattleClipSkillListLoaded()
{
    if (!_battleClipSkillIds.empty())
        return;

    _battleClipSkillIds = CLIENT->Get_AllSkillIds();

    if (!_battleClipSkillIds.empty() && _selectedBattleClipSkillIndex < 0)
        _selectedBattleClipSkillIndex = 0;
}

string Cinematic_Tool::Get_SelectedBattleClipSkillId() const
{
    if (!_useBattleClipSkillOverride)
        return "";

    if (_selectedBattleClipSkillIndex < 0 ||
        _selectedBattleClipSkillIndex >= static_cast<int>(_battleClipSkillIds.size()))
    {
        return "";
    }

    return _battleClipSkillIds[_selectedBattleClipSkillIndex];
}

Shared<Character> Cinematic_Tool::Resolve_BattleClipPreviewSubject() const
{
    auto battleController = CLIENT->Get_BattleController();
    if (!battleController)
        return nullptr;

    if (_selectedBattleClipMode == BattleCameraMode::ENEMYFOCUS ||
        _selectedBattleClipMode == BattleCameraMode::ENEMYATTACK)
    {
        if (const auto enemies = battleController->Get_AliveEnemies(); !enemies.empty() && enemies[0].actor != nullptr)
            return enemies[0].actor;
    }

    if (_selectedBattleClipMode == BattleCameraMode::PLAYERFOCUS ||
        _selectedBattleClipMode == BattleCameraMode::PLAYERATTACK)
    {
        //원래는 battleController->Get_TurnAction().target.actor
        if(const auto hero = battleController->Get_TurnAction().GetInitTarget().actor)   
            return hero;
        //if (const auto heroes = battleController->Get_AliveHeroes(); !heroes.empty() && heroes[0].actor != nullptr)
        //    return heroes[0].actor;
    }

    if (const auto heroes = battleController->Get_AliveHeroes(); !heroes.empty() && heroes[0].actor != nullptr)
        return heroes[0].actor;

    if (const auto enemies = battleController->Get_AliveEnemies(); !enemies.empty() && enemies[0].actor != nullptr)
        return enemies[0].actor;

    return nullptr;
}

Shared<Character> Cinematic_Tool::Resolve_BattleClipPreviewOpponent() const
{
    auto battleController = CLIENT->Get_BattleController();
    if (!battleController)
        return nullptr;

    if (_selectedBattleClipMode == BattleCameraMode::ENEMYATTACK)
    {
        if (auto target = battleController->Get_TurnAction().GetInitTarget().actor)
            return target;

        if (const auto heroes = battleController->Get_AliveHeroes(); !heroes.empty() && heroes[0].actor != nullptr)
            return heroes[0].actor;
    }

    if (_selectedBattleClipMode == BattleCameraMode::PLAYERATTACK)
    {
        if (const BattleChar* preview = battleController->Get_TargetPreview())
        {
            if (preview->actor != nullptr)
                return preview->actor;
        }

        if (const auto enemies = battleController->Get_AliveEnemies(); !enemies.empty() && enemies[0].actor != nullptr)
            return enemies[0].actor;
    }

    if (const auto enemies = battleController->Get_AliveEnemies(); !enemies.empty() && enemies[0].actor != nullptr)
        return enemies[0].actor;

    if (const auto heroes = battleController->Get_AliveHeroes(); !heroes.empty() && heroes[0].actor != nullptr)
        return heroes[0].actor;

    return nullptr;
}

Shared<ModelCom> Cinematic_Tool::Resolve_BattleClipPreviewModel() const
{
    if (Shared<Character> subject = Resolve_BattleClipPreviewSubject())
    {
        PartObject* body = subject->Get_PartObject(L"Part_Body");
        if (body != nullptr)
        {
            if (Shared<ModelCom> model = body->Get_Component<ModelCom>())
                return model;
        }
    }

    auto battleController = CLIENT->Get_BattleController();
    if (!battleController)
        return nullptr;

    const Client::BattleAction& action = battleController->Get_TurnAction();
    if (action.Get_CurChar().ModelCom != nullptr)
        return action.Get_CurChar().ModelCom;

    return nullptr;
}

void Cinematic_Tool::Apply_BattleClipPreviewAnimation(float previewTimeSec) const
{
    Shared<ModelCom> model = Resolve_BattleClipPreviewModel();
    if (model == nullptr)
        return;

    auto battleController = CLIENT->Get_BattleController();
    if (battleController == nullptr)
        return;

    const Client::BattleAction& action = battleController->Get_TurnAction();
    const Client::Skill_Data& skillData = action.GetSkillData();
    if ((_selectedBattleClipMode == BattleCameraMode::PLAYERATTACK ||
         _selectedBattleClipMode == BattleCameraMode::ENEMYATTACK) &&
        !skillData.AnimPhases.empty())
    {
        const uint32 phaseIndex = std::min<uint32>(action.GetCurAnimIndex(), static_cast<uint32>(skillData.AnimPhases.size() - 1));
        const string& animationKey = skillData.AnimPhases[phaseIndex].animationKey;
        const char* currentAnimationName = model->Get_AnimationName(model->Get_CurrentAnimationIndex());

        if (!animationKey.empty() &&
            (currentAnimationName == nullptr || animationKey != currentAnimationName))
        {
            model->Set_Animation(animationKey, false, 0.f);
        }
    }

    model->Set_AnimationPaused(true);
    model->Set_CurrentAnimationTimeSec(previewTimeSec);
    model->Refresh_CurrentAnimationPose();
}

void Cinematic_Tool::Clip_BattleClipHotInput()
{
    CameraKeyframe* key = Get_SelectedBattleClipKeyframe();

    if(key == nullptr)
        return;

    if(!_isFocused||!_isHovered)
        return;

}

void Cinematic_Tool::Clear_BattleClipPreviewState() const
{
    if (Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera())
        battleTurnCamera->Set_ClipScrubPreviewEnabled(false);

    if (Shared<ModelCom> model = Resolve_BattleClipPreviewModel())
        model->Set_AnimationPaused(false);
}

void Cinematic_Tool::Render_BattleClipTrackEditor(const char* label, CameraTrack& track)
{
    if (!ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::PushID(label);
    bool needsSort = false;
    float selectionTime = -1.f;

    const string addKeyId = string("Add Key##") + label;
    if (ImGui::Button(addKeyId.c_str()))
    {
        CameraKeyframe keyframe {};
        if (!track.keyframes.empty())
            keyframe.time = track.keyframes.back().time;

        track.keyframes.push_back(keyframe);
        Sort_BattleClipTrackKeyframes(track);
        _isBattleClipPreviewDirty = true;
    }

    ImGui::Separator();

    for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
    {
        CameraKeyframe& keyframe = track.keyframes[i];

        ImGui::PushID(i);
        ImGui::TextDisabled("Key %d", i);
        ImGui::SameLine();

        if (ImGui::Button("Remove"))
        {
            track.keyframes.erase(track.keyframes.begin() + i);
            if (&track == Get_SelectedBattleClipTrack())
                _battleClipSelectedKeyIndex = std::clamp(_battleClipSelectedKeyIndex, -1, static_cast<int>(track.keyframes.size()) - 1);
            _isBattleClipPreviewDirty = true;
            ImGui::PopID();
            break;
        }

        ImGui::SetNextItemWidth(140.f);
        if (ImGui::DragFloat("Time", &keyframe.time, 0.01f, 0.f, _editingBattleClip.duration))
        {
            keyframe.time = std::clamp(keyframe.time, 0.f, _editingBattleClip.duration);
            needsSort = true;
            _isBattleClipPreviewDirty = true;
            if (&track == Get_SelectedBattleClipTrack() && _battleClipSelectedKeyIndex == i)
                selectionTime = keyframe.time;
        }

        ImGui::SetNextItemWidth(140.f);
        if (ImGui::DragFloat("Value", &keyframe.value, 0.01f))
            _isBattleClipPreviewDirty = true;
        ImGui::Separator();
        ImGui::PopID();
    }

    if (needsSort)
    {
        Sort_BattleClipTrackKeyframes(track);

        if (&track == Get_SelectedBattleClipTrack() && selectionTime >= 0.f)
        {
            for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
            {
                if (fabsf(track.keyframes[i].time - selectionTime) <= 0.0001f)
                {
                    _battleClipSelectedKeyIndex = i;
                    break;
                }
            }
        }
    }

    ImGui::PopID();
}

void Cinematic_Tool::Render_BattleClipSequencer()
{
    bool consumedKeyClick = false;
    bool requestOpenInsertPopup = false;
    bool requestRemoveKeyPopup = false;
    int sequencerSelectedTrackIndex = _battleClipSelectedTrackIndex;
    const int previousFrame = _battleClipCurrentFrame;
    const int previousTrackIndex = _battleClipSelectedTrackIndex;
    const int previousKeyIndex = _battleClipSelectedKeyIndex;

    BattleClipSequencerAdapter adapter(
        &_editingBattleClip,
        &_battleClipSelectedTrackIndex,
        &_battleClipSelectedKeyIndex,
        &_battleClipCurrentFrame,
        &consumedKeyClick,
        &requestOpenInsertPopup,
        &requestRemoveKeyPopup,
        &_battleClipContextTrackIndex,
        &_battleClipContextFrame,
        nullptr,
        false,
        nullptr);

    static bool expanded = true;
    static int firstFrame = 0;
    ImSequencer::Sequencer(&adapter, &_battleClipCurrentFrame, &expanded, &sequencerSelectedTrackIndex, &firstFrame, ImSequencer::SEQUENCER_CHANGE_FRAME);

    if (!consumedKeyClick)
        _battleClipSelectedTrackIndex = sequencerSelectedTrackIndex;

    if (requestOpenInsertPopup)
        _requestOpenBattleClipInsertPopup = true;

    if (requestRemoveKeyPopup)
        _requestRemoveBattleClipKeyPopup = true;

    if (previousFrame != _battleClipCurrentFrame ||
        previousTrackIndex != _battleClipSelectedTrackIndex ||
        previousKeyIndex != _battleClipSelectedKeyIndex)
    {
        _isBattleClipPreviewDirty = true;
    }
}

void Cinematic_Tool::Render_SelectedBattleClipKeyframe()
{
    CameraKeyframe* keyframe = Get_SelectedBattleClipKeyframe();
    if (keyframe == nullptr)
        return;

    Draw_SectionHeader("Selected Battle Key");
    ImGui::TextDisabled("Track %d | Key %d", _battleClipSelectedTrackIndex, _battleClipSelectedKeyIndex);

    ImGui::SetNextItemWidth(140.f);
    if (ImGui::DragFloat("Selected Time", &keyframe->time, 0.01f, 0.f, _editingBattleClip.duration))
    {
        keyframe->time = std::clamp(keyframe->time, 0.f, _editingBattleClip.duration);
        if (CameraTrack* track = Get_SelectedBattleClipTrack())
        {
            const float selectionTime = keyframe->time;
            Sort_BattleClipTrackKeyframes(*track);
            for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
            {
                if (fabsf(track->keyframes[i].time - selectionTime) <= 0.0001f)
                {
                    _battleClipSelectedKeyIndex = i;
                    break;
                }
            }
        }

        _isBattleClipPreviewDirty = true;
    }

    ImGui::SetNextItemWidth(140.f);
    if (ImGui::DragFloat("Selected Value", &keyframe->value, 0.01f))
        _isBattleClipPreviewDirty = true;

    ImGui::SetNextItemWidth(160.f);
    const char* interpolationLabel = magic_enum::enum_name(keyframe->interpolation).data();
    if (ImGui::BeginCombo("Interpolation", interpolationLabel))
    {
        for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = keyframe->interpolation == mode;
            if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
                keyframe->interpolation = mode;
                _isBattleClipPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

void Cinematic_Tool::Render_BattleClipInsertKeyframePopup()
{
    if (_requestOpenBattleClipInsertPopup)
    {
        ImGui::OpenPopup("Insert Battle Clip Keyframe");
        _requestOpenBattleClipInsertPopup = false;
    }

    if (!ImGui::BeginPopup("Insert Battle Clip Keyframe"))
        return;

    ImGui::TextDisabled("Track %d", _battleClipContextTrackIndex);
    ImGui::TextDisabled("Frame %d", _battleClipContextFrame);
    ImGui::Separator();

    if (ImGui::Button("Insert Keyframe"))
    {
        Insert_BattleClipKeyAtFrame(_battleClipContextTrackIndex, _battleClipContextFrame);
        _isBattleClipPreviewDirty = true;
        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_BattleClipRemoveKeyframePopup()
{
    if (_requestRemoveBattleClipKeyPopup)
    {
        ImGui::OpenPopup("Remove Battle Clip Keyframe");
        _requestRemoveBattleClipKeyPopup = false;
    }

    if (!ImGui::BeginPopup("Remove Battle Clip Keyframe"))
        return;

    if (ImGui::Button("Remove Selected Key"))
    {
        CameraTrack* track = Get_SelectedBattleClipTrack();
        if (track != nullptr &&
            _battleClipSelectedKeyIndex >= 0 &&
            _battleClipSelectedKeyIndex < static_cast<int>(track->keyframes.size()))
        {
            track->keyframes.erase(track->keyframes.begin() + _battleClipSelectedKeyIndex);
            _battleClipSelectedKeyIndex = std::clamp(_battleClipSelectedKeyIndex, -1, static_cast<int>(track->keyframes.size()) - 1);
            _isBattleClipPreviewDirty = true;
        }

        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_RightPanel()
{
    Render_PreviewState();
    Render_SelectedKeyframePopup();

    if (ImGui::BeginTabBar("##CinematicRightPanelTabs"))
    {
        if (ImGui::BeginTabItem("Sequence"))
        {
            Render_SequenceSettings();
            Render_CameraShots();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Events"))
        {
            Render_EventTrack();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Tracks"))
        {
            Render_TrackList();
            Render_ActorRelativeCameraTracks();
            Render_ActorTransformTracks();
            Render_PartRotationTracks();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Selection"))
        {
            Render_KeyframeList();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    Render_LoadSequencePopup();
    Render_SequenceOverwriteWarningPopups();
    Render_RemoveKeyframePopup();

}

void Cinematic_Tool::Render_SequenceSettings()
{
    Draw_SectionHeader("Sequence");

    char nameBuffer[128]{};
    strncpy_s(nameBuffer, _sequence.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("name", nameBuffer, static_cast<size_t>(_countof(nameBuffer))))
        _sequence.name = nameBuffer;

    ImGui::SetNextItemWidth(100.f);
    if (ImGui::DragInt("fps", &_sequence.fps, 1.f, 1, 240))
    {
        _sequence.fps = std::clamp(_sequence.fps, 1, 240);
        _isPreviewDirty = true;
    }

    ImGui::SetNextItemWidth(100.f);
    if (ImGui::DragInt("start frame", &_sequence.startFrame, 1.f, 0, _sequence.endFrame))
    {
        _sequence.startFrame = std::clamp(_sequence.startFrame, 0, _sequence.endFrame);
        for (auto& shot : _sequence.cameraShots)
        {
            shot.startFrame = std::clamp(shot.startFrame, _sequence.startFrame, _sequence.endFrame);
            shot.endFrame = std::clamp(shot.endFrame, shot.startFrame, _sequence.endFrame);
        }
        _currentFrame = std::clamp(_currentFrame, _sequence.startFrame, _sequence.endFrame);
        _isPreviewDirty = true;
    }

    ImGui::SetNextItemWidth(100.f);
    if (ImGui::DragInt("end frame", &_sequence.endFrame, 1.f, _sequence.startFrame, 100000))
    {
        _sequence.endFrame = std::max(_sequence.endFrame, _sequence.startFrame);
        for (auto& shot : _sequence.cameraShots)
        {
            shot.startFrame = std::clamp(shot.startFrame, _sequence.startFrame, _sequence.endFrame);
            shot.endFrame = std::clamp(shot.endFrame, shot.startFrame, _sequence.endFrame);
        }
        _currentFrame = std::clamp(_currentFrame, _sequence.startFrame, _sequence.endFrame);
        _isPreviewDirty = true;
    }

    if(ImGui::Button("Save Sequence"))
    {
        _requestOpenSaveSequenceConfirmPopup = true;
    }

    ImGui::SameLine();
    if(ImGui::Button("Load Sequence"))
    {
        Refresh_SequenceFileList();
        _selectedSequenceFileIndex = -1;
        _requestOpenLoadSequencePopup = true;
    }

    if (ImGui::Button("Clear All Keys"))
    {
        _requestOpenClearAllKeysConfirmPopup = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("New Empty Sequence"))
    {
        _requestOpenNewEmptySequenceConfirmPopup = true;
    }
}

void Cinematic_Tool::Render_EventTrack()
{
    Draw_SectionHeader("Event Track");

    const bool canRemoveEvent =
        _selectedEventIndex >= 0 &&
        _selectedEventIndex < static_cast<int>(_sequence.eventTrack.size());

    auto addEvent = [&](CinematicEventPayload payload, bool refreshPreview)
    {
        CINEMATIC_EVENT event{};
        event.frame = _currentFrame;
        event.payload = payload;
        _sequence.eventTrack.push_back(event);
        _selectedEventIndex = static_cast<int>(_sequence.eventTrack.size()) - 1;
        std::sort(_sequence.eventTrack.begin(), _sequence.eventTrack.end(),
            [](const CINEMATIC_EVENT& lhs, const CINEMATIC_EVENT& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        if (refreshPreview)
            Request_CinematicPreviewRefresh(true);
    };

    ImGui::TextDisabled("Current frame %d", _currentFrame);
    ImGui::SameLine();
    ImGui::TextDisabled("Events %d", static_cast<int>(_sequence.eventTrack.size()));

    if (ImGui::BeginTable("##EventTrackButtons", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Button("Add Spawn", ImVec2(-FLT_MIN, 0.f)))
            addEvent(CinematicSpawnEventData{}, false);

        ImGui::TableNextColumn();
        if (ImGui::Button("Add PlayAnim", ImVec2(-FLT_MIN, 0.f)))
            addEvent(CinematicPlayAnimEventData{}, false);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Button("Add Despawn", ImVec2(-FLT_MIN, 0.f)))
            addEvent(CinematicDespawnEventData{}, true);

        ImGui::TableNextColumn();
        if (ImGui::Button("Add PartVisibility", ImVec2(-FLT_MIN, 0.f)))
        {
            CinematicPartVisibilityEventData payload{};
            payload.partTag = "Part_Sword_L";
            addEvent(payload, true);
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Button("Add WorldTimeScale", ImVec2(-FLT_MIN, 0.f)))
            addEvent(CinematicWorldTimeScaleEventData{}, true);

        ImGui::TableNextColumn();
        if (ImGui::Button("Add PlayEffect", ImVec2(-FLT_MIN, 0.f)))
            addEvent(CinematicPlayEffectEventData{}, true);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Button("Add PlaySound", ImVec2(-FLT_MIN, 0.f)))
            addEvent(CinematicPlaySoundEventData{}, true);

        ImGui::TableNextColumn();
        if (!canRemoveEvent)
            ImGui::BeginDisabled();
        if (ImGui::Button("Remove Selected", ImVec2(-FLT_MIN, 0.f)))
        {
            _sequence.eventTrack.erase(_sequence.eventTrack.begin() + _selectedEventIndex);
            _selectedEventIndex = std::clamp(
                _selectedEventIndex,
                -1,
                static_cast<int>(_sequence.eventTrack.size()) - 1);
            Request_CinematicPreviewRefresh(true);
        }
        if (!canRemoveEvent)
            ImGui::EndDisabled();

        ImGui::EndTable();
    }

    const bool canEditEvent =
        _selectedEventIndex >= 0 &&
        _selectedEventIndex < static_cast<int>(_sequence.eventTrack.size());

    if (!canEditEvent)
        ImGui::TextDisabled("Select an event to edit its details.");

    if (ImGui::BeginListBox("##EventTrackList", ImVec2(-FLT_MIN, 180.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequence.eventTrack.size()); ++i)
        {
            const CINEMATIC_EVENT& event = _sequence.eventTrack[i];
            const string label =
                std::to_string(event.frame) + " | " +
                Get_CinematicEventTypeLabel(event) + " | " +
                (event.targetId.empty() ? "<none>" : event.targetId);

            if (ImGui::Selectable(label.c_str(), _selectedEventIndex == i))
            {
                _selectedEventIndex = i;
                _currentFrame = event.frame;
                Request_CinematicPreviewRefresh(false);
            }
        }

        ImGui::EndListBox();
    }

    if (!canEditEvent)
        return;

    ImGui::Separator();
    ImGui::TextDisabled("Selected Event");

    CINEMATIC_EVENT& event = _sequence.eventTrack[_selectedEventIndex];
    const CINEMATIC_EVENT originalEvent = event;
    bool eventEdited = false;

    if (ImGui::DragInt("event frame", &event.frame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        event.frame = std::clamp(event.frame, _sequence.startFrame, _sequence.endFrame);
        _currentFrame = event.frame;
        eventEdited = true;
    }

    char targetBuffer[128]{};
    strncpy_s(targetBuffer, event.targetId.c_str(), _TRUNCATE);
    if (ImGui::InputText("target id", targetBuffer, static_cast<size_t>(_countof(targetBuffer))))
    {
        event.targetId = targetBuffer;
        eventEdited = true;
    }

    if (Draw_StringSuggestionCombo("target id presets", event.targetId, Collect_TargetIdSuggestions(_sequence)))
        eventEdited = true;

    const char* currentTypeLabel = Get_CinematicEventTypeLabel(event);
    if (ImGui::BeginCombo("event type", currentTypeLabel))
    {
        const bool isSpawn = get_if<CinematicSpawnEventData>(&event.payload) != nullptr;
        if (ImGui::Selectable("Spawn", isSpawn))
        {
            event.payload = CinematicSpawnEventData{};
            eventEdited = true;
        }
        if (isSpawn)
            ImGui::SetItemDefaultFocus();

        const bool isPlayAnim = get_if<CinematicPlayAnimEventData>(&event.payload) != nullptr;
        if (ImGui::Selectable("PlayAnim", isPlayAnim))
        {
            event.payload = CinematicPlayAnimEventData{};
            eventEdited = true;
        }
        if (isPlayAnim)
            ImGui::SetItemDefaultFocus();

        const bool isPlayEffect = get_if<CinematicPlayEffectEventData>(&event.payload) != nullptr;
        if (ImGui::Selectable("PlayEffect", isPlayEffect))
        {
            event.payload = CinematicPlayEffectEventData{};
            eventEdited = true;
        }
        if (isPlayEffect)
            ImGui::SetItemDefaultFocus();

        const bool isPlaySound = get_if<CinematicPlaySoundEventData>(&event.payload) != nullptr;
        if (ImGui::Selectable("PlaySound", isPlaySound))
        {
            event.payload = CinematicPlaySoundEventData{};
            eventEdited = true;
        }
        if (isPlaySound)
            ImGui::SetItemDefaultFocus();

        const bool isDespawn = get_if<CinematicDespawnEventData>(&event.payload) != nullptr;
        if (ImGui::Selectable("Despawn", isDespawn))
        {
            event.payload = CinematicDespawnEventData{};
            eventEdited = true;
        }
        if (isDespawn)
            ImGui::SetItemDefaultFocus();

        const bool isPartVisibility = get_if<CinematicPartVisibilityEventData>(&event.payload) != nullptr;
        if (ImGui::Selectable("PartVisibility", isPartVisibility))
        {
            CinematicPartVisibilityEventData payload{};
            payload.partTag = "Part_Sword_L";
            event.payload = payload;
            eventEdited = true;
        }
        if (isPartVisibility)
            ImGui::SetItemDefaultFocus();

        const bool isWorldTimeScale = get_if<CinematicWorldTimeScaleEventData>(&event.payload) != nullptr;
        if (ImGui::Selectable("WorldTimeScale", isWorldTimeScale))
        {
            event.payload = CinematicWorldTimeScaleEventData{};
            eventEdited = true;
        }
        if (isWorldTimeScale)
            ImGui::SetItemDefaultFocus();

        ImGui::EndCombo();
    }

    if (auto* playAnim = get_if<CinematicPlayAnimEventData>(&event.payload))
    {
        char animBuffer[128]{};
        strncpy_s(animBuffer, playAnim->animName.c_str(), _TRUNCATE);
        if (ImGui::InputText("anim name", animBuffer, static_cast<size_t>(_countof(animBuffer))))
        {
            playAnim->animName = animBuffer;
            eventEdited = true;
        }

        if (ImGui::DragFloat("blend duration", &playAnim->blendDuration, 0.01f, 0.f, 5.f, "%.2f"))
            eventEdited = true;
        if (ImGui::Checkbox("loop", &playAnim->loop))
            eventEdited = true;
        if (ImGui::DragFloat("anim start frame", &playAnim->startFrame, 1.f, 0.f, 100000.f, "%.0f"))
        {
            playAnim->startFrame = std::max(playAnim->startFrame, 0.f);
            eventEdited = true;
        }
        if (ImGui::DragFloat("anim speed", &playAnim->speed, 0.01f, 0.01f, 10.f, "%.2f"))
        {
            playAnim->speed = std::max(playAnim->speed, 0.01f);
            eventEdited = true;
        }
        if (ImGui::Checkbox("hold frame", &playAnim->holdFrame))
            eventEdited = true;
    }
    else if (auto* playEffect = get_if<CinematicPlayEffectEventData>(&event.payload))
    {
        string effectName = String::ToString(playEffect->effectName);
        char effectBuffer[128]{};
        strncpy_s(effectBuffer, effectName.c_str(), _TRUNCATE);
        if (ImGui::InputText("effect name", effectBuffer, static_cast<size_t>(_countof(effectBuffer))))
        {
            playEffect->effectName = String::ToWString(effectBuffer);
            eventEdited = true;
        }

        const auto beforeTarget = playEffect->target;
        Select_PresetEnum("effect target", playEffect->target);
        if (playEffect->target != beforeTarget)
            eventEdited = true;

        string partTag = String::ToString(playEffect->partTag);
        char partTagBuffer[128]{};
        strncpy_s(partTagBuffer, partTag.c_str(), _TRUNCATE);
        if (ImGui::InputText("part tag", partTagBuffer, static_cast<size_t>(_countof(partTagBuffer))))
        {
            playEffect->partTag = String::ToWString(partTagBuffer);
            eventEdited = true;
        }

        if (Draw_StringSuggestionCombo("part tag presets", partTag, Collect_PartTagSuggestions(_sequence)))
        {
            playEffect->partTag = String::ToWString(partTag);
            eventEdited = true;
        }

        char socketBuffer[128]{};
        strncpy_s(socketBuffer, playEffect->socketName.c_str(), _TRUNCATE);
        if (ImGui::InputText("socket name", socketBuffer, static_cast<size_t>(_countof(socketBuffer))))
        {
            playEffect->socketName = socketBuffer;
            eventEdited = true;
        }

        string layerTag = String::ToString(playEffect->layerTag);
        char layerBuffer[128]{};
        strncpy_s(layerBuffer, layerTag.c_str(), _TRUNCATE);
        if (ImGui::InputText("layer tag", layerBuffer, static_cast<size_t>(_countof(layerBuffer))))
        {
            playEffect->layerTag = String::ToWString(layerBuffer);
            eventEdited = true;
        }

        if (Draw_StringSuggestionCombo("layer presets", layerTag, Collect_LayerTagSuggestions(_sequence)))
        {
            playEffect->layerTag = String::ToWString(layerTag);
            eventEdited = true;
        }

        if (ImGui::DragFloat3("local offset", &playEffect->localOffset.x, 0.1f))
            eventEdited = true;
        if (ImGui::DragFloat3("local rotation", &playEffect->localRotation.x, 0.1f))
            eventEdited = true;
        if (ImGui::DragFloat3("scale", &playEffect->scale.x, 0.01f))
            eventEdited = true;
        if (ImGui::DragFloat("playback speed", &playEffect->playbackSpeed, 0.01f, 0.01f, 10.f, "%.2f"))
        {
            playEffect->playbackSpeed = std::max(playEffect->playbackSpeed, 0.01f);
            eventEdited = true;
        }
    }
    else if (auto* playSound = get_if<CinematicPlaySoundEventData>(&event.payload))
    {
        string soundCue = String::ToString(playSound->soundCue);
        char soundCueBuffer[128]{};
        strncpy_s(soundCueBuffer, soundCue.c_str(), _TRUNCATE);
        if (ImGui::InputText("sound cue", soundCueBuffer, static_cast<size_t>(_countof(soundCueBuffer))))
        {
            soundCue = soundCueBuffer;
            playSound->soundCue = String::ToWString(soundCue);
            eventEdited = true;
        }

        if (Draw_StringSuggestionCombo("sound cue presets", soundCue, Collect_SoundCueSuggestions(_sequence)))
        {
            playSound->soundCue = String::ToWString(soundCue);
            eventEdited = true;
        }

        if (ImGui::Checkbox("use target position", &playSound->useTargetPosition))
            eventEdited = true;

        if (ImGui::DragFloat("volume", &playSound->volume, 0.01f, 0.f, 10.f, "%.2f"))
        {
            playSound->volume = std::clamp(playSound->volume, 0.f, 10.f);
            eventEdited = true;
        }

        if (ImGui::DragFloat("speed", &playSound->speed, 0.01f, 0.1f, 3.f, "%.2f"))
        {
            playSound->speed = std::clamp(playSound->speed, 0.1f, 3.f);
            eventEdited = true;
        }

        if (ImGui::DragFloat("start ratio", &playSound->startRatio, 0.01f, 0.f, 0.99f, "%.2f"))
        {
            playSound->startRatio = std::clamp(playSound->startRatio, 0.f, 0.99f);
            eventEdited = true;
        }
    }
    else if (auto* spawn = get_if<CinematicSpawnEventData>(&event.payload))
    {
        char prefabBuffer[128]{};
        strncpy_s(prefabBuffer, spawn->prefabName.c_str(), _TRUNCATE);
        if (ImGui::InputText("prefab name", prefabBuffer, static_cast<size_t>(_countof(prefabBuffer))))
        {
            spawn->prefabName = prefabBuffer;
            eventEdited = true;
        }

        if (Draw_StringSuggestionCombo("prefab presets", spawn->prefabName, Collect_SpawnPrefabSuggestions(_sequence)))
            eventEdited = true;

        string layerTag = String::ToString(spawn->layerTag);
        char layerBuffer[128]{};
        strncpy_s(layerBuffer, layerTag.c_str(), _TRUNCATE);
        if (ImGui::InputText("layer tag", layerBuffer, static_cast<size_t>(_countof(layerBuffer))))
        {
            spawn->layerTag = String::ToWString(layerBuffer);
            eventEdited = true;
        }

        if (Draw_StringSuggestionCombo("layer presets", layerTag, Collect_LayerTagSuggestions(_sequence)))
        {
            spawn->layerTag = String::ToWString(layerTag);
            eventEdited = true;
        }

        if (ImGui::DragFloat3("spawn position", &spawn->position.x, 0.1f))
            eventEdited = true;
        if (ImGui::DragFloat3("spawn rotation", &spawn->rotation.x, 0.1f))
            eventEdited = true;
        ImGui::TextDisabled("spawn scale : prefab default");

        const char* cleanupLabel = spawn->cleanupPolicy == CinematicCleanupPolicy::Keep
            ? "Keep"
            : "DespawnOnStop";
        if (ImGui::BeginCombo("cleanup policy", cleanupLabel))
        {
            const bool despawnSelected = spawn->cleanupPolicy == CinematicCleanupPolicy::DespawnOnStop;
            if (ImGui::Selectable("DespawnOnStop", despawnSelected))
            {
                spawn->cleanupPolicy = CinematicCleanupPolicy::DespawnOnStop;
                eventEdited = true;
            }
            if (despawnSelected)
                ImGui::SetItemDefaultFocus();

            const bool keepSelected = spawn->cleanupPolicy == CinematicCleanupPolicy::Keep;
            if (ImGui::Selectable("Keep", keepSelected))
            {
                spawn->cleanupPolicy = CinematicCleanupPolicy::Keep;
                eventEdited = true;
            }
            if (keepSelected)
                ImGui::SetItemDefaultFocus();

            ImGui::EndCombo();
        }
    }
    else if (auto* partVisibility = get_if<CinematicPartVisibilityEventData>(&event.payload))
    {
        char partTagBuffer[128]{};
        strncpy_s(partTagBuffer, partVisibility->partTag.c_str(), _TRUNCATE);
        if (ImGui::InputText("part tag", partTagBuffer, static_cast<size_t>(_countof(partTagBuffer))))
        {
            partVisibility->partTag = partTagBuffer;
            eventEdited = true;
        }

        if (Draw_StringSuggestionCombo("part tag presets", partVisibility->partTag, Collect_PartTagSuggestions(_sequence)))
            eventEdited = true;
        if (ImGui::Checkbox("visible", &partVisibility->visible))
            eventEdited = true;
        ImGui::TextDisabled("Target should be a bound ContainerObject actor id.");
    }
    else if (auto* worldTimeScale = get_if<CinematicWorldTimeScaleEventData>(&event.payload))
    {
        if (ImGui::DragFloat("world time scale", &worldTimeScale->worldTimeScale, 0.01f, 0.f, 8.f, "%.2f"))
        {
            worldTimeScale->worldTimeScale = std::clamp(worldTimeScale->worldTimeScale, 0.f, 8.f);
            eventEdited = true;
        }
        ImGui::TextDisabled("Add another WorldTimeScale event later in the sequence to restore to 1.0 or a new value.");
    }

    const CINEMATIC_EVENT updatedEvent = event;
    const string selectedTargetId = updatedEvent.targetId;
    const int selectedFrame = updatedEvent.frame;
    const string selectedTypeLabel = Get_CinematicEventTypeLabel(updatedEvent);

    std::sort(_sequence.eventTrack.begin(), _sequence.eventTrack.end(),
        [](const CINEMATIC_EVENT& lhs, const CINEMATIC_EVENT& rhs)
        {
            return lhs.frame < rhs.frame;
        });

    for (int i = 0; i < static_cast<int>(_sequence.eventTrack.size()); ++i)
    {
        const CINEMATIC_EVENT& sortedEvent = _sequence.eventTrack[i];
        if (sortedEvent.frame != selectedFrame)
            continue;

        if (sortedEvent.targetId != selectedTargetId)
            continue;

        if (Get_CinematicEventTypeLabel(sortedEvent) != selectedTypeLabel)
            continue;

        _selectedEventIndex = i;
        break;
    }

    if (eventEdited)
        Request_CinematicPreviewRefresh(
            Should_RebuildPreviewForEventEdit(
                originalEvent,
                updatedEvent,
                _currentFrame,
                _previewApplyEventTrack));
}

void Cinematic_Tool::Render_ActorRelativeCameraTracks()
{
    Draw_SectionHeader("Actor Relative Camera Tracks");

    if (ImGui::Button("Add Actor Relative Track"))
    {
        CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK track{};
        track.name = "Actor Relative Track " + std::to_string(_sequence.actorRelativeCameraTracks.size());
        _sequence.actorRelativeCameraTracks.push_back(track);
        _selectedActorRelativeCameraTrackIndex = static_cast<int>(_sequence.actorRelativeCameraTracks.size()) - 1;
        _selectedActorRelativeCameraKeyIndex = -1;
        _selectedTrackIndex = Get_ActorRelativeTrackStartIndex(_sequence) + _selectedActorRelativeCameraTrackIndex;
    }

    ImGui::SameLine();
    const bool canRemoveTrack =
        _selectedActorRelativeCameraTrackIndex >= 0 &&
        _selectedActorRelativeCameraTrackIndex < static_cast<int>(_sequence.actorRelativeCameraTracks.size());
    if (!canRemoveTrack)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Actor Relative Track"))
    {
        _sequence.actorRelativeCameraTracks.erase(
            _sequence.actorRelativeCameraTracks.begin() + _selectedActorRelativeCameraTrackIndex);
        _selectedActorRelativeCameraTrackIndex = std::clamp(
            _selectedActorRelativeCameraTrackIndex,
            -1,
            static_cast<int>(_sequence.actorRelativeCameraTracks.size()) - 1);
        _selectedActorRelativeCameraKeyIndex = -1;
        Clamp_Selection();
    }

    if (!canRemoveTrack)
        ImGui::EndDisabled();

    if (ImGui::BeginListBox("##ActorRelativeCameraTrackList", ImVec2(-FLT_MIN, 100.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequence.actorRelativeCameraTracks.size()); ++i)
        {
            const auto& track = _sequence.actorRelativeCameraTracks[i];
            const string label = track.name.empty() ? ("Actor Relative Track " + std::to_string(i)) : track.name;
            if (ImGui::Selectable(label.c_str(), _selectedActorRelativeCameraTrackIndex == i))
            {
                _selectedActorRelativeCameraTrackIndex = i;
                _selectedActorRelativeCameraKeyIndex = -1;
                _selectedTrackIndex = Get_ActorRelativeTrackStartIndex(_sequence) + i;
            }
        }

        ImGui::EndListBox();
    }

    if (!canRemoveTrack)
        return;

    auto& track = _sequence.actorRelativeCameraTracks[_selectedActorRelativeCameraTrackIndex];

    char trackNameBuffer[128]{};
    strncpy_s(trackNameBuffer, track.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("actor relative track name", trackNameBuffer, static_cast<size_t>(_countof(trackNameBuffer))))
        track.name = trackNameBuffer;

    char targetBuffer[128]{};
    strncpy_s(targetBuffer, track.targetId.c_str(), _TRUNCATE);
    if (ImGui::InputText("actor relative target id", targetBuffer, static_cast<size_t>(_countof(targetBuffer))))
    {
        track.targetId = targetBuffer;
        Request_CinematicPreviewRefresh(true);
    }

    if (Draw_StringSuggestionCombo("actor relative target presets", track.targetId, Collect_TargetIdSuggestions(_sequence)))
        Request_CinematicPreviewRefresh(true);

    if (ImGui::Checkbox("actor relative track enabled", &track.enabled))
        Request_CinematicPreviewRefresh(false);
}

void Cinematic_Tool::Render_ActorTransformTracks()
{
    Draw_SectionHeader("Actor Transform Tracks");

    if (ImGui::Button("Add Actor Track"))
    {
        CINEMATIC_ACTOR_TRANSFORM_TRACK track{};
        track.name = "Actor Transform Track " + std::to_string(_sequence.actorTransformTracks.size());
        _sequence.actorTransformTracks.push_back(track);
        _selectedActorTransformTrackIndex = static_cast<int>(_sequence.actorTransformTracks.size()) - 1;
        _selectedActorTransformKeyIndex = -1;
    }

    ImGui::SameLine();
    const bool canRemoveTrack =
        _selectedActorTransformTrackIndex >= 0 &&
        _selectedActorTransformTrackIndex < static_cast<int>(_sequence.actorTransformTracks.size());
    if (!canRemoveTrack)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Actor Track"))
    {
        _sequence.actorTransformTracks.erase(
            _sequence.actorTransformTracks.begin() + _selectedActorTransformTrackIndex);
        _selectedActorTransformTrackIndex = std::clamp(
            _selectedActorTransformTrackIndex,
            -1,
            static_cast<int>(_sequence.actorTransformTracks.size()) - 1);
        _selectedActorTransformKeyIndex = -1;
    }

    if (!canRemoveTrack)
        ImGui::EndDisabled();

    if (ImGui::BeginListBox("##ActorTransformTrackList", ImVec2(-FLT_MIN, 100.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequence.actorTransformTracks.size()); ++i)
        {
            const auto& track = _sequence.actorTransformTracks[i];
            const string label = track.name.empty() ? ("Actor Track " + std::to_string(i)) : track.name;
            if (ImGui::Selectable(label.c_str(), _selectedActorTransformTrackIndex == i))
            {
                _selectedActorTransformTrackIndex = i;
                _selectedActorTransformKeyIndex = -1;
            }
        }

        ImGui::EndListBox();
    }

    if (!canRemoveTrack)
        return;

    auto& track = _sequence.actorTransformTracks[_selectedActorTransformTrackIndex];

    char trackNameBuffer[128]{};
    strncpy_s(trackNameBuffer, track.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("actor track name", trackNameBuffer, static_cast<size_t>(_countof(trackNameBuffer))))
        track.name = trackNameBuffer;

    char targetBuffer[128]{};
    strncpy_s(targetBuffer, track.targetId.c_str(), _TRUNCATE);
    if (ImGui::InputText("actor target id", targetBuffer, static_cast<size_t>(_countof(targetBuffer))))
        track.targetId = targetBuffer;

    Draw_StringSuggestionCombo("actor target presets", track.targetId, Collect_TargetIdSuggestions(_sequence));

    ImGui::Checkbox("actor track enabled", &track.enabled);

    Vec3 runtimePosition = Vec3::Zero;
    Vec3 runtimeRotation = Vec3::Zero;
    const bool hasRuntimeActorPose = Try_CaptureSelectedActorTransform(runtimePosition, runtimeRotation);
    const bool isCinematicPaused = CLIENT->Is_CinematicPaused();
    bool suspendActorTrackPreview = CLIENT->Is_ActorTransformTrackPreviewSuspended();

    if (!isCinematicPaused && suspendActorTrackPreview)
    {
        suspendActorTrackPreview = false;
        CLIENT->Set_ActorTransformTrackPreviewSuspended(false);
    }

    if (ImGui::Checkbox("suspend actor track preview", &suspendActorTrackPreview))
    {
        if (!isCinematicPaused)
            suspendActorTrackPreview = false;

        CLIENT->Set_ActorTransformTrackPreviewSuspended(suspendActorTrackPreview);
    }

    if (!isCinematicPaused)
        ImGui::TextDisabled("pause preview first to edit actor with gizmo safely");

    if (hasRuntimeActorPose)
    {
        ImGui::TextDisabled("runtime actor pose : Pos(%.2f, %.2f, %.2f) Rot(%.2f, %.2f, %.2f)",
            runtimePosition.x, runtimePosition.y, runtimePosition.z,
            runtimeRotation.x, runtimeRotation.y, runtimeRotation.z);
    }
    else
    {
        ImGui::TextDisabled("runtime actor pose : unavailable");
    }

    if (ImGui::Button("Add Actor Key"))
    {
        CINEMATIC_ACTOR_TRANSFORM_KEYFRAME keyframe{};
        keyframe.frame = _currentFrame;

        Vec3 previewPosition = Vec3::Zero;
        Vec3 previewRotation = Vec3::Zero;
        if (hasRuntimeActorPose)
        {
            keyframe.position = runtimePosition;
            keyframe.rotation = runtimeRotation;
        }
        else if (Evaluate_ActorTransformTrackPose(track, _currentFrame, previewPosition, previewRotation))
        {
            keyframe.position = previewPosition;
            keyframe.rotation = previewRotation;
        }

        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            if (track.keyframes[i].frame != keyframe.frame)
                continue;

            track.keyframes[i].position = keyframe.position;
            track.keyframes[i].rotation = keyframe.rotation;
            _selectedActorTransformKeyIndex = i;
            Request_CinematicPreviewRefresh(false);
            return;
        }

        track.keyframes.push_back(keyframe);
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& lhs, const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            if (track.keyframes[i].frame == keyframe.frame)
            {
                _selectedActorTransformKeyIndex = i;
                break;
            }
        }

        Request_CinematicPreviewRefresh(false);
    }

    ImGui::SameLine();
    const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME* previousActorKeyframe = nullptr;
    for (const auto& candidate : track.keyframes)
    {
        if (candidate.frame >= _currentFrame)
            continue;

        if (previousActorKeyframe == nullptr || candidate.frame > previousActorKeyframe->frame)
            previousActorKeyframe = &candidate;
    }

    if (previousActorKeyframe == nullptr)
        ImGui::BeginDisabled();

    if (ImGui::Button("Duplicate Prev Actor Key"))
    {
        CINEMATIC_ACTOR_TRANSFORM_KEYFRAME keyframe = *previousActorKeyframe;
        keyframe.frame = _currentFrame;

        bool replaced = false;
        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            if (track.keyframes[i].frame != keyframe.frame)
                continue;

            track.keyframes[i] = keyframe;
            _selectedActorTransformKeyIndex = i;
            replaced = true;
            break;
        }

        if (!replaced)
        {
            track.keyframes.push_back(keyframe);
            std::sort(track.keyframes.begin(), track.keyframes.end(),
                [](const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& lhs, const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& rhs)
                {
                    return lhs.frame < rhs.frame;
                });

            for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
            {
                if (track.keyframes[i].frame == keyframe.frame)
                {
                    _selectedActorTransformKeyIndex = i;
                    break;
                }
            }
        }

        Request_CinematicPreviewRefresh(false);
    }

    if (previousActorKeyframe == nullptr)
        ImGui::EndDisabled();

    ImGui::SameLine();
    const bool canRemoveKey =
        _selectedActorTransformKeyIndex >= 0 &&
        _selectedActorTransformKeyIndex < static_cast<int>(track.keyframes.size());
    if (!canRemoveKey)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Actor Key"))
    {
        track.keyframes.erase(track.keyframes.begin() + _selectedActorTransformKeyIndex);
        _selectedActorTransformKeyIndex = std::clamp(
            _selectedActorTransformKeyIndex,
            -1,
            static_cast<int>(track.keyframes.size()) - 1);
    }

    if (!canRemoveKey)
        ImGui::EndDisabled();

    if (ImGui::BeginListBox("##ActorTransformKeyList", ImVec2(-FLT_MIN, 120.f)))
    {
        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            const auto& keyframe = track.keyframes[i];
            char label[160]{};
            sprintf_s(label, "Frame %d | Pos(%.1f, %.1f, %.1f)",
                keyframe.frame,
                keyframe.position.x,
                keyframe.position.y,
                keyframe.position.z);

            if (ImGui::Selectable(label, _selectedActorTransformKeyIndex == i))
            {
                _selectedActorTransformKeyIndex = i;
                _currentFrame = keyframe.frame;
                _isPreviewDirty = true;
            }
        }

        ImGui::EndListBox();
    }

    if (!canRemoveKey)
        return;

    auto& keyframe = track.keyframes[_selectedActorTransformKeyIndex];

    const bool canCapturePoseToKey = canRemoveKey && hasRuntimeActorPose;
    if (!canCapturePoseToKey)
        ImGui::BeginDisabled();

    if (ImGui::Button("Capture From Runtime Actor"))
    {
        keyframe.position = runtimePosition;
        keyframe.rotation = runtimeRotation;
        Request_CinematicPreviewRefresh(false);
    }

    if (!canCapturePoseToKey)
        ImGui::EndDisabled();

    if (ImGui::DragInt("actor key frame", &keyframe.frame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        keyframe.frame = std::clamp(keyframe.frame, _sequence.startFrame, _sequence.endFrame);
        _currentFrame = keyframe.frame;
        Request_CinematicPreviewRefresh(false);
    }

    if (ImGui::DragFloat3("actor position", &keyframe.position.x, 0.1f))
        Request_CinematicPreviewRefresh(false);

    if (ImGui::DragFloat3("actor rotation", &keyframe.rotation.x, 0.1f))
        Request_CinematicPreviewRefresh(false);

    const char* interpolationLabel = magic_enum::enum_name(keyframe.interpolation).data();
    if (ImGui::BeginCombo("actor interpolation", interpolationLabel))
    {
        for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = keyframe.interpolation == mode;
            if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
                keyframe.interpolation = mode;
                Request_CinematicPreviewRefresh(false);
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& lhs, const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& rhs)
        {
            return lhs.frame < rhs.frame;
        });
}

void Cinematic_Tool::Render_PartRotationTracks()
{
    Draw_SectionHeader("Part Rotation Tracks");

    if (ImGui::Button("Add Part Rotation Track"))
    {
        CINEMATIC_PART_ROTATION_TRACK track{};
        track.name = "Part Rotation Track " + std::to_string(_sequence.partRotationTracks.size());
        _sequence.partRotationTracks.push_back(track);
        _selectedPartRotationTrackIndex = static_cast<int>(_sequence.partRotationTracks.size()) - 1;
        _selectedPartRotationKeyIndex = -1;
        _selectedTrackIndex = Get_PartRotationTrackStartIndex(_sequence) + _selectedPartRotationTrackIndex;
    }

    ImGui::SameLine();
    const bool canRemoveTrack =
        _selectedPartRotationTrackIndex >= 0 &&
        _selectedPartRotationTrackIndex < static_cast<int>(_sequence.partRotationTracks.size());
    if (!canRemoveTrack)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Part Rotation Track"))
    {
        _sequence.partRotationTracks.erase(
            _sequence.partRotationTracks.begin() + _selectedPartRotationTrackIndex);
        _selectedPartRotationTrackIndex = std::clamp(
            _selectedPartRotationTrackIndex,
            -1,
            static_cast<int>(_sequence.partRotationTracks.size()) - 1);
        _selectedPartRotationKeyIndex = -1;
        _selectedTrackIndex = std::clamp(
            _selectedTrackIndex,
            0,
            std::max(Get_PartRotationTrackStartIndex(_sequence) + static_cast<int>(_sequence.partRotationTracks.size()) - 1, 0));
    }

    if (!canRemoveTrack)
        ImGui::EndDisabled();

    if (ImGui::BeginListBox("##PartRotationTrackList", ImVec2(-FLT_MIN, 100.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequence.partRotationTracks.size()); ++i)
        {
            const auto& track = _sequence.partRotationTracks[i];
            const string label = track.name.empty() ? ("Part Rotation " + std::to_string(i)) : track.name;
            if (ImGui::Selectable(label.c_str(), _selectedPartRotationTrackIndex == i))
            {
                _selectedPartRotationTrackIndex = i;
                _selectedPartRotationKeyIndex = -1;
                _selectedTrackIndex = Get_PartRotationTrackStartIndex(_sequence) + i;
            }
        }

        ImGui::EndListBox();
    }

    if (!canRemoveTrack)
        return;

    auto& track = _sequence.partRotationTracks[_selectedPartRotationTrackIndex];

    char trackNameBuffer[128]{};
    strncpy_s(trackNameBuffer, track.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("part rotation track name", trackNameBuffer, static_cast<size_t>(_countof(trackNameBuffer))))
        track.name = trackNameBuffer;

    char targetBuffer[128]{};
    strncpy_s(targetBuffer, track.targetId.c_str(), _TRUNCATE);
    if (ImGui::InputText("part rotation target id", targetBuffer, static_cast<size_t>(_countof(targetBuffer))))
    {
        track.targetId = targetBuffer;
        Request_CinematicPreviewRefresh(false);
    }

    if (Draw_StringSuggestionCombo("part rotation target presets", track.targetId, Collect_TargetIdSuggestions(_sequence)))
        Request_CinematicPreviewRefresh(false);

    char partTagBuffer[128]{};
    strncpy_s(partTagBuffer, track.partTag.c_str(), _TRUNCATE);
    if (ImGui::InputText("part tag", partTagBuffer, static_cast<size_t>(_countof(partTagBuffer))))
    {
        track.partTag = partTagBuffer;
        Request_CinematicPreviewRefresh(false);
    }

    if (Draw_StringSuggestionCombo("part tag presets", track.partTag, Collect_PartTagSuggestions(_sequence)))
        Request_CinematicPreviewRefresh(false);

    if (ImGui::Checkbox("part rotation track enabled", &track.enabled))
        Request_CinematicPreviewRefresh(false);

    Vec3 runtimeRotation = Vec3::Zero;
    const bool hasRuntimePartRotation = Try_CaptureSelectedPartRotation(runtimeRotation);
    if (hasRuntimePartRotation)
    {
        ImGui::TextDisabled("runtime part rotation : Rot(%.2f, %.2f, %.2f)",
            runtimeRotation.x,
            runtimeRotation.y,
            runtimeRotation.z);
    }
    else
    {
        ImGui::TextDisabled("runtime part rotation : unavailable");
    }

    if (ImGui::Button("Add Part Rotation Key"))
    {
        CINEMATIC_PART_ROTATION_KEYFRAME keyframe{};
        keyframe.frame = _currentFrame;

        Vec3 previewRotation = Vec3::Zero;
        if (hasRuntimePartRotation)
            keyframe.rotation = runtimeRotation;
        else if (Evaluate_PartRotationTrackPose(track, _currentFrame, previewRotation))
            keyframe.rotation = previewRotation;

        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            if (track.keyframes[i].frame != keyframe.frame)
                continue;

            track.keyframes[i].rotation = keyframe.rotation;
            _selectedPartRotationKeyIndex = i;
            Request_CinematicPreviewRefresh(false);
            return;
        }

        track.keyframes.push_back(keyframe);
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const CINEMATIC_PART_ROTATION_KEYFRAME& lhs, const CINEMATIC_PART_ROTATION_KEYFRAME& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            if (track.keyframes[i].frame == keyframe.frame)
            {
                _selectedPartRotationKeyIndex = i;
                break;
            }
        }

        Request_CinematicPreviewRefresh(false);
    }

    ImGui::SameLine();
    const CINEMATIC_PART_ROTATION_KEYFRAME* previousPartKeyframe = nullptr;
    for (const auto& candidate : track.keyframes)
    {
        if (candidate.frame >= _currentFrame)
            continue;

        if (previousPartKeyframe == nullptr || candidate.frame > previousPartKeyframe->frame)
            previousPartKeyframe = &candidate;
    }

    if (previousPartKeyframe == nullptr)
        ImGui::BeginDisabled();

    if (ImGui::Button("Duplicate Prev Part Key"))
    {
        CINEMATIC_PART_ROTATION_KEYFRAME keyframe = *previousPartKeyframe;
        keyframe.frame = _currentFrame;

        bool replaced = false;
        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            if (track.keyframes[i].frame != keyframe.frame)
                continue;

            track.keyframes[i] = keyframe;
            _selectedPartRotationKeyIndex = i;
            replaced = true;
            break;
        }

        if (!replaced)
        {
            track.keyframes.push_back(keyframe);
            std::sort(track.keyframes.begin(), track.keyframes.end(),
                [](const CINEMATIC_PART_ROTATION_KEYFRAME& lhs, const CINEMATIC_PART_ROTATION_KEYFRAME& rhs)
                {
                    return lhs.frame < rhs.frame;
                });

            for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
            {
                if (track.keyframes[i].frame == keyframe.frame)
                {
                    _selectedPartRotationKeyIndex = i;
                    break;
                }
            }
        }

        Request_CinematicPreviewRefresh(false);
    }

    if (previousPartKeyframe == nullptr)
        ImGui::EndDisabled();

    ImGui::SameLine();
    const bool canRemoveKey =
        _selectedPartRotationKeyIndex >= 0 &&
        _selectedPartRotationKeyIndex < static_cast<int>(track.keyframes.size());
    if (!canRemoveKey)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Part Rotation Key"))
    {
        track.keyframes.erase(track.keyframes.begin() + _selectedPartRotationKeyIndex);
        _selectedPartRotationKeyIndex = std::clamp(
            _selectedPartRotationKeyIndex,
            -1,
            static_cast<int>(track.keyframes.size()) - 1);
        Request_CinematicPreviewRefresh(false);
    }

    if (!canRemoveKey)
        ImGui::EndDisabled();

    if (ImGui::BeginListBox("##PartRotationKeyList", ImVec2(-FLT_MIN, 120.f)))
    {
        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            const auto& keyframe = track.keyframes[i];
            char label[160]{};
            sprintf_s(label, "Frame %d | Rot(%.1f, %.1f, %.1f)",
                keyframe.frame,
                keyframe.rotation.x,
                keyframe.rotation.y,
                keyframe.rotation.z);

            if (ImGui::Selectable(label, _selectedPartRotationKeyIndex == i))
            {
                _selectedPartRotationKeyIndex = i;
                _currentFrame = keyframe.frame;
                _isPreviewDirty = true;
            }
        }

        ImGui::EndListBox();
    }

    if (!canRemoveKey)
        return;

    auto& keyframe = track.keyframes[_selectedPartRotationKeyIndex];

    if (!hasRuntimePartRotation)
        ImGui::BeginDisabled();

    if (ImGui::Button("Capture From Runtime Part"))
    {
        keyframe.rotation = runtimeRotation;
        Request_CinematicPreviewRefresh(false);
    }

    if (!hasRuntimePartRotation)
        ImGui::EndDisabled();

    if (ImGui::DragInt("part rotation key frame", &keyframe.frame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        keyframe.frame = std::clamp(keyframe.frame, _sequence.startFrame, _sequence.endFrame);
        _currentFrame = keyframe.frame;
        Request_CinematicPreviewRefresh(false);
    }

    if (ImGui::DragFloat3("part rotation", &keyframe.rotation.x, 0.1f))
        Request_CinematicPreviewRefresh(false);

    const char* interpolationLabel = magic_enum::enum_name(keyframe.interpolation).data();
    if (ImGui::BeginCombo("part rotation interpolation", interpolationLabel))
    {
        for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = keyframe.interpolation == mode;
            if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
                keyframe.interpolation = mode;
                Request_CinematicPreviewRefresh(false);
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const CINEMATIC_PART_ROTATION_KEYFRAME& lhs, const CINEMATIC_PART_ROTATION_KEYFRAME& rhs)
        {
            return lhs.frame < rhs.frame;
        });
}

void Cinematic_Tool::Render_LoadSequencePopup()
{
    if (_requestOpenLoadSequencePopup)
    {
        ImGui::OpenPopup("Load Cinematic Sequence");
        _requestOpenLoadSequencePopup = false;
    }

    if (!ImGui::BeginPopupModal("Load Cinematic Sequence", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (_sequenceFilePaths.empty())
    {
        ImGui::TextDisabled("No cinematic sequence files found.");
    }
    else if (ImGui::BeginListBox("##CinematicSequenceFiles", ImVec2(420.f, 240.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequenceFilePaths.size()); ++i)
        {
            const string label = _sequenceFilePaths[i].filename().string();
            const bool isSelected = (_selectedSequenceFileIndex == i);

            if (ImGui::Selectable(label.c_str(), isSelected))
                _selectedSequenceFileIndex = i;

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
            {
                _selectedSequenceFileIndex = i;
                _requestOpenLoadSequenceConfirmPopup = true;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndListBox();
    }

    ImGui::Separator();

    if (_selectedSequenceFileIndex >= 0 &&
        _selectedSequenceFileIndex < static_cast<int>(_sequenceFilePaths.size()))
    {
        ImGui::TextDisabled("Selected : %s", _sequenceFilePaths[_selectedSequenceFileIndex].filename().string().c_str());
    }
    else
    {
        ImGui::TextDisabled("No file selected.");
    }

    if (_selectedSequenceFileIndex < 0)
        ImGui::BeginDisabled();

    if (ImGui::Button("Load", ImVec2(120.f, 0.f)))
    {
        _requestOpenLoadSequenceConfirmPopup = true;
        ImGui::CloseCurrentPopup();
    }

    if (_selectedSequenceFileIndex < 0)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_SequenceOverwriteWarningPopups()
{
    if (_requestOpenSaveSequenceConfirmPopup)
    {
        ImGui::OpenPopup("Confirm Save Sequence");
        _requestOpenSaveSequenceConfirmPopup = false;
    }

    if (ImGui::BeginPopupModal("Confirm Save Sequence", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        const string targetFileName = _sequence.name + ".json";
        ImGui::Text("Saving will overwrite the current sequence file if it already exists.");
        ImGui::Separator();
        ImGui::TextDisabled("Target: %s", targetFileName.c_str());

        if (ImGui::Button("Save Anyway", ImVec2(140.f, 0.f)))
        {
            Cinematic_SequenceIO::SaveSequenceToFile(
                _sequence,
                "../../../Client/Bin/Resources/Data/json/Cinematics/" + _sequence.name + ".json");
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (_requestOpenLoadSequenceConfirmPopup)
    {
        ImGui::OpenPopup("Confirm Load Sequence");
        _requestOpenLoadSequenceConfirmPopup = false;
    }

    if (ImGui::BeginPopupModal("Confirm Load Sequence", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Loading a sequence will overwrite the current unsaved cinematic editing state.");
        ImGui::Separator();

        if (_selectedSequenceFileIndex >= 0 &&
            _selectedSequenceFileIndex < static_cast<int>(_sequenceFilePaths.size()))
        {
            ImGui::TextDisabled(
                "Selected: %s",
                _sequenceFilePaths[_selectedSequenceFileIndex].filename().string().c_str());
        }

        if (ImGui::Button("Load Anyway", ImVec2(140.f, 0.f)))
        {
            Load_SelectedSequenceFile();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(140.f, 0.f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (_requestOpenNewEmptySequenceConfirmPopup)
    {
        ImGui::OpenPopup("Confirm New Empty Sequence");
        _requestOpenNewEmptySequenceConfirmPopup = false;
    }

    if (ImGui::BeginPopupModal("Confirm New Empty Sequence", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Creating a new empty sequence will discard the current cinematic editing state.");
        ImGui::Separator();

        if (ImGui::Button("Create Empty Sequence", ImVec2(170.f, 0.f)))
        {
            Reset_ToNewSequence();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (_requestOpenClearAllKeysConfirmPopup)
    {
        ImGui::OpenPopup("Confirm Clear All Keys");
        _requestOpenClearAllKeysConfirmPopup = false;
    }

    if (ImGui::BeginPopupModal("Confirm Clear All Keys", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("This will remove all cinematic, modifier, and actor keyframes from the current sequence.");
        ImGui::TextDisabled("Event track entries are not removed.");
        ImGui::Separator();

        if (ImGui::Button("Clear All Keys", ImVec2(140.f, 0.f)))
        {
            Clear_AllSequenceKeyframes();
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void Cinematic_Tool::Render_CameraShots()
{
    Draw_SectionHeader("Camera Shots");

    if (ImGui::Button("Add Shot"))
    {
        CINEMATIC_CAMERA_SHOT shot{};
        shot.name = "Shot " + std::to_string(_sequence.cameraShots.size());
        shot.startFrame = _currentFrame;
        shot.endFrame = std::clamp(_currentFrame + std::max(_sequence.fps, 1), _sequence.startFrame, _sequence.endFrame);
        shot.trackIndex = std::clamp(_selectedTrackIndex, 0, std::max(Get_CinematicCameraTrackCount(_sequence) - 1, 0));
        _sequence.cameraShots.push_back(shot);
        _selectedCameraShotIndex = static_cast<int>(_sequence.cameraShots.size()) - 1;
        _isPreviewDirty = true;
    }

    ImGui::SameLine();
    const bool canRemoveShot =
        _selectedCameraShotIndex >= 0 &&
        _selectedCameraShotIndex < static_cast<int>(_sequence.cameraShots.size());
    if (!canRemoveShot)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Shot"))
    {
        _sequence.cameraShots.erase(_sequence.cameraShots.begin() + _selectedCameraShotIndex);
        _selectedCameraShotIndex = std::clamp(
            _selectedCameraShotIndex,
            -1,
            static_cast<int>(_sequence.cameraShots.size()) - 1);
        _isPreviewDirty = true;
    }

    if (!canRemoveShot)
        ImGui::EndDisabled();

    if (_sequence.cameraShots.empty())
    {
        ImGui::TextDisabled("No camera shots. Existing single-track playback stays active.");
        return;
    }

    std::sort(_sequence.cameraShots.begin(), _sequence.cameraShots.end(),
        [](const CINEMATIC_CAMERA_SHOT& lhs, const CINEMATIC_CAMERA_SHOT& rhs)
        {
            if (lhs.startFrame != rhs.startFrame)
                return lhs.startFrame < rhs.startFrame;

            return lhs.endFrame < rhs.endFrame;
        });

    if (ImGui::BeginListBox("##CameraShotList", ImVec2(-FLT_MIN, 110.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequence.cameraShots.size()); ++i)
        {
            const auto& shot = _sequence.cameraShots[i];
            char label[192]{};
            sprintf_s(label,
                "%s | %d-%d | %s",
                shot.name.c_str(),
                shot.startFrame,
                shot.endFrame,
                Build_CameraTrackLabel(_sequence, shot.trackIndex).c_str());

            if (ImGui::Selectable(label, _selectedCameraShotIndex == i))
            {
                _selectedCameraShotIndex = i;
                _currentFrame = std::clamp(shot.startFrame, _sequence.startFrame, _sequence.endFrame);
                _currentFrameExact = static_cast<float>(_currentFrame);
                _isPreviewDirty = true;
            }
        }

        ImGui::EndListBox();
    }

    if (!canRemoveShot)
        return;

    auto& shot = _sequence.cameraShots[_selectedCameraShotIndex];
    char shotNameBuffer[128]{};
    strncpy_s(shotNameBuffer, shot.name.c_str(), _TRUNCATE);
    if (ImGui::InputText("shot name", shotNameBuffer, static_cast<size_t>(_countof(shotNameBuffer))))
        shot.name = shotNameBuffer;

    if (ImGui::BeginCombo("shot track", Build_CameraTrackLabel(_sequence, shot.trackIndex).c_str()))
    {
        for (int i = 0; i < Get_CinematicCameraTrackCount(_sequence); ++i)
        {
            const string label = Build_CameraTrackLabel(_sequence, i);
            const bool isSelected = (shot.trackIndex == i);
            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                shot.trackIndex = i;
                _isPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (ImGui::Checkbox("shot enabled", &shot.enabled))
        _isPreviewDirty = true;

    if (ImGui::DragInt("shot start", &shot.startFrame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        shot.startFrame = std::clamp(shot.startFrame, _sequence.startFrame, _sequence.endFrame);
        shot.endFrame = std::max(shot.endFrame, shot.startFrame);
        _isPreviewDirty = true;
    }

    if (ImGui::DragInt("shot end", &shot.endFrame, 1.f, shot.startFrame, _sequence.endFrame))
    {
        shot.endFrame = std::clamp(shot.endFrame, shot.startFrame, _sequence.endFrame);
        _isPreviewDirty = true;
    }
}

void Cinematic_Tool::Render_ImSequencer()
{
    Clamp_Selection();

    if (EDITOR->IsPlaying())
    {
        ImGui::TextDisabled("Timeline is disabled while editor runtime is playing.");
        return;
    }

    bool consumedKeyClick = false;
    bool requestOpenKeyPopup = false;
    bool requestOpenInsertPopup = false;
    bool requestRemoveKeyPopup = false;
    int  sequencerSelectedTrackIndex = _selectedTrackIndex;
    const int previousFrame = _currentFrame;
    const float previousFrameExact = _currentFrameExact;
    const int previousTrackIndex = _selectedTrackIndex;
    const int previousKeyIndex = _selectedKeyIndex;
    const int fullFrameMin = _sequence.startFrame;
    const int fullFrameMax = std::max(_sequence.startFrame + 1, _sequence.endFrame);
    const int fullFrameRange = std::max(1, fullFrameMax - fullFrameMin);

    if (_sequencerVisibleFrameCount <= 0 || _sequencerVisibleFrameCount > fullFrameRange)
        _sequencerVisibleFrameCount = fullFrameRange;
    _sequencerFirstFrame = std::clamp(
        _sequencerFirstFrame,
        fullFrameMin,
        std::max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount));

    CinematicSequencerAdapter adapter(
        &_sequence,
        &_selectedTrackIndex,
        &_selectedKeyIndex,
        &_selectedEventIndex,
        &_selectedActorRelativeCameraTrackIndex,
        &_selectedActorRelativeCameraKeyIndex,
        &_selectedActorTransformTrackIndex,
        &_selectedActorTransformKeyIndex,
        &_selectedPartRotationTrackIndex,
        &_selectedPartRotationKeyIndex,
        &_currentFrame,
        &consumedKeyClick,
        &requestOpenKeyPopup,
        &requestOpenInsertPopup,
        &requestRemoveKeyPopup, 
        &_contextMenuTrackIndex,
        &_contextMenuFrame,
        &_isPreviewDirty,
        nullptr,
        nullptr,
        nullptr);

    static bool expanded = true;
    const int sequencerItemCount = std::max(1, adapter.GetItemCount());
    const float sequencerHeight = std::max(120.f, 20.f + 30.f * static_cast<float>(sequencerItemCount) + 10.f);
    ImRect sequencerItemRect(ImGui::GetCursorScreenPos(), ImGui::GetCursorScreenPos());
    bool sequencerHovered = false;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    if (ImGui::BeginChild("CinematicSequencerFixedHeight", ImVec2(0.f, sequencerHeight), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImSequencer::Sequencer(
            &adapter,
            &_currentFrame,
            &expanded,
            &sequencerSelectedTrackIndex,
            &_sequencerFirstFrame,
            ImSequencer::SEQUENCER_CHANGE_FRAME,
            _sequencerFirstFrame,
            _sequencerFirstFrame + _sequencerVisibleFrameCount,
            &_currentFrameExact);

        sequencerItemRect = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        sequencerHovered = ImGui::IsItemHovered();

        const ImGuiIO& io = ImGui::GetIO();
        if (sequencerHovered && io.KeyCtrl && io.MouseWheel != 0.f)
        {
            const float timelineMinX = sequencerItemRect.Min.x + 200.f;
            const float timelineMaxX = sequencerItemRect.Max.x;
            const float timelineWidth = std::max(1.f, timelineMaxX - timelineMinX);
            const float mouseRatio = std::clamp((io.MousePos.x - timelineMinX) / timelineWidth, 0.f, 1.f);
            const int oldVisibleFrameCount = std::clamp(_sequencerVisibleFrameCount, 1, fullFrameRange);
            const int mouseFrame = _sequencerFirstFrame + static_cast<int>(std::round(mouseRatio * static_cast<float>(oldVisibleFrameCount)));
            const float zoomScale = io.MouseWheel > 0.f ? 0.82f : 1.22f;
            const int minVisibleFrameCount = std::min(fullFrameRange, 8);
            const int newVisibleFrameCount = std::clamp(
                static_cast<int>(std::round(static_cast<float>(oldVisibleFrameCount) * zoomScale)),
                minVisibleFrameCount,
                fullFrameRange);
            const float pivotRatio = static_cast<float>(mouseFrame - _sequencerFirstFrame) / static_cast<float>(std::max(1, oldVisibleFrameCount));
            const int requestedFirstFrame = mouseFrame - static_cast<int>(std::round(pivotRatio * static_cast<float>(newVisibleFrameCount)));
            _sequencerVisibleFrameCount = newVisibleFrameCount;
            _sequencerFirstFrame = std::clamp(
                requestedFirstFrame,
                fullFrameMin,
                std::max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount));
        }
        else if (sequencerHovered && io.KeyShift && !io.KeyCtrl && io.MouseWheel != 0.f)
        {
            const int scrollMax = std::max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount);
            const int scrollStep = std::max(1, static_cast<int>(std::round(static_cast<float>(_sequencerVisibleFrameCount) * 0.1f)));
            const int direction = io.MouseWheel > 0.f ? -1 : 1;
            _sequencerFirstFrame = std::clamp(_sequencerFirstFrame + direction * scrollStep, fullFrameMin, scrollMax);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();

    const int scrollMaxFrame = std::max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount);
    const float scrollbarOffsetX = 200.f;
    const float scrollbarHeight = 16.f;
    const float scrollbarWidth = std::max(1.f, sequencerItemRect.Max.x - (sequencerItemRect.Min.x + scrollbarOffsetX));
    const bool scrollbarEnabled = _sequencerVisibleFrameCount < fullFrameRange && scrollMaxFrame > fullFrameMin;

    ImGui::Dummy(ImVec2(scrollbarOffsetX, scrollbarHeight));
    ImGui::SameLine(0.f, 0.f);
    ImGui::InvisibleButton("##CinematicSequencerHorizontalScroll", ImVec2(scrollbarWidth, scrollbarHeight));

    const ImRect scrollbarRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    const bool scrollbarHovered = ImGui::IsItemHovered();
    const bool scrollbarActive = ImGui::IsItemActive();
    const ImGuiIO& scrollbarIO = ImGui::GetIO();
    const float visibleRatio = std::clamp(static_cast<float>(_sequencerVisibleFrameCount) / static_cast<float>(fullFrameRange), 0.f, 1.f);
    const float thumbWidth = std::clamp(scrollbarWidth * visibleRatio, std::min(34.f, scrollbarWidth), scrollbarWidth);
    const float thumbTravel = std::max(1.f, scrollbarWidth - thumbWidth);

    if (scrollbarEnabled && scrollbarActive)
    {
        const float dragRatio = std::clamp((scrollbarIO.MousePos.x - scrollbarRect.Min.x - thumbWidth * 0.5f) / thumbTravel, 0.f, 1.f);
        const int requestedFirstFrame = fullFrameMin + static_cast<int>(std::round(dragRatio * static_cast<float>(scrollMaxFrame - fullFrameMin)));
        _sequencerFirstFrame = std::clamp(requestedFirstFrame, fullFrameMin, scrollMaxFrame);
    }

    const float scrollRatio = scrollMaxFrame > fullFrameMin
        ? std::clamp(static_cast<float>(_sequencerFirstFrame - fullFrameMin) / static_cast<float>(scrollMaxFrame - fullFrameMin), 0.f, 1.f)
        : 0.f;
    const float thumbMinX = scrollbarRect.Min.x + thumbTravel * scrollRatio;
    const ImRect thumbRect(
        ImVec2(thumbMinX, scrollbarRect.Min.y + 2.f),
        ImVec2(thumbMinX + thumbWidth, scrollbarRect.Max.y - 2.f));

    ImDrawList* scrollbarDrawList = ImGui::GetWindowDrawList();
    scrollbarDrawList->AddRectFilled(
        scrollbarRect.Min,
        scrollbarRect.Max,
        scrollbarEnabled ? IM_COL32(45, 47, 53, 255) : IM_COL32(38, 38, 42, 160),
        4.f);
    scrollbarDrawList->AddRect(
        scrollbarRect.Min,
        scrollbarRect.Max,
        IM_COL32(80, 82, 90, 180),
        4.f);
    scrollbarDrawList->AddRectFilled(
        thumbRect.Min,
        thumbRect.Max,
        !scrollbarEnabled ? IM_COL32(72, 72, 78, 160) :
            (scrollbarActive ? IM_COL32(118, 166, 255, 255) :
                (scrollbarHovered ? IM_COL32(92, 138, 224, 255) : IM_COL32(72, 112, 190, 255))),
        4.f);

    const int minVisibleFrameCount = std::min(fullFrameRange, 8);
    ImGui::SetNextItemWidth(std::max(160.f, scrollbarWidth));
    const int previousVisibleFrameCount = _sequencerVisibleFrameCount;
    if (ImGui::SliderInt("visible frames", &_sequencerVisibleFrameCount, minVisibleFrameCount, fullFrameRange))
    {
        _sequencerVisibleFrameCount = std::clamp(_sequencerVisibleFrameCount, minVisibleFrameCount, fullFrameRange);

        const float pivotFrame = std::clamp(
            _useExactFramePreview ? _currentFrameExact : static_cast<float>(_currentFrame),
            static_cast<float>(fullFrameMin),
            static_cast<float>(fullFrameMax));
        const float pivotRatio = previousVisibleFrameCount > 0
            ? std::clamp(
                (pivotFrame - static_cast<float>(_sequencerFirstFrame)) / static_cast<float>(previousVisibleFrameCount),
                0.f,
                1.f)
            : 0.5f;
        const int requestedFirstFrame = static_cast<int>(std::round(
            pivotFrame - pivotRatio * static_cast<float>(_sequencerVisibleFrameCount)));
        _sequencerFirstFrame = std::clamp(
            requestedFirstFrame,
            fullFrameMin,
            std::max(fullFrameMin, fullFrameMax - _sequencerVisibleFrameCount));
    }

    if (!consumedKeyClick)
        _selectedTrackIndex = sequencerSelectedTrackIndex;

    if (consumedKeyClick || requestOpenKeyPopup || requestOpenInsertPopup || requestRemoveKeyPopup)
        _currentFrameExact = static_cast<float>(_currentFrame);

    if (previousFrame != _currentFrame || std::fabs(previousFrameExact - _currentFrameExact) > 0.0001f)
    {
        _useExactFramePreview = std::fabs(_currentFrameExact - static_cast<float>(_currentFrame)) > 0.0001f;
        Request_CinematicPreviewRefresh(false);
    }

    if (requestOpenKeyPopup)
    {
        _requestOpenSelectedKeyPopup = true;
        Enter_FreeCameraEditPreview();
    }

    if (requestOpenInsertPopup)
    {
        _hasPendingInsertRequest = true;
        _pendingInsertTrackIndex = _contextMenuTrackIndex;
        _pendingInsertFrame = _contextMenuFrame;
        _hasPendingInsertPose = false;
        if (_contextMenuTrackIndex >= 0 &&
            _contextMenuTrackIndex < static_cast<int>(_sequence.cinematicTracks.size()))
        {
            _hasPendingInsertPose = Try_CaptureActiveFreeCameraPose(_pendingInsertPose);
        }

        _requestOpenInsertKeyPopup = true;
    }

    if(requestRemoveKeyPopup)
        _requestRemoveKeyPopup = true;

    if (previousFrame != _currentFrame ||
        previousTrackIndex != _selectedTrackIndex ||
        previousKeyIndex != _selectedKeyIndex)
    {
        _isPreviewDirty = true;
    }
}

void Cinematic_Tool::Render_TrackList()
{

    Draw_SectionHeader("Cinematic Tracks");

    if (ImGui::Button("Add World Track"))
    {
        CINEMATIC_TRACK track{};
        track.name = "World Track " + std::to_string(_sequence.cinematicTracks.size());
        _sequence.cinematicTracks.push_back(track);
        _selectedTrackIndex = static_cast<int>(_sequence.cinematicTracks.size()) - 1;
        _selectedKeyIndex = -1;
        _isPreviewDirty = true;
    }

    ImGui::SameLine();

    if (ImGui::Button("Add Actor Relative Track"))
    {
        CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK track{};
        track.name = "Actor Relative Track " + std::to_string(_sequence.actorRelativeCameraTracks.size());
        _sequence.actorRelativeCameraTracks.push_back(track);
        _selectedActorRelativeCameraTrackIndex = static_cast<int>(_sequence.actorRelativeCameraTracks.size()) - 1;
        _selectedTrackIndex = Get_ActorRelativeTrackStartIndex(_sequence) + _selectedActorRelativeCameraTrackIndex;
        _selectedKeyIndex = -1;
        _selectedActorRelativeCameraKeyIndex = -1;
        _isPreviewDirty = true;
    }
    ImGui::SameLine();
    const bool isRemoveTrackDisabled =
        (_selectedTrackIndex < 0) ||
        (!Get_SelectedTrack() && !Get_SelectedActorRelativeCameraTrack());
    if (isRemoveTrackDisabled)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Track"))
    {
        int removedTrackIndex = -1;
        if (CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack())
        {
            removedTrackIndex = _selectedTrackIndex;
            _sequence.cinematicTracks.erase(_sequence.cinematicTracks.begin() + _selectedTrackIndex);
            _selectedKeyIndex = -1;
        }
        else if (Is_SelectedActorRelativeCameraTrack())
        {
            const int relativeTrackIndex = Get_SelectedActorRelativeCameraTrackIndex();
            if (relativeTrackIndex >= 0 &&
                relativeTrackIndex < static_cast<int>(_sequence.actorRelativeCameraTracks.size()))
            {
                removedTrackIndex = Get_ActorRelativeTrackStartIndex(_sequence) + relativeTrackIndex;
                _sequence.actorRelativeCameraTracks.erase(_sequence.actorRelativeCameraTracks.begin() + relativeTrackIndex);
                _selectedActorRelativeCameraKeyIndex = -1;
            }
        }

        if (removedTrackIndex >= 0)
        {
            const int maxTrackIndex = std::max(Get_CinematicCameraTrackCount(_sequence) - 1, 0);
            for (auto& shot : _sequence.cameraShots)
            {
                if (shot.trackIndex == removedTrackIndex)
                    shot.trackIndex = std::clamp(shot.trackIndex, 0, maxTrackIndex);
                else if (shot.trackIndex > removedTrackIndex)
                    --shot.trackIndex;
            }
        }

        Clamp_Selection();
        _isPreviewDirty = true;
    }

    if (isRemoveTrackDisabled)
        ImGui::EndDisabled();

    if (ImGui::BeginListBox("##CameraTrackList", ImVec2(-FLT_MIN, 120.f)))
    {
        for (int i = 0; i < static_cast<int>(_sequence.cinematicTracks.size()); ++i)
        {
            CINEMATIC_TRACK& track = _sequence.cinematicTracks[i];
            const bool isSelected = (_selectedTrackIndex == i);
            const string label = track.name.empty() ? ("Track " + std::to_string(i)) : track.name;

            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                _selectedTrackIndex = i;
                _selectedKeyIndex = -1;
                _isPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        const int actorRelativeTrackStart = Get_ActorRelativeTrackStartIndex(_sequence);
        for (int i = 0; i < static_cast<int>(_sequence.actorRelativeCameraTracks.size()); ++i)
        {
            auto& track = _sequence.actorRelativeCameraTracks[i];
            const int trackIndex = actorRelativeTrackStart + i;
            const bool isSelected = (_selectedTrackIndex == trackIndex);
            const string label = track.name.empty() ? ("Actor Relative " + std::to_string(i)) : track.name;

            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                _selectedTrackIndex = trackIndex;
                _selectedKeyIndex = -1;
                _selectedActorRelativeCameraTrackIndex = i;
                _selectedActorRelativeCameraKeyIndex = -1;
                _isPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndListBox();
    }

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
    {
        if (CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* relativeTrack = Get_SelectedActorRelativeCameraTrack())
        {
            char trackNameBuffer[128]{};
            strncpy_s(trackNameBuffer, relativeTrack->name.c_str(), _TRUNCATE);
            if (ImGui::InputText("track name", trackNameBuffer, static_cast<size_t>(_countof(trackNameBuffer))))
                relativeTrack->name = trackNameBuffer;

            char targetBuffer[128]{};
            strncpy_s(targetBuffer, relativeTrack->targetId.c_str(), _TRUNCATE);
            if (ImGui::InputText("target id", targetBuffer, static_cast<size_t>(_countof(targetBuffer))))
            {
                relativeTrack->targetId = targetBuffer;
                _isPreviewDirty = true;
            }

            if (ImGui::Checkbox("track enabled", &relativeTrack->enabled))
                _isPreviewDirty = true;
            return;
        }

        if (Is_SelectedModifierTrack())
            ImGui::TextDisabled("Modifier track is selected in the timeline.");
        return;
    }

    char trackNameBuffer[128]{};
    strncpy_s(trackNameBuffer, selectedTrack->name.c_str(), _TRUNCATE);
    if (ImGui::InputText("track name", trackNameBuffer, static_cast<size_t>(_countof(trackNameBuffer))))
        selectedTrack->name = trackNameBuffer;

    if (ImGui::Checkbox("track enabled", &selectedTrack->enabled))
        _isPreviewDirty = true;
}

void Cinematic_Tool::Render_KeyframeList()
{
    if (Is_SelectedActorRelativeCameraTrack())
    {
        CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* selectedTrack = Get_SelectedActorRelativeCameraTrack();
        if (selectedTrack == nullptr)
        {
            ImGui::TextDisabled("No actor-relative track selected.");
            return;
        }

        Draw_SectionHeader("Actor Relative Keys");

        if (ImGui::Button("Add Relative Key"))
        {
            CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME keyframe{};
            keyframe.frame = _currentFrame;

            float distance = keyframe.distance;
            float height = keyframe.height;
            float sideOffset = keyframe.sideOffset;
            float yawOffset = keyframe.yawOffset;
            float pitch = keyframe.pitch;
            float lookHeight = keyframe.lookHeight;
            if (Try_CaptureSelectedActorRelativeCameraValues(distance, height, sideOffset, yawOffset, pitch, lookHeight))
            {
                keyframe.distance = distance;
                keyframe.height = height;
                keyframe.sideOffset = sideOffset;
                keyframe.yawOffset = yawOffset;
                keyframe.pitch = pitch;
                keyframe.lookHeight = lookHeight;
            }

            selectedTrack->keyframes.push_back(keyframe);
            std::sort(selectedTrack->keyframes.begin(), selectedTrack->keyframes.end(),
                [](const CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& lhs, const CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& rhs)
                {
                    return lhs.frame < rhs.frame;
                });

            for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
            {
                if (selectedTrack->keyframes[i].frame == keyframe.frame)
                {
                    _selectedActorRelativeCameraKeyIndex = i;
                    break;
                }
            }

            _isPreviewDirty = true;
        }

        ImGui::SameLine();
        const bool isRemoveKeyDisabled =
            (_selectedActorRelativeCameraKeyIndex < 0 ||
                _selectedActorRelativeCameraKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()));
        if (isRemoveKeyDisabled)
            ImGui::BeginDisabled();

        if (ImGui::Button("Remove Relative Key"))
        {
            selectedTrack->keyframes.erase(selectedTrack->keyframes.begin() + _selectedActorRelativeCameraKeyIndex);
            _selectedActorRelativeCameraKeyIndex = std::clamp(
                _selectedActorRelativeCameraKeyIndex,
                -1,
                static_cast<int>(selectedTrack->keyframes.size()) - 1);
            _isPreviewDirty = true;
        }

        if (isRemoveKeyDisabled)
            ImGui::EndDisabled();

        if (selectedTrack->keyframes.empty())
        {
            ImGui::TextDisabled("No actor-relative keys yet.");
            return;
        }

        if (ImGui::BeginListBox("##ActorRelativeCameraKeyframeList", ImVec2(-FLT_MIN, 160.f)))
        {
            for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
            {
                const auto& keyframe = selectedTrack->keyframes[i];
                char label[160]{};
                sprintf_s(label, "Frame %d | Dist %.2f | Yaw %.1f", keyframe.frame, keyframe.distance, keyframe.yawOffset);

                const bool isSelected = (_selectedActorRelativeCameraKeyIndex == i);
                if (ImGui::Selectable(label, isSelected))
                {
                    _selectedActorRelativeCameraKeyIndex = i;
                    _currentFrame = keyframe.frame;
                    Enter_FreeCameraEditPreview();
                    _isPreviewDirty = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndListBox();
        }

        return;
    }

    if (Is_SelectedModifierTrack())
    {
        CameraTrack* selectedModifierTrack = Get_SelectedModifierTrack();
        if (selectedModifierTrack == nullptr)
        {
            ImGui::TextDisabled("No modifier track selected.");
            return;
        }

        Draw_SectionHeader("Modifier Keyframes");

        const bool isRemoveKeyDisabled =
            (_selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int>(selectedModifierTrack->keyframes.size()));
        if (isRemoveKeyDisabled)
            ImGui::BeginDisabled();

        if (ImGui::Button("Remove Key"))
        {
            selectedModifierTrack->keyframes.erase(selectedModifierTrack->keyframes.begin() + _selectedKeyIndex);
            _selectedKeyIndex = std::clamp(_selectedKeyIndex, -1, static_cast<int>(selectedModifierTrack->keyframes.size()) - 1);
            _isPreviewDirty = true;
        }

        if (isRemoveKeyDisabled)
            ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Sort Keys"))
        {
            Sort_SelectedModifierTrackKeyframes();
            _isPreviewDirty = true;
        }

        if (selectedModifierTrack->keyframes.empty())
        {
            ImGui::TextDisabled("No modifier keys yet. Right click the timeline row to insert one.");
            return;
        }

        if (ImGui::BeginListBox("##ModifierKeyframeList", ImVec2(-FLT_MIN, 160.f)))
        {
            for (int i = 0; i < static_cast<int>(selectedModifierTrack->keyframes.size()); ++i)
            {
                const CameraKeyframe& keyframe = selectedModifierTrack->keyframes[i];
                char label[128]{};
                sprintf_s(label, "Time %.3f | Value %.3f", keyframe.time, keyframe.value);

                const bool isSelected = (_selectedKeyIndex == i);
                if (ImGui::Selectable(label, isSelected))
                {
                    _selectedKeyIndex = i;
                    _currentFrame = std::clamp(
                        _sequence.startFrame + static_cast<int>(std::lround(keyframe.time * static_cast<float>(std::max(_sequence.fps, 1)))),
                        _sequence.startFrame,
                        _sequence.endFrame);
                    Enter_FreeCameraEditPreview();
                    _isPreviewDirty = true;
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndListBox();
        }

        return;
    }

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
    {
        ImGui::TextDisabled("No track selected.");
        return;
    }

    Draw_SectionHeader("Keyframes");

    if (ImGui::Button("Capture From FreeCamera"))
    {
        Capture_KeyframeFromFreeCamera();
        _isPreviewDirty = true;
    }

    ImGui::SameLine();
    const CINEMATIC_KEYFRAME* previousCameraKeyframe = nullptr;
    for (const auto& candidate : selectedTrack->keyframes)
    {
        if (candidate.frame >= _currentFrame)
            continue;

        if (previousCameraKeyframe == nullptr || candidate.frame > previousCameraKeyframe->frame)
            previousCameraKeyframe = &candidate;
    }

    if (previousCameraKeyframe == nullptr)
        ImGui::BeginDisabled();

    if (ImGui::Button("Duplicate Prev Key"))
    {
        CINEMATIC_KEYFRAME keyframe = *previousCameraKeyframe;
        keyframe.frame = _currentFrame;

        bool replaced = false;
        for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
        {
            if (selectedTrack->keyframes[i].frame != keyframe.frame)
                continue;

            selectedTrack->keyframes[i] = keyframe;
            _selectedKeyIndex = i;
            replaced = true;
            break;
        }

        if (!replaced)
        {
            selectedTrack->keyframes.push_back(keyframe);
            Sort_SelectedTrackKeyframes();

            for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
            {
                if (selectedTrack->keyframes[i].frame == keyframe.frame)
                {
                    _selectedKeyIndex = i;
                    break;
                }
            }
        }

        _isPreviewDirty = true;
    }

    if (previousCameraKeyframe == nullptr)
        ImGui::EndDisabled();

    ImGui::SameLine();
    const bool isRemoveKeyDisabled =
        (_selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()));
    if (isRemoveKeyDisabled)
        ImGui::BeginDisabled();

    if (ImGui::Button("Remove Key"))
    {
        selectedTrack->keyframes.erase(selectedTrack->keyframes.begin() + _selectedKeyIndex);
        _selectedKeyIndex = std::clamp(_selectedKeyIndex, -1, static_cast<int>(selectedTrack->keyframes.size()) - 1);
        _isPreviewDirty = true;
    }

    if (isRemoveKeyDisabled)
        ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Sort Keys"))
    {
        Sort_SelectedTrackKeyframes();
        _isPreviewDirty = true;
    }

    if (selectedTrack->keyframes.empty())
    {
        ImGui::TextDisabled("No keyframes yet. Capture one from the current FreeCamera pose.");
        return;
    }

    if (ImGui::BeginListBox("##CameraKeyframeList", ImVec2(-FLT_MIN, 160.f)))
    {
        for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
        {
            const CINEMATIC_KEYFRAME& keyframe = selectedTrack->keyframes[i];
            char label[128]{};
            sprintf_s(label, "Frame %d | Pos(%.1f, %.1f, %.1f)",
                keyframe.frame,
                keyframe.camPose.position.x,
                keyframe.camPose.position.y,
                keyframe.camPose.position.z);

            const bool isSelected = (_selectedKeyIndex == i);

            if (ImGui::Selectable(label, isSelected))
            {
                _selectedKeyIndex = i;
                _currentFrame = keyframe.frame;
                Enter_FreeCameraEditPreview();
                _isPreviewDirty = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndListBox();
    }
}

void Cinematic_Tool::Render_SelectedKeyframe()
{
    if (CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME* keyframe = Get_SelectedActorRelativeCameraKeyframe())
    {
        Draw_SectionHeader("Actor Relative Keyframe");
        Render_SelectedActorRelativeCameraKeyframeEditor(*keyframe);
        return;
    }

    if (CameraKeyframe* modifierKeyframe = Get_SelectedModifierKeyframe())
    {
        Draw_SectionHeader("Modifier Keyframe");
        Render_SelectedModifierKeyframeEditor(*modifierKeyframe);
        return;
    }

    if (CINEMATIC_PART_ROTATION_KEYFRAME* partKeyframe = Get_SelectedPartRotationKeyframe())
    {
        const bool shouldExpandEditor = _requestOpenSelectedKeyPopup;
        if (shouldExpandEditor)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            _requestOpenSelectedKeyPopup = false;
        }

        const bool isEditorOpen = ImGui::CollapsingHeader("Selected Part Rotation Keyframe", ImGuiTreeNodeFlags_None);
        if (!isEditorOpen)
            return;

        if (shouldExpandEditor)
            ImGui::SetScrollHereY();

        ImGui::TextDisabled("Track %d | Key %d", _selectedTrackIndex, _selectedPartRotationKeyIndex);
        ImGui::Separator();

        if (ImGui::DragInt("part rotation key frame", &partKeyframe->frame, 1.f, _sequence.startFrame, _sequence.endFrame))
        {
            partKeyframe->frame = std::clamp(partKeyframe->frame, _sequence.startFrame, _sequence.endFrame);
            _currentFrame = partKeyframe->frame;
            Request_CinematicPreviewRefresh(false);
        }

        if (ImGui::DragFloat3("part rotation", &partKeyframe->rotation.x, 0.1f))
            Request_CinematicPreviewRefresh(false);

        const char* interpolationLabel = magic_enum::enum_name(partKeyframe->interpolation).data();
        if (ImGui::BeginCombo("part rotation interpolation", interpolationLabel))
        {
            for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
            {
                const bool isSelected = partKeyframe->interpolation == mode;
                if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
                {
                    partKeyframe->interpolation = mode;
                    Request_CinematicPreviewRefresh(false);
                }

                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }

            ImGui::EndCombo();
        }

        Sort_AllPartRotationTrackKeyframes();
        return;
    }

    CINEMATIC_KEYFRAME* keyframe = Get_SelectedKeyframe();
    if (keyframe == nullptr)
    {
        ImGui::TextDisabled("No keyframe selected.");
        return;
    }

    Draw_SectionHeader("Keyframe");
    Render_SelectedKeyframeEditor(*keyframe);
}

void Cinematic_Tool::Render_SelectedKeyframeEditor(CINEMATIC_KEYFRAME& keyframe)
{
    const auto applyEditedKeyframe = [this, &keyframe]()
    {
        _currentFrame = keyframe.frame;

        const bool needsEditModeEntry =
            (CLIENT->Is_CinematicPlaying() && !CLIENT->Is_CinematicPaused()) ||
            !_isPreviewStoppedForEditing ||
            !GAME->Is_ActiveCameraType(ObjectType::FreeCamera);

        if (needsEditModeEntry)
            Enter_FreeCameraEditPreview(false);
        else
        {
            _isPreviewDirty = true;
            Apply_CurrentFramePose();
        }
    };

    if (ImGui::DragInt("frame", &keyframe.frame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        keyframe.frame = std::clamp(keyframe.frame, _sequence.startFrame, _sequence.endFrame);
        applyEditedKeyframe();
    }

    if(ImGui::DragFloat3("position", &keyframe.camPose.position.x, 0.1f))
        applyEditedKeyframe();

    if(ImGui::DragFloat3("rotation", &keyframe.camPose.rotation.x, 0.1f))
        applyEditedKeyframe();
    

    if(ImGui::DragFloat("fovY", &keyframe.camPose.fovY, 0.01f, 0.1f, 3.13f))
        applyEditedKeyframe();

    if (ImGui::DragFloat("near", &keyframe.camPose.nearPlane, 0.01f, 0.01f, keyframe.camPose.farPlane - 0.01f))
    {
        keyframe.camPose.nearPlane = std::clamp(keyframe.camPose.nearPlane, 0.01f, keyframe.camPose.farPlane - 0.01f);
        applyEditedKeyframe();
    }

    if (ImGui::DragFloat("far", &keyframe.camPose.farPlane, 0.1f, keyframe.camPose.nearPlane + 0.01f, 100000.f))
    {
        keyframe.camPose.farPlane = std::max(keyframe.camPose.farPlane, keyframe.camPose.nearPlane + 0.01f);
        applyEditedKeyframe();
    }

   // const char* interpolationLabels[] = { "Step", "Linear" };
   // int interpolationIndex = static_cast<int>(keyframe.interpolation);
   // if (ImGui::Combo("interpolation", &interpolationIndex, interpolationLabels, IM_ARRAYSIZE(interpolationLabels)))
   // {
   //     keyframe.interpolation = static_cast<InterpolationMode>(interpolationIndex);
   //     _isPreviewDirty = true;
   // }
   //

    const char* interpolationLabel = magic_enum::enum_name(keyframe.interpolation).data();
    
    if(ImGui::BeginCombo("Interpolation", interpolationLabel))
    {
        for(InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = (mode == keyframe.interpolation);

            if(ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
                keyframe.interpolation = mode;
                applyEditedKeyframe();
            }

            if(isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (ImGui::Button("Overwrite Keyframe With Current Camera"))
        _requestOpenOverwriteKeyframePopup = true;

    if (_requestOpenOverwriteKeyframePopup)
    {
        ImGui::OpenPopup("Confirm Overwrite Keyframe");
        _requestOpenOverwriteKeyframePopup = false;
    }

    if (ImGui::BeginPopupModal("Confirm Overwrite Keyframe", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Overwrite the selected keyframe with the current FreeCamera pose?");
        ImGui::Separator();

        if (ImGui::Button("Yes", ImVec2(120.f, 0.f)))
        {
            Shared<FreeCamera> freeCamera = Get_ActiveFreeCamera();
            CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();

            if (freeCamera != nullptr &&
                selectedTrack != nullptr &&
                _selectedKeyIndex >= 0 &&
                _selectedKeyIndex < static_cast<int>(selectedTrack->keyframes.size()))
            {
                auto& keyframe = selectedTrack->keyframes[_selectedKeyIndex];
                keyframe.camPose.farPlane = freeCamera->Get_Far();
                keyframe.camPose.nearPlane = freeCamera->Get_Near();
                keyframe.camPose.fovY = freeCamera->Get_FovY();
                keyframe.camPose.position = freeCamera->Get_Transform()->Get_WorldPosition();
                keyframe.camPose.rotation = freeCamera->Get_Transform()->Get_RotationEuler();

                _isPreviewDirty = true;
            }

            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

}

void Cinematic_Tool::Render_SelectedActorRelativeCameraKeyframeEditor(CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& keyframe)
{
    const auto applyEditedKeyframe = [this, &keyframe]()
    {
        _currentFrame = keyframe.frame;

        const bool needsEditModeEntry =
            (CLIENT->Is_CinematicPlaying() && !CLIENT->Is_CinematicPaused()) ||
            !_isPreviewStoppedForEditing ||
            !GAME->Is_ActiveCameraType(ObjectType::FreeCamera);

        if (needsEditModeEntry)
            Enter_FreeCameraEditPreview(false);
        else
        {
            _isPreviewDirty = true;
            Apply_CurrentFramePose();
        }
    };

    if (ImGui::DragInt("frame", &keyframe.frame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        keyframe.frame = std::clamp(keyframe.frame, _sequence.startFrame, _sequence.endFrame);
        applyEditedKeyframe();
    }

    if (ImGui::DragFloat("distance", &keyframe.distance, 0.01f, 0.01f, 1000.f))
    {
        keyframe.distance = std::max(keyframe.distance, 0.01f);
        applyEditedKeyframe();
    }
    if (ImGui::DragFloat("height", &keyframe.height, 0.01f))
        applyEditedKeyframe();
    if (ImGui::DragFloat("side offset", &keyframe.sideOffset, 0.01f))
        applyEditedKeyframe();
    if (ImGui::DragFloat("yaw offset", &keyframe.yawOffset, 0.05f, -360.f, 360.f))
        applyEditedKeyframe();
    if (ImGui::DragFloat("pitch", &keyframe.pitch, 0.05f, -89.f, 89.f))
        applyEditedKeyframe();
    if (ImGui::DragFloat("look height", &keyframe.lookHeight, 0.01f))
        applyEditedKeyframe();
    if (ImGui::DragFloat("fovY", &keyframe.fovY, 0.01f, 0.1f, 3.13f))
        applyEditedKeyframe();
    if (ImGui::DragFloat("near", &keyframe.nearPlane, 0.01f, 0.01f, keyframe.farPlane - 0.01f))
    {
        keyframe.nearPlane = std::clamp(keyframe.nearPlane, 0.01f, keyframe.farPlane - 0.01f);
        applyEditedKeyframe();
    }
    if (ImGui::DragFloat("far", &keyframe.farPlane, 0.1f, keyframe.nearPlane + 0.01f, 100000.f))
    {
        keyframe.farPlane = std::max(keyframe.farPlane, keyframe.nearPlane + 0.01f);
        applyEditedKeyframe();
    }

    const char* interpolationLabel = magic_enum::enum_name(keyframe.interpolation).data();
    if (ImGui::BeginCombo("Interpolation", interpolationLabel))
    {
        for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = (mode == keyframe.interpolation);
            if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
                keyframe.interpolation = mode;
                applyEditedKeyframe();
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }
}

void Cinematic_Tool::Render_SelectedModifierKeyframeEditor(CameraKeyframe& keyframe)
{
    const float maxTime = Evaluate_CinematicSequenceTime(_sequence, static_cast<float>(_sequence.endFrame));
    bool needsSort = false;
    bool previewChanged = false;

    if (ImGui::DragFloat("time", &keyframe.time, 0.01f, 0.f, maxTime))
    {
        keyframe.time = std::clamp(keyframe.time, 0.f, maxTime);
        needsSort = true;
        previewChanged = true;
    }

    if (ImGui::DragFloat("value", &keyframe.value, 0.01f))
        previewChanged = true;

    const char* interpolationLabel = magic_enum::enum_name(keyframe.interpolation).data();
    if (ImGui::BeginCombo("Interpolation", interpolationLabel))
    {
        for (InterpolationMode mode : magic_enum::enum_values<InterpolationMode>())
        {
            const bool isSelected = (mode == keyframe.interpolation);
            if (ImGui::Selectable(magic_enum::enum_name(mode).data(), isSelected))
            {
                keyframe.interpolation = mode;
                previewChanged = true;
            }

            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }

        ImGui::EndCombo();
    }

    if (needsSort)
    {
        Sort_SelectedModifierTrackKeyframes();
        if (CameraKeyframe* selectedModifierKeyframe = Get_SelectedModifierKeyframe())
        {
            _currentFrame = std::clamp(
                _sequence.startFrame + static_cast<int>(std::lround(selectedModifierKeyframe->time * static_cast<float>(std::max(_sequence.fps, 1)))),
                _sequence.startFrame,
                _sequence.endFrame);
        }
    }

    if (previewChanged)
        Request_CinematicPreviewRefresh(false);
}

void Cinematic_Tool::Render_SelectedKeyframePopup()
{
    if (CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME* relativeKeyframe = Get_SelectedActorRelativeCameraKeyframe())
    {
        const bool shouldExpandEditor = _requestOpenSelectedKeyPopup;
        if (shouldExpandEditor)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            _requestOpenSelectedKeyPopup = false;
        }

        const bool isEditorOpen = ImGui::CollapsingHeader("Selected Actor Relative Keyframe", ImGuiTreeNodeFlags_None);
        const bool enteredEditor = isEditorOpen && ImGui::IsItemToggledOpen();
        if (!isEditorOpen)
            return;

        if (shouldExpandEditor || enteredEditor)
            Enter_FreeCameraEditPreview();

        if (shouldExpandEditor)
            ImGui::SetScrollHereY();

        ImGui::TextDisabled("Track %d | Key %d", _selectedTrackIndex, _selectedActorRelativeCameraKeyIndex);
        ImGui::Separator();
        Render_SelectedActorRelativeCameraKeyframeEditor(*relativeKeyframe);
        return;
    }

    if (CameraKeyframe* modifierKeyframe = Get_SelectedModifierKeyframe())
    {
        const bool shouldExpandEditor = _requestOpenSelectedKeyPopup;
        if (shouldExpandEditor)
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_Always);
            _requestOpenSelectedKeyPopup = false;
        }

        const bool isEditorOpen = ImGui::CollapsingHeader("Selected Modifier Keyframe", ImGuiTreeNodeFlags_None);
        const bool enteredEditor = isEditorOpen && ImGui::IsItemToggledOpen();
        if (!isEditorOpen)
            return;

        if (shouldExpandEditor || enteredEditor)
            Enter_FreeCameraEditPreview();

        if (shouldExpandEditor)
            ImGui::SetScrollHereY();

        ImGui::TextDisabled("Track %d | Key %d", _selectedTrackIndex, _selectedKeyIndex);
        ImGui::Separator();
        Render_SelectedModifierKeyframeEditor(*modifierKeyframe);
        return;
    }

    CINEMATIC_KEYFRAME* keyframe = Get_SelectedKeyframe();

    if (keyframe == nullptr)
    {
        _requestOpenSelectedKeyPopup = false;
        return;
    }

    const bool shouldExpandEditor = _requestOpenSelectedKeyPopup;
    if (shouldExpandEditor)
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_Always);
        _requestOpenSelectedKeyPopup = false;
    }

    const bool isEditorOpen = ImGui::CollapsingHeader("Selected Keyframe", ImGuiTreeNodeFlags_None);
    const bool enteredEditor = isEditorOpen && ImGui::IsItemToggledOpen();
    if (!isEditorOpen)
        return;

    if (shouldExpandEditor || enteredEditor)
        Enter_FreeCameraEditPreview();

    if (shouldExpandEditor)
        ImGui::SetScrollHereY();

    ImGui::TextDisabled("Track %d | Key %d", _selectedTrackIndex, _selectedKeyIndex);
    ImGui::Separator();
    Render_SelectedKeyframeEditor(*keyframe);
}

void Cinematic_Tool::Render_InsertKeyframePopup()
{
    if (_requestOpenInsertKeyPopup)
    {
        ImGui::OpenPopup("Insert Keyframe");
        _requestOpenInsertKeyPopup = false;
    }

    if (!ImGui::BeginPopup("Insert Keyframe"))
    {
        if (_hasPendingInsertRequest && !ImGui::IsPopupOpen("Insert Keyframe"))
            Clear_PendingInsertRequest();
        return;
    }

    ImGui::TextDisabled("Track %d", _pendingInsertTrackIndex);
    ImGui::TextDisabled("Frame %d", _pendingInsertFrame);
    ImGui::Separator();

    const int cinematicTrackCount = static_cast<int>(_sequence.cinematicTracks.size());
    const int actorRelativeTrackStart = Get_ActorRelativeTrackStartIndex(_sequence);
    const int actorRelativeTrackEnd = actorRelativeTrackStart + static_cast<int>(_sequence.actorRelativeCameraTracks.size());
    const int modifierTrackStart = Get_ModifierTrackStartIndex(_sequence);
    const int modifierTrackEnd = modifierTrackStart + CinematicSequencerAdapter::kModifierTrackCount;
    const int actorTrackStart = Get_ActorTransformTrackStartIndex(_sequence);
    const int actorTrackEnd = actorTrackStart + static_cast<int>(_sequence.actorTransformTracks.size());
    const int partRotationTrackStart = Get_PartRotationTrackStartIndex(_sequence);
    const int partRotationTrackEnd = partRotationTrackStart + static_cast<int>(_sequence.partRotationTracks.size());

    const bool isActorRelativeInsert =
        _pendingInsertTrackIndex >= actorRelativeTrackStart &&
        _pendingInsertTrackIndex < actorRelativeTrackEnd;
    const bool isModifierInsert =
        _pendingInsertTrackIndex >= modifierTrackStart &&
        _pendingInsertTrackIndex < modifierTrackEnd;
    const bool isActorTransformInsert =
        _pendingInsertTrackIndex >= actorTrackStart &&
        _pendingInsertTrackIndex < actorTrackEnd;
    const bool isPartRotationInsert =
        _pendingInsertTrackIndex >= partRotationTrackStart &&
        _pendingInsertTrackIndex < partRotationTrackEnd;
    const bool isCameraTrackInsert =
        _pendingInsertTrackIndex >= 0 &&
        _pendingInsertTrackIndex < cinematicTrackCount;

    if (isCameraTrackInsert && !_hasPendingInsertPose)
        ImGui::TextDisabled("Active FreeCamera is required to insert from the current camera pose.");

    const bool canInsertFromCamera =
        _hasPendingInsertRequest &&
        (isModifierInsert || isActorTransformInsert || isPartRotationInsert || isActorRelativeInsert ||
            (isCameraTrackInsert && _hasPendingInsertPose)) &&
        _pendingInsertTrackIndex >= 0;

    if (!canInsertFromCamera)
        ImGui::BeginDisabled();

    if (ImGui::Button("Insert Keyframe Here"))
    {
        Insert_KeyframeAtFrame(_pendingInsertTrackIndex, _pendingInsertFrame);
        Clear_PendingInsertRequest();
        ImGui::CloseCurrentPopup();
    }

    if (!canInsertFromCamera)
        ImGui::EndDisabled();

    if (ImGui::Button("Cancel"))
    {
        Clear_PendingInsertRequest();
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void Cinematic_Tool::Render_RemoveKeyframePopup()
{
    if(_requestRemoveKeyPopup)
    {
        ImGui::OpenPopup("Remove Keyframe");
        _requestRemoveKeyPopup = false;
    }

    if(!ImGui::BeginPopup("Remove Keyframe"))
        return;

    if (CameraKeyframe* modifierKeyframe = Get_SelectedModifierKeyframe())
    {
        ImGui::TextDisabled("Modifier Track %d | Key %d", _selectedTrackIndex, _selectedKeyIndex);
        ImGui::Separator();
        Render_SelectedModifierKeyframeEditor(*modifierKeyframe);
        ImGui::Separator();

        if (ImGui::Button("Remove Keyframe"))
        {
            if (CameraTrack* selectedModifierTrack = Get_SelectedModifierTrack())
            {
                selectedModifierTrack->keyframes.erase(selectedModifierTrack->keyframes.begin() + _selectedKeyIndex);
                _selectedKeyIndex = std::clamp(_selectedKeyIndex, -1, static_cast<int>(selectedModifierTrack->keyframes.size()) - 1);
                _isPreviewDirty = true;
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Close"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
        return;
    }

    if (CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME* relativeKeyframe = Get_SelectedActorRelativeCameraKeyframe())
    {
        UNREFERENCED_PARAMETER(relativeKeyframe);

        if (ImGui::Button("Remove Keyframe Here"))
        {
            if (CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* selectedTrack = Get_SelectedActorRelativeCameraTrack())
            {
                selectedTrack->keyframes.erase(selectedTrack->keyframes.begin() + _selectedActorRelativeCameraKeyIndex);
                _selectedActorRelativeCameraKeyIndex = std::clamp(
                    _selectedActorRelativeCameraKeyIndex,
                    -1,
                    static_cast<int>(selectedTrack->keyframes.size()) - 1);
                _isPreviewDirty = true;
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
        return;
    }

    if (CINEMATIC_PART_ROTATION_KEYFRAME* partKeyframe = Get_SelectedPartRotationKeyframe())
    {
        UNREFERENCED_PARAMETER(partKeyframe);

        if (ImGui::Button("Remove Keyframe Here"))
        {
            if (CINEMATIC_PART_ROTATION_TRACK* selectedTrack = Get_SelectedPartRotationTrack())
            {
                selectedTrack->keyframes.erase(selectedTrack->keyframes.begin() + _selectedPartRotationKeyIndex);
                _selectedPartRotationKeyIndex = std::clamp(
                    _selectedPartRotationKeyIndex,
                    -1,
                    static_cast<int>(selectedTrack->keyframes.size()) - 1);
                _isPreviewDirty = true;
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Cancel"))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
        return;
    }

    if (ImGui::Button("Remove Keyframe Here"))
    {
        CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();

        selectedTrack->keyframes.erase(selectedTrack->keyframes.begin()+_selectedKeyIndex);
        _selectedKeyIndex = std::clamp(_selectedKeyIndex, -1, static_cast<int>(selectedTrack->keyframes.size()));
        _isPreviewDirty = true;

        ImGui::CloseCurrentPopup();
    }

    if (ImGui::Button("Cancel"))
        ImGui::CloseCurrentPopup();

    ImGui::EndPopup();
}

void Cinematic_Tool::Step_PreviewFrame(int frameDelta)
{
    if (frameDelta == 0)
        return;

    const bool isPlaying = CLIENT->Is_CinematicPlaying();
    const bool isPaused = CLIENT->Is_CinematicPaused();
    if (isPlaying && !isPaused)
        CLIENT->Pause_Sequence();

    _currentFrame = std::clamp(_currentFrame + frameDelta, _sequence.startFrame, _sequence.endFrame);
    _currentFrameExact = static_cast<float>(_currentFrame);
    _useExactFramePreview = false;
    Request_CinematicPreviewRefresh(false);
}

void Cinematic_Tool::Step_SelectedTrackKeyframe(int direction)
{
    if (direction == 0)
        return;

    int targetFrame = -1;
    auto syncCurrentFrame = [&]()
    {
        _currentFrame = targetFrame;
        _currentFrameExact = static_cast<float>(_currentFrame);
        _useExactFramePreview = false;
        Request_CinematicPreviewRefresh(false);
    };

    if (CINEMATIC_TRACK* track = Get_SelectedTrack())
    {
        int targetKeyIndex = -1;
        for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
        {
            const int frame = track->keyframes[i].frame;
            if (direction > 0)
            {
                if (frame <= _currentFrame)
                    continue;
                if (targetFrame < 0 || frame < targetFrame)
                {
                    targetFrame = frame;
                    targetKeyIndex = i;
                }
            }
            else
            {
                if (frame >= _currentFrame)
                    continue;
                if (targetFrame < 0 || frame > targetFrame)
                {
                    targetFrame = frame;
                    targetKeyIndex = i;
                }
            }
        }

        if (targetKeyIndex >= 0)
        {
            _selectedKeyIndex = targetKeyIndex;
            syncCurrentFrame();
        }
        return;
    }

    if (CameraTrack* modifierTrack = Get_SelectedModifierTrack())
    {
        int targetKeyIndex = -1;
        for (int i = 0; i < static_cast<int>(modifierTrack->keyframes.size()); ++i)
        {
            const int frame = std::clamp(
                _sequence.startFrame + static_cast<int>(std::lround(
                    modifierTrack->keyframes[i].time * static_cast<float>(std::max(_sequence.fps, 1)))),
                _sequence.startFrame,
                _sequence.endFrame);
            if (direction > 0)
            {
                if (frame <= _currentFrame)
                    continue;
                if (targetFrame < 0 || frame < targetFrame)
                {
                    targetFrame = frame;
                    targetKeyIndex = i;
                }
            }
            else
            {
                if (frame >= _currentFrame)
                    continue;
                if (targetFrame < 0 || frame > targetFrame)
                {
                    targetFrame = frame;
                    targetKeyIndex = i;
                }
            }
        }

        if (targetKeyIndex >= 0)
        {
            _selectedKeyIndex = targetKeyIndex;
            syncCurrentFrame();
        }
        return;
    }

    if (CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* track = Get_SelectedActorRelativeCameraTrack())
    {
        int targetKeyIndex = -1;
        for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
        {
            const int frame = track->keyframes[i].frame;
            if (direction > 0)
            {
                if (frame <= _currentFrame)
                    continue;
                if (targetFrame < 0 || frame < targetFrame)
                {
                    targetFrame = frame;
                    targetKeyIndex = i;
                }
            }
            else
            {
                if (frame >= _currentFrame)
                    continue;
                if (targetFrame < 0 || frame > targetFrame)
                {
                    targetFrame = frame;
                    targetKeyIndex = i;
                }
            }
        }

        if (targetKeyIndex >= 0)
        {
            _selectedActorRelativeCameraKeyIndex = targetKeyIndex;
            syncCurrentFrame();
        }
        return;
    }

    if (Is_SelectedPartRotationTrack())
    {
        if (CINEMATIC_PART_ROTATION_TRACK* track = Get_SelectedPartRotationTrack())
        {
            int targetKeyIndex = -1;
            for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
            {
                const int frame = track->keyframes[i].frame;
                if (direction > 0)
                {
                    if (frame <= _currentFrame)
                        continue;
                    if (targetFrame < 0 || frame < targetFrame)
                    {
                        targetFrame = frame;
                        targetKeyIndex = i;
                    }
                }
                else
                {
                    if (frame >= _currentFrame)
                        continue;
                    if (targetFrame < 0 || frame > targetFrame)
                    {
                        targetFrame = frame;
                        targetKeyIndex = i;
                    }
                }
            }

            if (targetKeyIndex >= 0)
            {
                _selectedPartRotationKeyIndex = targetKeyIndex;
                syncCurrentFrame();
            }
        }
        return;
    }

    if (_selectedActorTransformTrackIndex >= 0 &&
        _selectedActorTransformTrackIndex < static_cast<int>(_sequence.actorTransformTracks.size()))
    {
        const auto& track = _sequence.actorTransformTracks[_selectedActorTransformTrackIndex];
        int targetKeyIndex = -1;
        for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
        {
            const int frame = track.keyframes[i].frame;
            if (direction > 0)
            {
                if (frame <= _currentFrame)
                    continue;
                if (targetFrame < 0 || frame < targetFrame)
                {
                    targetFrame = frame;
                    targetKeyIndex = i;
                }
            }
            else
            {
                if (frame >= _currentFrame)
                    continue;
                if (targetFrame < 0 || frame > targetFrame)
                {
                    targetFrame = frame;
                    targetKeyIndex = i;
                }
            }
        }

        if (targetKeyIndex >= 0)
        {
            _selectedActorTransformKeyIndex = targetKeyIndex;
            syncCurrentFrame();
        }
        return;
    }
}

void Cinematic_Tool::Render_PreviewState()
{
    Draw_SectionHeader("Preview");

    const bool isPlaying = CLIENT->Is_CinematicPlaying();
    const bool isPaused = CLIENT->Is_CinematicPaused();

    const bool cameraTrackChanged = ImGui::Checkbox("Camera", &_previewApplyCameraTrack);
    ImGui::SameLine();
    const bool eventTrackChanged = ImGui::Checkbox("Events", &_previewApplyEventTrack);
    ImGui::SameLine();
    const bool actorTrackChanged = ImGui::Checkbox("Actor Moves", &_previewApplyActorTransformTrack);

    if (!_previewApplyCameraTrack)
        ImGui::TextDisabled("Camera OFF: FreeCamera stays fixed while the timeline previews events and actor movement.");

    if (cameraTrackChanged)
    {
        if (isPlaying && !isPaused)
            CLIENT->Pause_Sequence();

        Enter_FreeCameraEditPreview();
    }
    else if (eventTrackChanged || actorTrackChanged)
    {
        if (isPlaying && !isPaused)
            CLIENT->Pause_Sequence();

        Request_CinematicPreviewRefresh(true);
    }

    ImGui::SetNextItemWidth(120.f);
    if (ImGui::DragInt("current frame", &_currentFrame, 1.f, _sequence.startFrame, _sequence.endFrame))
    {
        _currentFrame = std::clamp(_currentFrame, _sequence.startFrame, _sequence.endFrame);
        _currentFrameExact = static_cast<float>(_currentFrame);
        _useExactFramePreview = false;
        Request_CinematicPreviewRefresh(false);
    }

    ImGui::SetNextItemWidth(140.f);
    if (ImGui::DragFloat("current frame exact", &_currentFrameExact, 0.05f, static_cast<float>(_sequence.startFrame), static_cast<float>(_sequence.endFrame)))
    {
        _currentFrameExact = std::clamp(
            _currentFrameExact,
            static_cast<float>(_sequence.startFrame),
            static_cast<float>(_sequence.endFrame));
        _currentFrame = std::clamp(
            static_cast<int>(std::floor(_currentFrameExact)),
            _sequence.startFrame,
            _sequence.endFrame);
        _useExactFramePreview = true;
        Request_CinematicPreviewRefresh(false);
    }

    const char* previewButtonLabel = "Play Preview";
    if (isPlaying)
        previewButtonLabel = isPaused
            ? (_isPreviewStoppedForEditing ? "Play Preview" : "Resume Preview")
            : "Pause Preview";

    if (ImGui::Button(previewButtonLabel))
        Toggle_PreviewPlayback();

    if (isPlaying)
    {
        ImGui::SameLine();
        if (ImGui::Button("Stop Preview"))
        {
            if (!isPaused)
                CLIENT->Pause_Sequence();

            Enter_FreeCameraEditPreview();
        }

        ImGui::SameLine();
        if (ImGui::Button("End Preview"))
        {
            _isPreviewStoppedForEditing = false;
            EDITOR->Set_CinematicPreviewFreeCameraOverride(false);
            GAME->Set_PreviewLetterboxEnabled(false);
            CLIENT->Stop_Sequence(true);
        }
    }

    ImGui::SameLine();
    if (!isPlaying)
        ImGui::TextDisabled("Stopped");
    else if (isPaused)
        ImGui::TextDisabled(_isPreviewStoppedForEditing ? "Stopped" : "Paused");
    else
        ImGui::TextDisabled("Playing");

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    CINEMATIC_KEYFRAME* leftKey = nullptr;
    CINEMATIC_KEYFRAME* rightKey = nullptr;

    if (selectedTrack != nullptr)
        Find_SurroundingKeyFrames(selectedTrack, static_cast<float>(_currentFrame), leftKey, rightKey);

    const int keyCount = selectedTrack ? static_cast<int>(selectedTrack->keyframes.size()) : 0;

    ImGui::TextDisabled("Selected Track : %d", _selectedTrackIndex);
    ImGui::TextDisabled("Selected Key : %d", _selectedKeyIndex);
    ImGui::TextDisabled("Keys In Track : %d", keyCount);
    ImGui::TextDisabled("Left Frame : %d", leftKey ? leftKey->frame : -1);
    ImGui::TextDisabled("Right Frame : %d", rightKey ? rightKey->frame : -1);
    ImGui::TextDisabled("Current Playing Frame : %d", CLIENT->Get_CurrentSequenceFrame());

    if(leftKey&&rightKey)
    {
        if(leftKey->frame == rightKey->frame)
            return;
        float t = static_cast<float>(_currentFrame - leftKey->frame) / static_cast<float>(rightKey->frame - leftKey->frame);
        ImGui::TextDisabled("t : %f", t);
    }


}

void Cinematic_Tool::Ensure_DefaultTrack()
{
    if (_sequence.cinematicTracks.empty() && _sequence.actorRelativeCameraTracks.empty())
        _sequence.cinematicTracks.push_back(CINEMATIC_TRACK{});
}

void Cinematic_Tool::Clamp_Selection()
{
    const int cinematicTrackCount = static_cast<int>(_sequence.cinematicTracks.size());
    const int actorRelativeTrackCount = static_cast<int>(_sequence.actorRelativeCameraTracks.size());
    const int cameraShotCount = static_cast<int>(_sequence.cameraShots.size());
    const int eventCount = static_cast<int>(_sequence.eventTrack.size());
    const int actorTransformTrackCount = static_cast<int>(_sequence.actorTransformTracks.size());
    const int partRotationTrackCount = static_cast<int>(_sequence.partRotationTracks.size());
    const int totalTrackCount =
        cinematicTrackCount +
        actorRelativeTrackCount +
        CinematicSequencerAdapter::kModifierTrackCount +
        CinematicSequencerAdapter::kEventTrackCount +
        actorTransformTrackCount +
        partRotationTrackCount;

    if (totalTrackCount == 0)
    {
        _selectedTrackIndex = 0;
        _selectedKeyIndex = -1;
        _selectedEventIndex = -1;
        _selectedCameraShotIndex = -1;
        _selectedActorRelativeCameraTrackIndex = -1;
        _selectedActorRelativeCameraKeyIndex = -1;
        _selectedActorTransformTrackIndex = -1;
        _selectedActorTransformKeyIndex = -1;
        _selectedPartRotationTrackIndex = -1;
        _selectedPartRotationKeyIndex = -1;
        return;
    }

    _selectedTrackIndex = std::clamp(_selectedTrackIndex, 0, std::max(totalTrackCount - 1, 0));
    _selectedCameraShotIndex = std::clamp(_selectedCameraShotIndex, -1, cameraShotCount - 1);
    _selectedEventIndex = std::clamp(_selectedEventIndex, -1, eventCount - 1);
    if (Is_SelectedActorRelativeCameraTrack())
        _selectedActorRelativeCameraTrackIndex = Get_SelectedActorRelativeCameraTrackIndex();
    else
        _selectedActorRelativeCameraTrackIndex = std::clamp(_selectedActorRelativeCameraTrackIndex, -1, actorRelativeTrackCount - 1);

    const int actorTransformTrackStart = Get_ActorTransformTrackStartIndex(_sequence);
    if (_selectedTrackIndex >= actorTransformTrackStart &&
        _selectedTrackIndex < actorTransformTrackStart + actorTransformTrackCount)
    {
        _selectedActorTransformTrackIndex = _selectedTrackIndex - actorTransformTrackStart;
    }
    else
    {
        _selectedActorTransformTrackIndex = std::clamp(_selectedActorTransformTrackIndex, -1, actorTransformTrackCount - 1);
    }

    const int partRotationTrackStart = Get_PartRotationTrackStartIndex(_sequence);
    if (_selectedTrackIndex >= partRotationTrackStart &&
        _selectedTrackIndex < partRotationTrackStart + partRotationTrackCount)
    {
        _selectedPartRotationTrackIndex = _selectedTrackIndex - partRotationTrackStart;
    }
    else
    {
        _selectedPartRotationTrackIndex = std::clamp(_selectedPartRotationTrackIndex, -1, partRotationTrackCount - 1);
    }

    if (_selectedActorRelativeCameraTrackIndex >= 0 &&
        _selectedActorRelativeCameraTrackIndex < actorRelativeTrackCount)
    {
        const auto& relativeTrack = _sequence.actorRelativeCameraTracks[_selectedActorRelativeCameraTrackIndex];
        _selectedActorRelativeCameraKeyIndex = std::clamp(
            _selectedActorRelativeCameraKeyIndex,
            -1,
            static_cast<int>(relativeTrack.keyframes.size()) - 1);
    }
    else
    {
        _selectedActorRelativeCameraKeyIndex = -1;
    }

    if (_selectedActorTransformTrackIndex >= 0 &&
        _selectedActorTransformTrackIndex < actorTransformTrackCount)
    {
        const auto& actorTrack = _sequence.actorTransformTracks[_selectedActorTransformTrackIndex];
        _selectedActorTransformKeyIndex = std::clamp(
            _selectedActorTransformKeyIndex,
            -1,
            static_cast<int>(actorTrack.keyframes.size()) - 1);
    }
    else
    {
        _selectedActorTransformKeyIndex = -1;
    }

    if (_selectedPartRotationTrackIndex >= 0 &&
        _selectedPartRotationTrackIndex < partRotationTrackCount)
    {
        const auto& partTrack = _sequence.partRotationTracks[_selectedPartRotationTrackIndex];
        _selectedPartRotationKeyIndex = std::clamp(
            _selectedPartRotationKeyIndex,
            -1,
            static_cast<int>(partTrack.keyframes.size()) - 1);
    }
    else
    {
        _selectedPartRotationKeyIndex = -1;
    }

    if (Is_SelectedModifierTrack())
    {
        CameraTrack* selectedModifierTrack = Get_SelectedModifierTrack();
        if (selectedModifierTrack == nullptr || selectedModifierTrack->keyframes.empty())
        {
            _selectedKeyIndex = -1;
            return;
        }

        _selectedKeyIndex = std::clamp(_selectedKeyIndex, 0, static_cast<int>(selectedModifierTrack->keyframes.size()) - 1);
        return;
    }

    if (Is_SelectedActorRelativeCameraTrack())
    {
        if (CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* selectedActorRelativeTrack = Get_SelectedActorRelativeCameraTrack())
        {
            if (selectedActorRelativeTrack->keyframes.empty())
            {
                _selectedActorRelativeCameraKeyIndex = -1;
                return;
            }

            _selectedActorRelativeCameraKeyIndex = std::clamp(
                _selectedActorRelativeCameraKeyIndex,
                0,
                static_cast<int>(selectedActorRelativeTrack->keyframes.size()) - 1);
        }
        else
        {
            _selectedActorRelativeCameraKeyIndex = -1;
        }

        _selectedKeyIndex = -1;
        return;
    }

    if (Is_SelectedPartRotationTrack())
    {
        if (CINEMATIC_PART_ROTATION_TRACK* selectedPartTrack = Get_SelectedPartRotationTrack())
        {
            if (selectedPartTrack->keyframes.empty())
            {
                _selectedPartRotationKeyIndex = -1;
                return;
            }

            _selectedPartRotationKeyIndex = std::clamp(
                _selectedPartRotationKeyIndex,
                0,
                static_cast<int>(selectedPartTrack->keyframes.size()) - 1);
        }
        else
        {
            _selectedPartRotationKeyIndex = -1;
        }

        _selectedKeyIndex = -1;
        return;
    }

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr || selectedTrack->keyframes.empty())
    {
        _selectedKeyIndex = -1;
        return;
    }

    _selectedKeyIndex = std::clamp(_selectedKeyIndex, 0, static_cast<int>(selectedTrack->keyframes.size()) - 1);
}

void Cinematic_Tool::Sort_SelectedTrackKeyframes()
{
    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
        return;

    const int selectedFrame = (_selectedKeyIndex >= 0 &&
        _selectedKeyIndex < static_cast<int>(selectedTrack->keyframes.size()))
        ? selectedTrack->keyframes[_selectedKeyIndex].frame
        : -1;

    std::sort(selectedTrack->keyframes.begin(), selectedTrack->keyframes.end(),
        [](const CINEMATIC_KEYFRAME& lhs, const CINEMATIC_KEYFRAME& rhs)
        {
            return lhs.frame < rhs.frame;
        });

    if (selectedTrack->keyframes.empty())
    {
        _selectedKeyIndex = -1;
        return;
    }

    if (selectedFrame < 0)
    {
        _selectedKeyIndex = 0;
        return;
    }

    for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
    {
        if (selectedTrack->keyframes[i].frame == selectedFrame)
        {
            _selectedKeyIndex = i;
            return;
        }
    }

    _selectedKeyIndex = 0;
}

void Cinematic_Tool::Sort_BattleClipTrackKeyframes(CameraTrack& track)
{
    std::sort(track.keyframes.begin(), track.keyframes.end(),
        [](const CameraKeyframe& lhs, const CameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });
}

void Cinematic_Tool::Insert_BattleClipKeyAtFrame(int trackIndex, int frame)
{
    CameraTrack* track = nullptr;

    switch (trackIndex)
    {
    case 0:
        track = &_editingBattleClip.distanceOffset;
        break;
    case 1:
        track = &_editingBattleClip.fovOffset;
        break;
    case 2:
        track = &_editingBattleClip.shakeAmplitude;
        break;
    case 3:
        track = &_editingBattleClip.sideOffset;
        break;
    case 4:
        track = &_editingBattleClip.pivotOffset;
        break;
    case 5:
        track = &_editingBattleClip.heightOffset;
        break;
    case 6:
        track = &_editingBattleClip.aimOffset;
        break;
    case 7:
        track = &_editingBattleClip.lookHeightOffset;
        break;
    case 8:
        track = &_editingBattleClip.yawOffset;
        break;
    default:
        return;
    }

    const float keyTime = std::clamp(static_cast<float>(frame) / 30.f, 0.f, _editingBattleClip.duration);

    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        if (fabsf(track->keyframes[i].time - keyTime) <= 0.0001f)
        {
            _battleClipSelectedTrackIndex = trackIndex;
            _battleClipSelectedKeyIndex = i;
            _battleClipCurrentFrame = frame;
            return;
        }
    }

    CameraKeyframe keyframe {};
    keyframe.time = keyTime;
    track->keyframes.push_back(keyframe);
    Sort_BattleClipTrackKeyframes(*track);

    _battleClipSelectedTrackIndex = trackIndex;
    _battleClipCurrentFrame = frame;

    for (int i = 0; i < static_cast<int>(track->keyframes.size()); ++i)
    {
        if (fabsf(track->keyframes[i].time - keyTime) <= 0.0001f)
        {
            _battleClipSelectedKeyIndex = i;
            break;
        }
    }
}

void Cinematic_Tool::Capture_KeyframeFromFreeCamera()
{
    CAMERAPOSE camPose{};
    if (!Try_CaptureActiveFreeCameraPose(camPose))
        return;

    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
        return;

    CINEMATIC_KEYFRAME keyframe{};
    keyframe.frame = _currentFrame;
    keyframe.camPose = camPose;

    selectedTrack->keyframes.push_back(keyframe);
    Sort_SelectedTrackKeyframes();

    for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
    {
        if (selectedTrack->keyframes[i].frame == _currentFrame)
            _selectedKeyIndex = i;
    }

    _isPreviewDirty = true;
}

void Cinematic_Tool::Sort_SelectedModifierTrackKeyframes()
{
    CameraTrack* selectedTrack = Get_SelectedModifierTrack();
    if (selectedTrack == nullptr)
        return;

    const float selectedTime = (_selectedKeyIndex >= 0 &&
        _selectedKeyIndex < static_cast<int>(selectedTrack->keyframes.size()))
        ? selectedTrack->keyframes[_selectedKeyIndex].time
        : -1.f;

    std::sort(selectedTrack->keyframes.begin(), selectedTrack->keyframes.end(),
        [](const CameraKeyframe& lhs, const CameraKeyframe& rhs)
        {
            return lhs.time < rhs.time;
        });

    if (selectedTrack->keyframes.empty())
    {
        _selectedKeyIndex = -1;
        return;
    }

    if (selectedTime < 0.f)
    {
        _selectedKeyIndex = 0;
        return;
    }

    for (int i = 0; i < static_cast<int>(selectedTrack->keyframes.size()); ++i)
    {
        if (fabsf(selectedTrack->keyframes[i].time - selectedTime) <= 0.0001f)
        {
            _selectedKeyIndex = i;
            return;
        }
    }

    _selectedKeyIndex = 0;
}

void Cinematic_Tool::Insert_KeyframeAtFrame(int trackIndex, int frame)
{
    const int cinematicTrackCount = static_cast<int>(_sequence.cinematicTracks.size());
    const int actorRelativeTrackStart = Get_ActorRelativeTrackStartIndex(_sequence);
    const int actorRelativeTrackEnd = actorRelativeTrackStart + static_cast<int>(_sequence.actorRelativeCameraTracks.size());
    const int modifierTrackStart = Get_ModifierTrackStartIndex(_sequence);
    const int modifierTrackEnd = modifierTrackStart + CinematicSequencerAdapter::kModifierTrackCount;
    const int actorTrackStart = Get_ActorTransformTrackStartIndex(_sequence);
    const int actorTrackEnd = actorTrackStart + static_cast<int>(_sequence.actorTransformTracks.size());
    const int partRotationTrackStart = Get_PartRotationTrackStartIndex(_sequence);
    const int partRotationTrackEnd = partRotationTrackStart + static_cast<int>(_sequence.partRotationTracks.size());

    if (trackIndex >= actorRelativeTrackStart && trackIndex < actorRelativeTrackEnd)
    {
        const int actorRelativeTrackIndex = trackIndex - actorRelativeTrackStart;
        auto& actorRelativeTrack = _sequence.actorRelativeCameraTracks[actorRelativeTrackIndex];
        frame = std::clamp(frame, _sequence.startFrame, _sequence.endFrame);

        for (int i = 0; i < static_cast<int>(actorRelativeTrack.keyframes.size()); ++i)
        {
            if (actorRelativeTrack.keyframes[i].frame == frame)
            {
                _selectedTrackIndex = trackIndex;
                _selectedKeyIndex = -1;
                _selectedActorRelativeCameraTrackIndex = actorRelativeTrackIndex;
                _selectedActorRelativeCameraKeyIndex = i;
                _currentFrame = frame;
                _isPreviewDirty = true;
                return;
            }
        }

        CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME keyframe{};
        keyframe.frame = frame;
        Evaluate_ActorRelativeCameraTrackPose(actorRelativeTrack, frame, keyframe);

        actorRelativeTrack.keyframes.push_back(keyframe);
        std::sort(actorRelativeTrack.keyframes.begin(), actorRelativeTrack.keyframes.end(),
            [](const CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& lhs, const CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        _selectedTrackIndex = trackIndex;
        _selectedKeyIndex = -1;
        _selectedActorRelativeCameraTrackIndex = actorRelativeTrackIndex;
        _currentFrame = frame;

        for (int i = 0; i < static_cast<int>(actorRelativeTrack.keyframes.size()); ++i)
        {
            if (actorRelativeTrack.keyframes[i].frame == frame)
            {
                _selectedActorRelativeCameraKeyIndex = i;
                break;
            }
        }

        _requestOpenSelectedKeyPopup = true;
        _isPreviewDirty = true;
        return;
    }

    if (trackIndex >= actorTrackStart && trackIndex < actorTrackEnd)
    {
        const int actorTrackIndex = trackIndex - actorTrackStart;
        auto& actorTrack = _sequence.actorTransformTracks[actorTrackIndex];
        frame = std::clamp(frame, _sequence.startFrame, _sequence.endFrame);

        for (int i = 0; i < static_cast<int>(actorTrack.keyframes.size()); ++i)
        {
            if (actorTrack.keyframes[i].frame == frame)
            {
                _selectedTrackIndex = trackIndex;
                _selectedKeyIndex = -1;
                _selectedActorTransformTrackIndex = actorTrackIndex;
                _selectedActorTransformKeyIndex = i;
                _currentFrame = frame;
                _isPreviewDirty = true;
                return;
            }
        }

        CINEMATIC_ACTOR_TRANSFORM_KEYFRAME keyframe{};
        keyframe.frame = frame;
        Evaluate_ActorTransformTrackPose(actorTrack, frame, keyframe.position, keyframe.rotation);

        actorTrack.keyframes.push_back(keyframe);
        std::sort(actorTrack.keyframes.begin(), actorTrack.keyframes.end(),
            [](const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& lhs, const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        _selectedTrackIndex = trackIndex;
        _selectedKeyIndex = -1;
        _selectedActorTransformTrackIndex = actorTrackIndex;
        _currentFrame = frame;

        for (int i = 0; i < static_cast<int>(actorTrack.keyframes.size()); ++i)
        {
            if (actorTrack.keyframes[i].frame == frame)
            {
                _selectedActorTransformKeyIndex = i;
                break;
            }
        }

        _isPreviewDirty = true;
        return;
    }

    if (trackIndex >= partRotationTrackStart && trackIndex < partRotationTrackEnd)
    {
        const int partTrackIndex = trackIndex - partRotationTrackStart;
        auto& partTrack = _sequence.partRotationTracks[partTrackIndex];
        frame = std::clamp(frame, _sequence.startFrame, _sequence.endFrame);

        for (int i = 0; i < static_cast<int>(partTrack.keyframes.size()); ++i)
        {
            if (partTrack.keyframes[i].frame == frame)
            {
                _selectedTrackIndex = trackIndex;
                _selectedKeyIndex = -1;
                _selectedPartRotationTrackIndex = partTrackIndex;
                _selectedPartRotationKeyIndex = i;
                _currentFrame = frame;
                _isPreviewDirty = true;
                return;
            }
        }

        CINEMATIC_PART_ROTATION_KEYFRAME keyframe{};
        keyframe.frame = frame;
        Evaluate_PartRotationTrackPose(partTrack, frame, keyframe.rotation);

        partTrack.keyframes.push_back(keyframe);
        std::sort(partTrack.keyframes.begin(), partTrack.keyframes.end(),
            [](const CINEMATIC_PART_ROTATION_KEYFRAME& lhs, const CINEMATIC_PART_ROTATION_KEYFRAME& rhs)
            {
                return lhs.frame < rhs.frame;
            });

        _selectedTrackIndex = trackIndex;
        _selectedKeyIndex = -1;
        _selectedPartRotationTrackIndex = partTrackIndex;
        _currentFrame = frame;

        for (int i = 0; i < static_cast<int>(partTrack.keyframes.size()); ++i)
        {
            if (partTrack.keyframes[i].frame == frame)
            {
                _selectedPartRotationKeyIndex = i;
                break;
            }
        }

        _isPreviewDirty = true;
        return;
    }

    if (trackIndex >= modifierTrackStart && trackIndex < modifierTrackEnd)
    {
        CameraTrack* modifierTrack = nullptr;

        const int modifierTrackIndex = trackIndex - modifierTrackStart;
        switch (modifierTrackIndex)
        {
        case 0: modifierTrack = &_sequence.modifierClip.shakeAmplitude; break;
        case 1: modifierTrack = &_sequence.modifierClip.shakeFrequencyX; break;
        case 2: modifierTrack = &_sequence.modifierClip.shakeFrequencyY; break;
        case 3: modifierTrack = &_sequence.modifierClip.shakeDecayRateX; break;
        case 4: modifierTrack = &_sequence.modifierClip.shakeDecayRate; break;
        case 5: modifierTrack = &_sequence.modifierClip.shakeVerticalRatio; break;
        case 6: modifierTrack = &_sequence.modifierClip.shakeStrengthScale; break;
        default: return;
        }

        const float keyTime = std::max(static_cast<float>(frame - _sequence.startFrame) / static_cast<float>(std::max(_sequence.fps, 1)), 0.f);
        for (int i = 0; i < static_cast<int>(modifierTrack->keyframes.size()); ++i)
        {
            if (fabsf(modifierTrack->keyframes[i].time - keyTime) <= 0.0001f)
            {
                _selectedTrackIndex = trackIndex;
                _selectedKeyIndex = i;
                _currentFrame = frame;
                _requestOpenSelectedKeyPopup = true;
                _isPreviewDirty = true;
                return;
            }
        }

        CameraKeyframe keyframe{};
        keyframe.time = keyTime;
        modifierTrack->keyframes.push_back(keyframe);
        _selectedTrackIndex = trackIndex;
        _currentFrame = frame;
        Sort_SelectedModifierTrackKeyframes();

        for (int i = 0; i < static_cast<int>(modifierTrack->keyframes.size()); ++i)
        {
            if (fabsf(modifierTrack->keyframes[i].time - keyTime) <= 0.0001f)
            {
                _selectedKeyIndex = i;
                break;
            }
        }

        _requestOpenSelectedKeyPopup = true;
        _isPreviewDirty = true;
        return;
    }

    if (trackIndex < 0 || trackIndex >= cinematicTrackCount)
        return;

    CINEMATIC_TRACK& track = _sequence.cinematicTracks[trackIndex];
    frame = std::clamp(frame, _sequence.startFrame, _sequence.endFrame);

    for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
    {
        if (track.keyframes[i].frame == frame)
        {
            _selectedTrackIndex = trackIndex;
            _selectedKeyIndex = i;
            _currentFrame = frame;
            _requestOpenSelectedKeyPopup = true;
            _isPreviewDirty = true;
            return;
        }
    }

    CINEMATIC_KEYFRAME keyframe{};
    keyframe.frame = frame;
    if (!_hasPendingInsertPose ||
        _pendingInsertTrackIndex != trackIndex ||
        _pendingInsertFrame != frame)
    {
        return;
    }

    keyframe.camPose = _pendingInsertPose;

    track.keyframes.push_back(keyframe);
    _selectedTrackIndex = trackIndex;
    _currentFrame = frame;
    Sort_SelectedTrackKeyframes();

    for (int i = 0; i < static_cast<int>(track.keyframes.size()); ++i)
    {
        if (track.keyframes[i].frame == frame)
        {
            _selectedKeyIndex = i;
            break;
        }
    }

    _requestOpenSelectedKeyPopup = true;
    _isPreviewDirty = true;
}

void Cinematic_Tool::Clear_AllSequenceKeyframes()
{
    CLIENT->Stop_Sequence(true);
    Clear_PendingInsertRequest();

    for (auto& track : _sequence.cinematicTracks)
        track.keyframes.clear();
    _sequence.modifierClip = {};
    _sequence.cameraShots.clear();
    for (auto& track : _sequence.actorRelativeCameraTracks)
        track.keyframes.clear();
    for (auto& actorTrack : _sequence.actorTransformTracks)
        actorTrack.keyframes.clear();
    for (auto& partTrack : _sequence.partRotationTracks)
        partTrack.keyframes.clear();

    _selectedKeyIndex = -1;
    _selectedCameraShotIndex = -1;
    _selectedActorRelativeCameraKeyIndex = -1;
    _selectedActorTransformKeyIndex = -1;
    _selectedPartRotationKeyIndex = -1;
    _currentFrame = std::clamp(_currentFrame, _sequence.startFrame, _sequence.endFrame);
    _lastPreviewFrame = -1;
    _lastPreviewTrackIndex = -1;
    _lastPreviewKeyIndex = -1;
    _isPreviewDirty = true;
}

void Cinematic_Tool::Reset_ToNewSequence()
{
    CLIENT->Stop_Sequence(true);
    Clear_PendingInsertRequest();

    _sequence = {};
    Ensure_DefaultTrack();
    _selectedTrackIndex = 0;
    _selectedKeyIndex = -1;
    _selectedSequenceFileIndex = -1;
    _selectedEventIndex = -1;
    _selectedCameraShotIndex = -1;
    _selectedActorRelativeCameraTrackIndex = -1;
    _selectedActorRelativeCameraKeyIndex = -1;
    _selectedActorTransformTrackIndex = -1;
    _selectedActorTransformKeyIndex = -1;
    _selectedPartRotationTrackIndex = -1;
    _selectedPartRotationKeyIndex = -1;
    _currentFrame = std::clamp(_sequence.startFrame, _sequence.startFrame, _sequence.endFrame);
    _currentFrameExact = static_cast<float>(_currentFrame);
    _useExactFramePreview = false;
    _lastPreviewFrame = -1;
    _lastPreviewTrackIndex = -1;
    _lastPreviewKeyIndex = -1;
    _isPreviewDirty = true;
}

void Cinematic_Tool::Refresh_SequenceFileList()
{
    _sequenceFilePaths.clear();

    const fs::path sequenceFolder = "../../../Client/Bin/Resources/Data/json/Cinematics";
    if (!fs::exists(sequenceFolder))
        return;

    for (const auto& entry : fs::directory_iterator(sequenceFolder))
    {
        if (!entry.is_regular_file())
            continue;

        if (entry.path().extension() != ".json")
            continue;

        _sequenceFilePaths.push_back(entry.path());
    }

    std::sort(_sequenceFilePaths.begin(), _sequenceFilePaths.end());
}

void Cinematic_Tool::Load_SelectedSequenceFile()
{
    if (_selectedSequenceFileIndex < 0 ||
        _selectedSequenceFileIndex >= static_cast<int>(_sequenceFilePaths.size()))
    {
        return;
    }

    CINEMATIC_SEQUENCE loadedSequence{};
    const string filePath = _sequenceFilePaths[_selectedSequenceFileIndex].string();
    if (!Cinematic_SequenceIO::LoadSequenceFromFile(filePath, loadedSequence))
        return;

    CLIENT->Stop_Sequence(true);
    Clear_PendingInsertRequest();
    _sequence = std::move(loadedSequence);

    Ensure_DefaultTrack();
    _currentFrame = std::clamp(_sequence.startFrame, _sequence.startFrame, _sequence.endFrame);
    _selectedTrackIndex = 0;
    _selectedKeyIndex = -1;
    _selectedEventIndex = -1;
    _selectedCameraShotIndex = -1;
    _selectedActorRelativeCameraTrackIndex = -1;
    _selectedActorRelativeCameraKeyIndex = -1;
    _selectedActorTransformTrackIndex = -1;
    _selectedActorTransformKeyIndex = -1;
    _selectedPartRotationTrackIndex = -1;
    _selectedPartRotationKeyIndex = -1;
    Clamp_Selection();
    _lastPreviewFrame = -1;
    _lastPreviewTrackIndex = -1;
    _lastPreviewKeyIndex = -1;
    _isPreviewDirty = true;
}

void Cinematic_Tool::Apply_CurrentFramePose()
{
    if (CLIENT->Is_CinematicPlaying())
    {
        if (CLIENT->Is_CinematicPaused())
        {
            if (!_previewApplyCameraTrack)
            {
                if (Sync_PreviewSequenceToRunner())
                {
                    if (_useExactFramePreview)
                    {
                        CLIENT->Preview_CurrentSequenceFrameExact(
                            _currentFrameExact,
                            false,
                            _previewApplyEventTrack,
                            _previewApplyActorTransformTrack,
                            false);
                    }
                    else
                    {
                        CLIENT->Preview_CurrentSequenceFrame(
                            _currentFrame,
                            false,
                            _previewApplyEventTrack,
                            _previewApplyActorTransformTrack,
                            false);
                    }
                }

                return;
            }

            const bool useStoppedEditFreeCameraPreview =
                _isPreviewStoppedForEditing &&
                GAME->Is_ActiveCameraType(ObjectType::FreeCamera);

            if (Sync_PreviewSequenceToRunner())
            {
                if (_useExactFramePreview)
                {
                    CLIENT->Preview_CurrentSequenceFrameExact(
                        _currentFrameExact,
                        _previewApplyCameraTrack,
                        _previewApplyEventTrack,
                        _previewApplyActorTransformTrack,
                        false);
                }
                else
                {
                    CLIENT->Preview_CurrentSequenceFrame(
                        _currentFrame,
                        _previewApplyCameraTrack,
                        _previewApplyEventTrack,
                        _previewApplyActorTransformTrack,
                        false);
                }

                if (useStoppedEditFreeCameraPreview)
                {
                    Sync_FreeCameraToCinematicPreviewPose();
                    return;
                }

                if (!useStoppedEditFreeCameraPreview)
                    return;
            }
        }
        else
        {
            return;
        }
    }

    if (!_previewApplyCameraTrack)
        return;

    Shared<FreeCamera> freeCamera = Get_ActiveFreeCamera();
    if(nullptr == freeCamera)
        return;

    CAMERAPOSE camPose{};
    CINEMATIC_TRACK* shotTrack = nullptr;
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* shotActorRelativeTrack = nullptr;
    const float previewFrameExact = _useExactFramePreview ? _currentFrameExact : static_cast<float>(_currentFrame);
    const bool useCameraShotPreview = Resolve_CameraShotTrack(_sequence, previewFrameExact, shotTrack, shotActorRelativeTrack);
    if (useCameraShotPreview && shotTrack != nullptr)
    {
        CINEMATIC_KEYFRAME* leftKey = nullptr;
        CINEMATIC_KEYFRAME* rightKey = nullptr;
        camPose = Evaluate_CinematicPose(shotTrack, previewFrameExact, leftKey, rightKey);
    }
    else if (useCameraShotPreview && shotActorRelativeTrack != nullptr)
    {
        if (shotActorRelativeTrack->targetId.empty())
            return;

        Shared<GameObject> actor = CLIENT->Get_CinematicBoundActor(shotActorRelativeTrack->targetId);
        if (!Build_ActorRelativeCameraPose(*shotActorRelativeTrack, _currentFrame, actor, camPose))
            return;
    }
    else if (CINEMATIC_TRACK* track = Get_SelectedTrack())
    {
        CINEMATIC_KEYFRAME* leftKey = nullptr;
        CINEMATIC_KEYFRAME* rightKey = nullptr;
        camPose = Evaluate_CinematicPose(track, previewFrameExact, leftKey, rightKey);
    }
    else if (CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* actorRelativeTrack = Get_SelectedActorRelativeCameraTrack())
    {
        if (actorRelativeTrack->targetId.empty())
            return;

        Shared<GameObject> actor = CLIENT->Get_CinematicBoundActor(actorRelativeTrack->targetId);
        if (!Build_ActorRelativeCameraPose(*actorRelativeTrack, _currentFrame, actor, camPose))
            return;
    }
    else
    {
        return;
    }

    const Client::CameraModifierSample modifierSample = Evaluate_CinematicModifierSample(_sequence, static_cast<float>(_currentFrame));
    const float sequenceTime = Evaluate_CinematicSequenceTime(_sequence, static_cast<float>(_currentFrame));

    freeCamera->Get_Transform()->Set_WorldPosition(camPose.position);
    freeCamera->Get_Transform()->Set_RotationEuler(camPose.rotation);
    freeCamera->Set_Pitch(camPose.rotation.x);
    freeCamera->Set_Yaw(camPose.rotation.y);
    freeCamera->Set_FovY(camPose.fovY);
    freeCamera->Set_Near(camPose.nearPlane);
    freeCamera->Set_Far(camPose.farPlane);

    freeCamera->Update_TransformMatrices();

    const Vec3 shakeOffset = Sample_CameraModifierShakeOffset(
        modifierSample,
        sequenceTime,
        freeCamera->Get_Transform()->Get_WorldRight(),
        freeCamera->Get_Transform()->Get_WorldUp());

    freeCamera->Get_Transform()->Set_WorldPosition(camPose.position + shakeOffset);
    freeCamera->Update_TransformMatrices();
    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
}

bool Cinematic_Tool::Sync_PreviewSequenceToRunner()
{
    Sort_AllActorTransformTrackKeyframes();
    Sort_AllPartRotationTrackKeyframes();

    int previewTrackIndex = -1;
    if (_selectedTrackIndex >= 0 &&
        _selectedTrackIndex < Get_ModifierTrackStartIndex(_sequence))
    {
        previewTrackIndex = _selectedTrackIndex;
    }

    return CLIENT->Sync_CinematicPreviewSequence(_sequence, previewTrackIndex);
}

void Cinematic_Tool::Request_CinematicPreviewRefresh(bool rebuildContext)
{
    _isPreviewDirty = true;

    const bool isPlaying = CLIENT->Is_CinematicPlaying();
    const bool isPaused = CLIENT->Is_CinematicPaused();
    if ((!isPlaying || isPaused) && _previewApplyCameraTrack)
    {
        if (!_isPreviewStoppedForEditing ||
            !GAME->Is_ActiveCameraType(ObjectType::FreeCamera))
        {
            Enter_FreeCameraEditPreview(false);
        }
        else
        {
            Apply_CurrentFramePose();
        }
        return;
    }

    if (!isPlaying || !isPaused)
        return;

    if (!Sync_PreviewSequenceToRunner())
        return;

    if (_useExactFramePreview)
    {
        CLIENT->Preview_CurrentSequenceFrameExact(
            _currentFrameExact,
            _previewApplyCameraTrack,
            _previewApplyEventTrack,
            _previewApplyActorTransformTrack,
            rebuildContext);
    }
    else
    {
        CLIENT->Preview_CurrentSequenceFrame(
            _currentFrame,
            _previewApplyCameraTrack,
            _previewApplyEventTrack,
            _previewApplyActorTransformTrack,
            rebuildContext);
    }
}

void Cinematic_Tool::Enter_FreeCameraEditPreview(bool requestViewportFocus)
{
    if (CLIENT->Is_CinematicPlaying() && !CLIENT->Is_CinematicPaused())
        CLIENT->Pause_Sequence();

    _isPreviewStoppedForEditing = true;
    EDITOR->Set_CinematicPreviewFreeCameraOverride(true);
    GAME->Set_PreviewLetterboxEnabled(true);
    EDITOR->Activate_FreeCamera();

    if (requestViewportFocus)
    {
        if (EDITOR->IsPlaying())
            EDITOR->Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Game);
        else
            EDITOR->Request_WindowFocus(EDITOR_VIEWPORT_TARGET::Scene);
    }

    _isPreviewDirty = true;
    Apply_CurrentFramePose();
}

void Cinematic_Tool::Sort_AllActorTransformTrackKeyframes()
{
    for (auto& track : _sequence.actorRelativeCameraTracks)
    {
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& lhs, const CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME& rhs)
            {
                return lhs.frame < rhs.frame;
            });
    }

    for (auto& track : _sequence.actorTransformTracks)
    {
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& lhs, const CINEMATIC_ACTOR_TRANSFORM_KEYFRAME& rhs)
            {
                return lhs.frame < rhs.frame;
            });
    }
}

void Cinematic_Tool::Sort_AllPartRotationTrackKeyframes()
{
    for (auto& track : _sequence.partRotationTracks)
    {
        std::sort(track.keyframes.begin(), track.keyframes.end(),
            [](const CINEMATIC_PART_ROTATION_KEYFRAME& lhs, const CINEMATIC_PART_ROTATION_KEYFRAME& rhs)
            {
                return lhs.frame < rhs.frame;
            });
    }
}

void Cinematic_Tool::Sync_FreeCameraToCinematicPreviewPose()
{
    EDITOR->Activate_FreeCamera();

    Shared<Client::CinematicCamera> cinematicCamera =
        dynamic_pointer_cast<Client::CinematicCamera>(GAME->Get_CameraByType(ObjectType::CinematicCamera));
    Shared<Client::FreeCamera> freeCamera =
        dynamic_pointer_cast<Client::FreeCamera>(GAME->Get_CameraByType(ObjectType::FreeCamera));

    if (cinematicCamera == nullptr || freeCamera == nullptr)
        return;

    Shared<TransformCom> cinematicTransform = cinematicCamera->Get_Transform();
    Shared<TransformCom> freeTransform = freeCamera->Get_Transform();
    if (cinematicTransform == nullptr || freeTransform == nullptr)
        return;

    freeTransform->Set_WorldPosition(cinematicTransform->Get_WorldPosition());
    freeTransform->Set_WorldRotationEuler(cinematicTransform->Get_WorldRotationEuler());
    freeCamera->Set_FovY(cinematicCamera->Get_FovY());
    freeCamera->Set_Near(cinematicCamera->Get_Near());
    freeCamera->Set_Far(cinematicCamera->Get_Far());
    freeCamera->Set_Yaw(cinematicCamera->Get_Yaw());
    freeCamera->Set_Pitch(cinematicCamera->Get_Pitch());
    freeCamera->Update_TransformMatrices();

    GAME->UpdateFrustum(*GAME->Get_Transform(D3DTS::View), *GAME->Get_Transform(D3DTS::Proj));
}

void Cinematic_Tool::Apply_CurrentBattleClipPreview()
{
    Shared<Client::BattleTurnCamera> battleTurnCamera = Get_BattleTurnCamera();
    if (battleTurnCamera == nullptr)
        return;

    BattleTurnCamera::BattleShotRequest request {};
    request.mode = _selectedBattleClipMode;
    request.subject = Resolve_BattleClipPreviewSubject();
    request.opponent = Resolve_BattleClipPreviewOpponent();

    if (request.subject == nullptr)
        return;

    _battleClipCurrentFrame = std::clamp(_battleClipCurrentFrame, 0, Get_BattleClipFrameMax(_editingBattleClip));
    const float previewTimeSec = static_cast<float>(_battleClipCurrentFrame) / kBattleClipPreviewFps;

    Apply_BattleClipPreviewAnimation(previewTimeSec);
    battleTurnCamera->Set_Clip(_selectedBattleClipMode, _editingBattleClip);
    battleTurnCamera->Set_IsClipPreview(true);
    battleTurnCamera->Preview_ClipAtTime(request, previewTimeSec);
    GAME->Change_ActiveCamera(battleTurnCamera);
}

bool Cinematic_Tool::Try_CaptureSelectedActorRelativeCameraValues(
    float& outDistance,
    float& outHeight,
    float& outSideOffset,
    float& outYawOffset,
    float& outPitch,
    float& outLookHeight) const
{
    const CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* track =
        (_selectedActorRelativeCameraTrackIndex >= 0 &&
            _selectedActorRelativeCameraTrackIndex < static_cast<int>(_sequence.actorRelativeCameraTracks.size()))
        ? &_sequence.actorRelativeCameraTracks[_selectedActorRelativeCameraTrackIndex]
        : nullptr;
    if (track == nullptr || track->targetId.empty())
        return false;

    Shared<GameObject> actor = CLIENT->Get_CinematicBoundActor(track->targetId);
    Shared<FreeCamera> freeCamera = Get_ActiveFreeCamera();
    if (actor == nullptr || freeCamera == nullptr)
        return false;

    Shared<TransformCom> actorTransform = actor->Get_Transform();
    Shared<TransformCom> cameraTransform = freeCamera->Get_Transform();
    if (actorTransform == nullptr || cameraTransform == nullptr)
        return false;

    const Vec3 actorPos = actorTransform->Get_WorldPosition();
    const Vec3 actorRot = actorTransform->Get_WorldRotationEuler();
    const Vec3 cameraPos = cameraTransform->Get_WorldPosition();
    const Vec3 cameraRot = cameraTransform->Get_RotationEuler();
    const Vec3 delta = cameraPos - actorPos;

    const float actorYawRad = XMConvertToRadians(actorRot.y);
    Vec3 actorForward{};
    actorForward.x = sinf(actorYawRad);
    actorForward.y = 0.f;
    actorForward.z = cosf(actorYawRad);
    actorForward.Normalize();

    Vec3 actorRight{};
    actorRight.x = cosf(actorYawRad);
    actorRight.y = 0.f;
    actorRight.z = -sinf(actorYawRad);
    actorRight.Normalize();

    const float flatX = delta.Dot(actorRight);
    const float flatZ = delta.Dot(actorForward);
    outSideOffset = flatX;
    outHeight = delta.y;
    outDistance = sqrtf(flatX * flatX + flatZ * flatZ);

    const float cameraYaw = cameraRot.y;
    outYawOffset = cameraYaw - actorRot.y;
    while (outYawOffset > 180.f)
        outYawOffset -= 360.f;
    while (outYawOffset < -180.f)
        outYawOffset += 360.f;

    outPitch = cameraRot.x;
    return true;
}

bool Cinematic_Tool::Try_CaptureSelectedActorTransform(Vec3& outPosition, Vec3& outRotation) const
{
    if (_selectedActorTransformTrackIndex < 0 ||
        _selectedActorTransformTrackIndex >= static_cast<int>(_sequence.actorTransformTracks.size()))
    {
        return false;
    }

    const auto& track = _sequence.actorTransformTracks[_selectedActorTransformTrackIndex];
    if (track.targetId.empty())
        return false;

    Shared<GameObject> actor = CLIENT->Get_CinematicBoundActor(track.targetId);
    if (actor == nullptr)
        return false;

    Shared<TransformCom> transform = actor->Get_Transform();
    if (transform == nullptr)
        return false;

    outPosition = transform->Get_WorldPosition();
    outRotation = transform->Get_WorldRotationEuler();
    return true;
}

bool Cinematic_Tool::Try_CaptureSelectedPartRotation(Vec3& outRotation) const
{
    if (_selectedPartRotationTrackIndex < 0 ||
        _selectedPartRotationTrackIndex >= static_cast<int>(_sequence.partRotationTracks.size()))
    {
        return false;
    }

    const auto& track = _sequence.partRotationTracks[_selectedPartRotationTrackIndex];
    if (track.targetId.empty() || track.partTag.empty())
        return false;

    Shared<GameObject> actor = CLIENT->Get_CinematicBoundActor(track.targetId);
    if (actor == nullptr)
        return false;

    const auto container = dynamic_pointer_cast<ContainerObject>(actor);
    if (container == nullptr)
        return false;

    PartObject* part = container->Get_PartObject(String::ToWString(track.partTag));
    if (part == nullptr)
        return false;

    Shared<TransformCom> transform = part->Get_Transform();
    if (transform == nullptr)
        return false;

    outRotation = transform->Get_RotationEuler();
    return true;
}

CINEMATIC_TRACK* Cinematic_Tool::Get_SelectedTrack()
{
    if (_selectedTrackIndex < 0 || _selectedTrackIndex >= static_cast<int>(_sequence.cinematicTracks.size()))
        return nullptr;

    return &_sequence.cinematicTracks[_selectedTrackIndex];
}

CINEMATIC_KEYFRAME* Cinematic_Tool::Get_SelectedKeyframe()
{
    CINEMATIC_TRACK* selectedTrack = Get_SelectedTrack();
    if (selectedTrack == nullptr)
        return nullptr;

    if (_selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()))
        return nullptr;

    return &selectedTrack->keyframes[_selectedKeyIndex];
}

bool Cinematic_Tool::Is_SelectedActorRelativeCameraTrack() const
{
    return _selectedTrackIndex >= Get_ActorRelativeTrackStartIndex(_sequence) &&
        _selectedTrackIndex < Get_ModifierTrackStartIndex(_sequence);
}

int Cinematic_Tool::Get_SelectedActorRelativeCameraTrackIndex() const
{
    if (!Is_SelectedActorRelativeCameraTrack())
        return -1;

    return _selectedTrackIndex - Get_ActorRelativeTrackStartIndex(_sequence);
}

CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* Cinematic_Tool::Get_SelectedActorRelativeCameraTrack()
{
    const int trackIndex = Get_SelectedActorRelativeCameraTrackIndex();
    if (trackIndex < 0 || trackIndex >= static_cast<int>(_sequence.actorRelativeCameraTracks.size()))
        return nullptr;

    return &_sequence.actorRelativeCameraTracks[trackIndex];
}

CINEMATIC_ACTOR_RELATIVE_CAMERA_KEYFRAME* Cinematic_Tool::Get_SelectedActorRelativeCameraKeyframe()
{
    CINEMATIC_ACTOR_RELATIVE_CAMERA_TRACK* selectedTrack = Get_SelectedActorRelativeCameraTrack();
    if (selectedTrack == nullptr)
        return nullptr;

    if (_selectedActorRelativeCameraKeyIndex < 0 ||
        _selectedActorRelativeCameraKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()))
    {
        return nullptr;
    }

    return &selectedTrack->keyframes[_selectedActorRelativeCameraKeyIndex];
}

CINEMATIC_PART_ROTATION_TRACK* Cinematic_Tool::Get_SelectedPartRotationTrack()
{
    const int trackIndex = Get_SelectedPartRotationTrackIndex();
    if (trackIndex < 0 || trackIndex >= static_cast<int>(_sequence.partRotationTracks.size()))
        return nullptr;

    return &_sequence.partRotationTracks[trackIndex];
}

CINEMATIC_PART_ROTATION_KEYFRAME* Cinematic_Tool::Get_SelectedPartRotationKeyframe()
{
    CINEMATIC_PART_ROTATION_TRACK* selectedTrack = Get_SelectedPartRotationTrack();
    if (selectedTrack == nullptr)
        return nullptr;

    if (_selectedPartRotationKeyIndex < 0 ||
        _selectedPartRotationKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()))
    {
        return nullptr;
    }

    return &selectedTrack->keyframes[_selectedPartRotationKeyIndex];
}

bool Cinematic_Tool::Is_SelectedModifierTrack() const
{
    return _selectedTrackIndex >= Get_ModifierTrackStartIndex(_sequence) &&
        _selectedTrackIndex < Get_EventTrackIndex(_sequence);
}

int Cinematic_Tool::Get_SelectedModifierTrackIndex() const
{
    if (!Is_SelectedModifierTrack())
        return -1;

    return _selectedTrackIndex - Get_ModifierTrackStartIndex(_sequence);
}

CameraTrack* Cinematic_Tool::Get_SelectedModifierTrack()
{
    switch (Get_SelectedModifierTrackIndex())
    {
    case 0: return &_sequence.modifierClip.shakeAmplitude;
    case 1: return &_sequence.modifierClip.shakeFrequencyX;
    case 2: return &_sequence.modifierClip.shakeFrequencyY;
    case 3: return &_sequence.modifierClip.shakeDecayRateX;
    case 4: return &_sequence.modifierClip.shakeDecayRate;
    case 5: return &_sequence.modifierClip.shakeVerticalRatio;
    case 6: return &_sequence.modifierClip.shakeStrengthScale;
    default: return nullptr;
    }
}

bool Cinematic_Tool::Is_SelectedPartRotationTrack() const
{
    return _selectedTrackIndex >= Get_PartRotationTrackStartIndex(_sequence) &&
        _selectedTrackIndex < Get_PartRotationTrackStartIndex(_sequence) + static_cast<int>(_sequence.partRotationTracks.size());
}

int Cinematic_Tool::Get_SelectedPartRotationTrackIndex() const
{
    if (!Is_SelectedPartRotationTrack())
        return -1;

    return _selectedTrackIndex - Get_PartRotationTrackStartIndex(_sequence);
}

CameraKeyframe* Cinematic_Tool::Get_SelectedModifierKeyframe()
{
    CameraTrack* selectedTrack = Get_SelectedModifierTrack();
    if (selectedTrack == nullptr)
        return nullptr;

    if (_selectedKeyIndex < 0 || _selectedKeyIndex >= static_cast<int>(selectedTrack->keyframes.size()))
        return nullptr;

    return &selectedTrack->keyframes[_selectedKeyIndex];
}

Shared<Client::BattleTurnCamera> Cinematic_Tool::Get_BattleTurnCamera() const
{
    Shared<Camera> camera = GAME->Get_CameraByType(ObjectType::BattleTurnCamera);
    if (camera == nullptr)
        return nullptr;

    return dynamic_pointer_cast<Client::BattleTurnCamera>(camera);
}

CameraTrack* Cinematic_Tool::Get_SelectedBattleClipTrack()
{
    switch (_battleClipSelectedTrackIndex)
    {
    case 0:
        return &_editingBattleClip.distanceOffset;
    case 1:
        return &_editingBattleClip.fovOffset;
    case 2:
        return &_editingBattleClip.shakeAmplitude;
    case 3:
        return &_editingBattleClip.sideOffset;
    case 4:
        return &_editingBattleClip.pivotOffset;
    case 5:
        return &_editingBattleClip.heightOffset;
    case 6:
        return &_editingBattleClip.aimOffset;
    case 7:
        return &_editingBattleClip.lookHeightOffset;
    case 8:
        return &_editingBattleClip.yawOffset;
    default:
        return nullptr;
    }
}

CameraKeyframe* Cinematic_Tool::Get_SelectedBattleClipKeyframe()
{
    CameraTrack* track = Get_SelectedBattleClipTrack();
    if (track == nullptr)
        return nullptr;

    if (_battleClipSelectedKeyIndex < 0 || _battleClipSelectedKeyIndex >= static_cast<int>(track->keyframes.size()))
        return nullptr;

    return &track->keyframes[_battleClipSelectedKeyIndex];
}

void Cinematic_Tool::Clear_PendingInsertRequest()
{
    _hasPendingInsertRequest = false;
    _hasPendingInsertPose = false;
    _pendingInsertTrackIndex = -1;
    _pendingInsertFrame = 0;
    _pendingInsertPose = {};
    _isPreviewDirty = true;
}

Shared<FreeCamera> Cinematic_Tool::Get_ActiveFreeCamera() const
{
    Shared<Camera> activeCamera = GAME->Get_ActiveCamera();
    if (activeCamera == nullptr || activeCamera->Get_ObjectType() != ObjectType::FreeCamera)
        return nullptr;

    return dynamic_pointer_cast<FreeCamera>(activeCamera);
}

bool Cinematic_Tool::Try_CaptureActiveFreeCameraPose(CAMERAPOSE& outPose) const
{
    Shared<FreeCamera> freeCamera = Get_ActiveFreeCamera();
    if (freeCamera == nullptr)
        return false;

    outPose.position = freeCamera->Get_Transform()->Get_WorldPosition();
    outPose.rotation = freeCamera->Get_Transform()->Get_RotationEuler();
    outPose.fovY = freeCamera->Get_FovY();
    outPose.nearPlane = freeCamera->Get_Near();
    outPose.farPlane = freeCamera->Get_Far();
    return true;
}

Shared<Cinematic_Tool> Cinematic_Tool::Create()
{
    Shared<Cinematic_Tool> instance =  make_shared<Cinematic_Tool>();

    if(FAILED(instance->Initialize()))
    {
        instance = nullptr; 
    }
    return instance;
}
