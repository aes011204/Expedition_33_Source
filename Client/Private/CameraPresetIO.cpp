#include "CameraPresetIO.h"
#include "TargetCamera.h"

NS_BEGIN(Client)

CameraPresetIO::CameraPresetIO()
{
}

CameraPresetIO::~CameraPresetIO()
{
}

namespace
{
    string Build_SoulLikePresetFilePath(const string& presetKey)
    {
        if (presetKey.empty())
            return "../../../Client/Bin/Resources/Data/json/CameraPreset/CameraPreset_SOULLIKE.json";

        return "../../../Client/Bin/Resources/Data/json/CameraPreset/CameraPreset_SOULLIKE_" + presetKey + ".json";
    }

    string Build_BattlePresetFilePath(BattleCameraMode cameraMode, const string& skillId)
    {
        const string modeName = string(magic_enum::enum_name(cameraMode));
        string fileName = "BattlePreset_" + modeName;
    
        if (!skillId.empty())
            fileName += "_" + skillId;
    
        return "../../../Client/Bin/Resources/Data/json/CameraPreset/" + fileName + ".json";
    }

    string Build_BattleDriftFilePath(BattleCameraMode cameraMode, bool isBossVariant)
    {
        const string modeName = string(magic_enum::enum_name(cameraMode));
        string fileName = "BattleDrift_" + modeName;
        if (cameraMode == BattleCameraMode::ENEMYATTACK && isBossVariant)
            fileName += "_Boss";

        return "../../../Client/Bin/Resources/Data/json/CameraPreset/" + fileName + ".json";
    }

    string Build_DialoguePresetFilePath(const string& presetId)
    {
        string fileName = "DialoguePreset";
        if (!presetId.empty())
            fileName += "_" + presetId;

        return "../../../Client/Bin/Resources/Data/json/CameraPreset/" + fileName + ".json";
    }
}

void CameraPresetIO::SaveSoulLikePresetToFile(const string& presetKey, const TargetCameraPreset& preset)
{
    json jsonData{};

    jsonData["snapOnEnter"] = preset.snapOnEnter;
    jsonData["followSpeed"] = preset.followSpeed;
    jsonData["targetFollowSpeed"] = preset.targetFollowSpeed;
    jsonData["verticalFollowSpeed"] = preset.verticalFollowSpeed;
    jsonData["targetFollowEasing"] = static_cast<int>(preset.targetFollowEasing);
    jsonData["aimOffset"] = preset.aimOffset;
    jsonData["pivotHeight"] = preset.pivotHeight;
    jsonData["sideOffset"] = preset.sideOffset;
    jsonData["distance"] = preset.distance;
    jsonData["heightOffset"] = preset.heightOffset;
    jsonData["lookHeight"] = preset.lookHeight;
    jsonData["pitchLookScale"] = preset.pitchLookScale;
    jsonData["pitchMax"] = preset.pitchMax;
    jsonData["pitchMin"] = preset.pitchMin;
    jsonData["mouseSensor"] = preset.camera.mouseSensor;
    jsonData["mouseSensitivity"] = preset.camera.mouseSensitivity;
    jsonData["yawOffset"] = preset.yawOffset;
    jsonData["pitchOffset"] = preset.pitchOffset;
    jsonData["rollOffset"] = preset.rollOffset;
    jsonData["fovY"] = preset.camera.fovy;
    jsonData["nearPlane"] = preset.camera.nearPlane;
    jsonData["farPlane"] = preset.camera.farPlane;
    jsonData["allowMouseRotation"] = preset.allowMouseRotation;
    jsonData["doCollisionTest"] = preset.doCollisionTest;
    jsonData["collisionProbeRadius"] = preset.collisionProbeRadius;
    jsonData["collisionSkin"] = preset.collisionSkin;

    const fs::path savePath = Build_SoulLikePresetFilePath(presetKey);
    if (!fs::exists(savePath.parent_path()))
        fs::create_directories(savePath.parent_path());

    ofstream file(savePath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open SoulLike camera preset save file.");
        return;
    }

    file << jsonData.dump(4);
}

