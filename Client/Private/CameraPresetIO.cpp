#include "CameraPresetIO.h"
#include "TargetCamera.h"

CameraPresetIO::CameraPresetIO()
{
}

CameraPresetIO::~CameraPresetIO()
{
}

void CameraPresetIO::SaveTargetPresetToFile(TargetCameraMode cameraMode, const TargetCameraPreset& preset)
{
    json jsonData {};

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
    jsonData["rollOffset"]        = preset.rollOffset;
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

void CameraPresetIO::SaveBattlePresetToFile(BattleCameraMode cameraMode, const BattleCameraPreset& preset)
{
    json jsonData {};

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
    jsonData["rollOffset"]        = preset.rollOffset;
    //jsonData["allowMouseRotation"] = preset.allowMouseRotation;

    const string name = string(magic_enum::enum_name(cameraMode));

    const string filePath = "../../../Client/Bin/Resources/Data/json/CameraPreset/BattlePreset_" + name + ".json";

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

bool CameraPresetIO::LoadBattlePresetFromFile(BattleCameraMode cameraMode, BattleCameraPreset& outPreset)
{
    const string name = string(magic_enum::enum_name(cameraMode));

    const string filePath = "../../../Client/Bin/Resources/Data/json/CameraPreset/BattlePreset_" + name + ".json";

    ifstream file(filePath);
    if (!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    BattleCameraPreset preset;

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
    //preset.allowMouseRotation = jsonData.value("allowMouseRotation",false);
    outPreset = preset;


    return true;
    
}