bool CameraPresetIO::LoadSoulLikePresetFromFile(const string& presetKey, TargetCameraPreset& outPreset)
{
    ifstream file(Build_SoulLikePresetFilePath(presetKey));
    if (!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    TargetCameraPreset preset = outPreset;
    const int targetFollowEasing = jsonData.value(
        "targetFollowEasing",
        static_cast<int>(BattleCameraTransitionEasing::EaseOut));

    preset.snapOnEnter = jsonData.value("snapOnEnter", preset.snapOnEnter);
    preset.followSpeed = jsonData.value("followSpeed", preset.followSpeed);
    preset.targetFollowSpeed = jsonData.value("targetFollowSpeed", preset.targetFollowSpeed);
    preset.verticalFollowSpeed = jsonData.value("verticalFollowSpeed", preset.verticalFollowSpeed);
    switch (targetFollowEasing)
    {
    case static_cast<int>(BattleCameraTransitionEasing::Linear):
        preset.targetFollowEasing = BattleCameraTransitionEasing::Linear;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseIn):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseIn;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOut):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOut;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInStrong):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutStrong):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOutStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInOutStrong):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInOutStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInCubic):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInCubic;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutCubic):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOutCubic;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInExpo):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInExpo;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutExpo):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOutExpo;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutBack):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOutBack;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInOut):
    default:
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInOut;
        break;
    }

    preset.distance = jsonData.value("distance", preset.distance);
    preset.sideOffset = jsonData.value("sideOffset", preset.sideOffset);
    preset.heightOffset = jsonData.value("heightOffset", preset.heightOffset);
    preset.aimOffset = jsonData.value("aimOffset", preset.aimOffset);
    preset.pivotHeight = jsonData.value("pivotHeight", preset.pivotHeight);
    preset.pitchLookScale = jsonData.value("pitchLookScale", preset.pitchLookScale);
    preset.pitchMin = jsonData.value("pitchMin", preset.pitchMin);
    preset.pitchMax = jsonData.value("pitchMax", preset.pitchMax);
    preset.lookHeight = jsonData.value("lookHeight", preset.lookHeight);
    preset.yawOffset = jsonData.value("yawOffset", preset.yawOffset);
    preset.pitchOffset = jsonData.value("pitchOffset", preset.pitchOffset);
    preset.rollOffset = jsonData.value("rollOffset", preset.rollOffset);
    preset.camera.mouseSensor = jsonData.value("mouseSensor", preset.camera.mouseSensor);
    preset.camera.mouseSensitivity = jsonData.value("mouseSensitivity", preset.camera.mouseSensitivity);
    preset.camera.fovy = jsonData.value("fovY", preset.camera.fovy);
    preset.camera.nearPlane = jsonData.value("nearPlane", preset.camera.nearPlane);
    preset.camera.farPlane = jsonData.value("farPlane", preset.camera.farPlane);
    preset.allowMouseRotation = jsonData.value("allowMouseRotation", preset.allowMouseRotation);
    preset.doCollisionTest = jsonData.value("doCollisionTest", preset.doCollisionTest);
    preset.collisionProbeRadius = jsonData.value("collisionProbeRadius", preset.collisionProbeRadius);
    preset.collisionSkin = jsonData.value("collisionSkin", preset.collisionSkin);

    outPreset = preset;
    return true;
}

void CameraPresetIO::SaveSoulLikeSkillPresetToFile(const string& skillKey, const TargetCameraPreset& preset)
{
    if (skillKey.empty())
        return;

    SaveSoulLikePresetToFile("PlayerSkill_" + skillKey, preset);
}

bool CameraPresetIO::LoadSoulLikeSkillPresetFromFile(const string& skillKey, TargetCameraPreset& outPreset)
{
    if (skillKey.empty())
        return false;

    return LoadSoulLikePresetFromFile("PlayerSkill_" + skillKey, outPreset);
}

void CameraPresetIO::SaveDialoguePresetToFile(const string& presetId, const DialogueCameraPreset& preset)
{
    json jsonData{};

    jsonData["distance"] = preset.distance;
    jsonData["sideOffset"] = preset.sideOffset;
    jsonData["heightOffset"] = preset.heightOffset;
    jsonData["aimOffset"] = preset.aimOffset;
    jsonData["pitchLookScale"] = preset.pitchLookScale;
    jsonData["pitchMin"] = preset.pitchMin;
    jsonData["pitchMax"] = preset.pitchMax;
    jsonData["yawOffset"] = preset.yawOffset;
    jsonData["pitchOffset"] = preset.pitchOffset;
    jsonData["rollOffset"] = preset.rollOffset;
    jsonData["lookHeight"] = preset.lookHeight;
    jsonData["pivotHeight"] = preset.pivotHeight;
    jsonData["mouseSensor"] = preset.camera.mouseSensor;
    jsonData["mouseSensitivity"] = preset.camera.mouseSensitivity;
    jsonData["fovY"] = preset.camera.fovy;
    jsonData["nearPlane"] = preset.camera.nearPlane;
    jsonData["farPlane"] = preset.camera.farPlane;

    const fs::path savePath = Build_DialoguePresetFilePath(presetId);
    if (!fs::exists(savePath.parent_path()))
        fs::create_directories(savePath.parent_path());

    ofstream file(savePath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open dialogue preset save file.");
        return;
    }

    file << jsonData.dump(4);
}

bool CameraPresetIO::LoadDialoguePresetFromFile(const string& presetId, DialogueCameraPreset& outPreset)
{
    ifstream file(Build_DialoguePresetFilePath(presetId));
    if (!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    DialogueCameraPreset preset = outPreset;
    preset.distance = jsonData.value("distance", preset.distance);
    preset.sideOffset = jsonData.value("sideOffset", preset.sideOffset);
    preset.heightOffset = jsonData.value("heightOffset", preset.heightOffset);
    preset.aimOffset = jsonData.value("aimOffset", preset.aimOffset);
    preset.pitchLookScale = jsonData.value("pitchLookScale", preset.pitchLookScale);
    preset.pitchMin = jsonData.value("pitchMin", preset.pitchMin);
    preset.pitchMax = jsonData.value("pitchMax", preset.pitchMax);
    preset.yawOffset = jsonData.value("yawOffset", preset.yawOffset);
    preset.pitchOffset = jsonData.value("pitchOffset", preset.pitchOffset);
    preset.rollOffset = jsonData.value("rollOffset", preset.rollOffset);
    preset.lookHeight = jsonData.value("lookHeight", preset.lookHeight);
    preset.pivotHeight = jsonData.value("pivotHeight", preset.pivotHeight);
    preset.camera.mouseSensor = jsonData.value("mouseSensor", preset.camera.mouseSensor);
    preset.camera.mouseSensitivity = jsonData.value("mouseSensitivity", preset.camera.mouseSensitivity);
    preset.camera.fovy = jsonData.value("fovY", preset.camera.fovy);
    preset.camera.nearPlane = jsonData.value("nearPlane", preset.camera.nearPlane);
    preset.camera.farPlane = jsonData.value("farPlane", preset.camera.farPlane);

    outPreset = preset;
    return true;
}

void CameraPresetIO::SaveTargetPresetToFile(TargetCameraMode cameraMode, const TargetCameraPreset& preset)
{
    json jsonData {};

    jsonData["snapOnEnter"]     = preset.snapOnEnter;
    jsonData["followSpeed"]     = preset.followSpeed;
    jsonData["targetFollowSpeed"] = preset.targetFollowSpeed;
    jsonData["verticalFollowSpeed"] = preset.verticalFollowSpeed;
    jsonData["targetFollowEasing"] = static_cast<int>(preset.targetFollowEasing);
    jsonData["aimOffset"]       = preset.aimOffset;
    jsonData["pivotHeight"]     = preset.pivotHeight;
    jsonData["sideOffset"]      = preset.sideOffset;
    jsonData["distance"]        = preset.distance;
    jsonData["heightOffset"]    = preset.heightOffset;
    jsonData["lookHeight"]      = preset.lookHeight;
    jsonData["pitchLookScale"]  = preset.pitchLookScale;
    jsonData["pitchMax"]        = preset.pitchMax;
    jsonData["pitchMin"]        = preset.pitchMin;
    jsonData["mouseSensor"]     = preset.camera.mouseSensor;
    jsonData["mouseSensitivity"]= preset.camera.mouseSensitivity;
    jsonData["yawOffset"]       = preset.yawOffset;
    jsonData["pitchOffset"]     = preset.pitchOffset;
    jsonData["rollOffset"]      = preset.rollOffset;
    jsonData["fovY"]            = preset.camera.fovy;
    jsonData["nearPlane"]       = preset.camera.nearPlane;
    jsonData["farPlane"]        = preset.camera.farPlane;
    jsonData["allowMouseRotation"] = preset.allowMouseRotation;

    const string name = string(magic_enum::enum_name(cameraMode));

    const string filePath = "../../../Client/Bin/Resources/Data/json/CameraPreset/CameraPreset_" + name + ".json";

    const fs::path savePath = filePath;
    if (!fs::exists(savePath.parent_path()))
        fs::create_directories(savePath.parent_path());

    ofstream file(savePath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open camera preset save file.");
        return;
    }

    file << jsonData.dump(4);
}


bool CameraPresetIO::LoadTargetPresetFromFile(TargetCameraMode cameraMode, TargetCameraPreset& outPreset)
{
    const string name = string(magic_enum::enum_name(cameraMode));

    const string filePath = "../../../Client/Bin/Resources/Data/json/CameraPreset/CameraPreset_" + name + ".json";

    ifstream file(filePath);
    if (!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    TargetCameraPreset preset;
    const int targetFollowEasing = jsonData.value(
        "targetFollowEasing",
        static_cast<int>(BattleCameraTransitionEasing::EaseOut));

    preset.snapOnEnter = jsonData.value("snapOnEnter", false);
    preset.followSpeed = jsonData.value("followSpeed", preset.followSpeed);
    preset.targetFollowSpeed = jsonData.value("targetFollowSpeed", preset.targetFollowSpeed);
    preset.verticalFollowSpeed = jsonData.value("verticalFollowSpeed", preset.verticalFollowSpeed);
    switch (targetFollowEasing)
    {
    case static_cast<int>(BattleCameraTransitionEasing::Linear):
        preset.targetFollowEasing = BattleCameraTransitionEasing::Linear;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseIn):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseIn;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOut):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOut;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInStrong):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutStrong):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOutStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInOutStrong):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInOutStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInCubic):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInCubic;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutCubic):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOutCubic;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInExpo):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInExpo;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutExpo):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOutExpo;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutBack):
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseOutBack;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInOut):
    default:
        preset.targetFollowEasing = BattleCameraTransitionEasing::EaseInOut;
        break;
    }
    preset.distance  = jsonData.value("distance", preset.distance);
    preset.sideOffset  = jsonData.value("sideOffset", preset.sideOffset);
    preset.heightOffset  = jsonData.value("heightOffset", preset.heightOffset);
    preset.aimOffset = jsonData.value("aimOffset", preset.aimOffset);
    preset.pivotHeight  = jsonData.value("pivotHeight", preset.pivotHeight);
    preset.pitchLookScale  = jsonData.value("pitchLookScale", preset.pitchLookScale);
    preset.pitchMin  = jsonData.value("pitchMin", preset.pitchMin);
    preset.pitchMax  = jsonData.value("pitchMax", preset.pitchMax);
    preset.lookHeight  = jsonData.value("lookHeight", preset.lookHeight);
    preset.yawOffset = jsonData.value("yawOffset", 0.f);
    preset.pitchOffset = jsonData.value ("pitchOffset", 0.f);
    preset.rollOffset = jsonData.value("rollOffset", 0.f);
    preset.camera.mouseSensor = jsonData.value("mouseSensor",preset.camera.mouseSensor);
    preset.camera.mouseSensitivity = jsonData.value("mouseSensitivity", preset.camera.mouseSensitivity);
    preset.camera.fovy = jsonData.value("fovY", preset.camera.fovy);
    preset.camera.nearPlane = jsonData.value("nearPlane", preset.camera.nearPlane);
    preset.camera.farPlane = jsonData.value("farPlane", preset.camera.farPlane);
    preset.allowMouseRotation = jsonData.value("allowMouseRotation",true);
    outPreset = preset;

    return true;
}

void CameraPresetIO::SavePreviewToFile(PreviewShotPanel panel, HeroId heroId, const PreviewShotPreset& preset)
{
    json jsonData {};

    jsonData["lookOffset"]["x"] = preset.lookOffset.x;
    jsonData["lookOffset"]["y"] = preset.lookOffset.y;
    jsonData["lookOffset"]["z"] = preset.lookOffset.z;
    jsonData["followSpeed"] = preset.followSpeed;
    jsonData["distance"] = preset.distance;

    const string panelName = string(magic_enum::enum_name(panel));
    const string heroName = string(magic_enum::enum_name(heroId));

    const string filePath = "../../../Client/Bin/Resources/Data/json/CameraPreset/PreviewPreset_" + panelName + "_"+ heroName + ".json";

    const fs::path savePath = filePath;
    if (!fs::exists(savePath.parent_path()))
        fs::create_directories(savePath.parent_path());

    ofstream file(savePath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open preview preset save file.");
        return;
    }

    file << jsonData.dump(4);
}

bool CameraPresetIO::LoadPreviewFromFile(PreviewShotPanel panel, HeroId heroId, PreviewShotPreset& outPreset)
{
    const string panelName = string(magic_enum::enum_name(panel));
    const string heroName = string(magic_enum::enum_name(heroId));

    const string filePath = "../../../Client/Bin/Resources/Data/json/CameraPreset/PreviewPreset_" + panelName + "_" + heroName + ".json";

    ifstream  file(filePath);

    if(!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    PreviewShotPreset preset {};

    preset.lookOffset.x = jsonData["lookOffset"].value("x", 0.f);
    preset.lookOffset.y = jsonData["lookOffset"].value("y", 0.f);
    preset.lookOffset.z = jsonData["lookOffset"].value("z", 0.f);
    preset.followSpeed = jsonData.value("followSpeed", 0.f);
    preset.distance = jsonData.value("distance", 0.f);

    outPreset = preset;

    return true;
}

void CameraPresetIO::SaveBattlePresetToFile(BattleCameraMode cameraMode, const BattleCameraPreset& preset, const string& skillId)
{
    json jsonData {};

    jsonData["snapOnEnter"]     = preset.snapOnEnter;
    jsonData["followSpeed"]     = preset.followSpeed;
    jsonData["aimOffset"]       = preset.aimOffset;
    jsonData["pivotHeight"]     = preset.pivotHeight;
    jsonData["sideOffset"]      = preset.sideOffset;
    jsonData["distance"]        = preset.distance;
    jsonData["heightOffset"]    = preset.heightOffset;
    jsonData["lookHeight"]      = preset.lookHeight;
    jsonData["pitchLookScale"]  = preset.pitchLookScale;
    jsonData["pitchMax"]        = preset.pitchMax;
    jsonData["pitchMin"]        = preset.pitchMin;
    jsonData["mouseSensor"]     = preset.camera.mouseSensor;
    jsonData["mouseSensitivity"]= preset.camera.mouseSensitivity;
    jsonData["yawOffset"]       = preset.yawOffset;
    jsonData["pitchOffset"]     = preset.pitchOffset;
    jsonData["rollOffset"]      = preset.rollOffset;
    jsonData["fovY"]            = preset.camera.fovy;
    jsonData["farPlane"]        = preset.camera.farPlane;
    jsonData["nearPlane"]       = preset.camera.nearPlane;
    jsonData["transitionEasing"]= preset.transitionEasing;
    jsonData["transitionDuration"] = preset.transitionDuration;
    //jsonData["allowMouseRotation"] = preset.allowMouseRotation;

    const string filePath = Build_BattlePresetFilePath(cameraMode, skillId);

    const fs::path savePath = filePath;
    if (!fs::exists(savePath.parent_path()))
        fs::create_directories(savePath.parent_path());

    ofstream file(savePath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open camera preset save file.");
        return;
    }

    file << jsonData.dump(4);
}

bool CameraPresetIO:: LoadBattlePresetFromFile(BattleCameraMode cameraMode, BattleCameraPreset& outPreset, const string& skillId)
{
    const string filePath = Build_BattlePresetFilePath(cameraMode, skillId);

    ifstream file(filePath);
    if (!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    BattleCameraPreset preset {};
    const int transitionEasing = jsonData.value("transitionEasing", static_cast<int>(BattleCameraTransitionEasing::EaseOut));

    preset.snapOnEnter = jsonData.value("snapOnEnter", false);
    switch (transitionEasing)
    {
    case static_cast<int>(BattleCameraTransitionEasing::Linear):
        preset.transitionEasing = BattleCameraTransitionEasing::Linear;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseIn):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseIn;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOut):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseOut;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInStrong):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseInStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutStrong):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseOutStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInOutStrong):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseInOutStrong;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInCubic):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseInCubic;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutCubic):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseOutCubic;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInExpo):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseInExpo;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutExpo):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseOutExpo;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseOutBack):
        preset.transitionEasing = BattleCameraTransitionEasing::EaseOutBack;
        break;
    case static_cast<int>(BattleCameraTransitionEasing::EaseInOut):
    default:
        preset.transitionEasing = BattleCameraTransitionEasing::EaseInOut;
        break;
    }
    preset.followSpeed = jsonData.value("followSpeed", preset.followSpeed);
    preset.distance  = jsonData.value("distance", preset.distance);
    preset.sideOffset  = jsonData.value("sideOffset", preset.sideOffset);
    preset.heightOffset  = jsonData.value("heightOffset", preset.heightOffset);
    preset.aimOffset = jsonData.value("aimOffset", preset.aimOffset);
    preset.pivotHeight  = jsonData.value("pivotHeight", preset.pivotHeight);
    preset.pitchLookScale  = jsonData.value("pitchLookScale", preset.pitchLookScale);
    preset.pitchMin  = jsonData.value("pitchMin", preset.pitchMin);
    preset.pitchMax  = jsonData.value("pitchMax", preset.pitchMax);
    preset.lookHeight  = jsonData.value("lookHeight", preset.lookHeight);
    preset.yawOffset = jsonData.value("yawOffset", 0.f);
    preset.pitchOffset = jsonData.value ("pitchOffset", 0.f);
    preset.rollOffset = jsonData.value("rollOffset", 0.f);
    preset.camera.mouseSensor = jsonData.value("mouseSensor",preset.camera.mouseSensor);
    preset.camera.mouseSensitivity = jsonData.value("mouseSensitivity", preset.camera.mouseSensitivity);
    preset.camera.fovy = jsonData.value("fovY",preset.camera.fovy);
    preset.camera.farPlane = jsonData.value("farPlane", preset.camera.farPlane);
    preset.camera.nearPlane = jsonData.value("nearPlane", preset.camera.nearPlane);
    preset.transitionDuration = jsonData.value("transitionDuration", preset.transitionDuration);
    //preset.allowMouseRotation = jsonData.value("allowMouseRotation",false);
    outPreset = preset;


    return true;
    
}

void CameraPresetIO::SaveBattleDriftToFile(BattleCameraMode mode, const BattleCameraDriftProfile& profile, bool isBossVariant)
{
    json jsonData{};

    jsonData["enabled"] = profile.enabled;
    jsonData["lateralAmplitude"] = profile.lateralAmplitude;
    jsonData["verticalAmplitude"] = profile.verticalAmplitude;
    jsonData["depthAmplitude"] = profile.depthAmplitude;
    jsonData["rollAmplitude"] = profile.rollAmplitude;
    jsonData["lateralFrequency"] = profile.lateralFrequency;
    jsonData["verticalFrequency"] = profile.verticalFrequency;
    jsonData["depthFrequency"] = profile.depthFrequency;
    jsonData["rollFrequency"] = profile.rollFrequency;

    const string filePath = Build_BattleDriftFilePath(mode, isBossVariant);

    const fs::path savePath = filePath;
    if (!fs::exists(savePath.parent_path()))
        fs::create_directories(savePath.parent_path());

    ofstream file(savePath);
    if (!file.is_open())
    {
        MSG_BOX("Failed to open battle drift save file.");
        return;
    }

    file << jsonData.dump(4);
}

bool CameraPresetIO::LoadBattleDriftFromFile(BattleCameraMode mode, BattleCameraDriftProfile& outProfile, bool isBossVariant)
{
    const string filePath = Build_BattleDriftFilePath(mode, isBossVariant);

    ifstream file(filePath);
    if (!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    BattleCameraDriftProfile profile{};

    profile.enabled = jsonData.value("enabled", profile.enabled);
    profile.lateralAmplitude = jsonData.value("lateralAmplitude", profile.lateralAmplitude);
    profile.verticalAmplitude = jsonData.value("verticalAmplitude", profile.verticalAmplitude);
    profile.depthAmplitude = jsonData.value("depthAmplitude", profile.depthAmplitude);
    profile.rollAmplitude = jsonData.value("rollAmplitude", profile.rollAmplitude);
    profile.lateralFrequency = jsonData.value("lateralFrequency", profile.lateralFrequency);
    profile.verticalFrequency = jsonData.value("verticalFrequency", profile.verticalFrequency);
    profile.depthFrequency = jsonData.value("depthFrequency", profile.depthFrequency);
    profile.rollFrequency = jsonData.value("rollFrequency", profile.rollFrequency);

    outProfile = profile;
    return true;
}

NS_END
