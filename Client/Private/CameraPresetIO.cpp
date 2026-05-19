#include "CameraPresetIO.h"
#include "TargetCamera.h"

CameraPresetIO::CameraPresetIO()
{
}

CameraPresetIO::~CameraPresetIO()
{
}

void CameraPresetIO::SavePresetToFile(TargetCamera::CAMERA_STATE cameraState, const TargetCamera::TARGETCAMERA_DESC& preset)
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
    jsonData["mouseSensor"]     = preset.mouseSensor;
    jsonData["mouseSensitivity"]= preset.mouseSensitivity;
    jsonData["yawOffset"]       = preset.yawOffset;
    jsonData["pitchOffset"]     = preset.pitchOffset;
    jsonData["rollOffset"]        = preset.rollOffset;
    jsonData["allowMouseRotation"] = preset.allowMouseRotation;

    const string name = string(magic_enum::enum_name(cameraState));

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


bool CameraPresetIO::LoadPresetFromFile(TargetCamera::CAMERA_STATE cameraState, TargetCamera::TARGETCAMERA_DESC& outPreset)
{
    const string name = string(magic_enum::enum_name(cameraState));

    const string filePath = "../../../Client/Bin/Resources/Data/json/CameraPreset/CameraPreset_" + name + ".json";

    ifstream file(filePath);
    if (!file.is_open())
        return false;

    json jsonData{};
    file >> jsonData;

    TargetCamera::TARGETCAMERA_DESC preset;

    preset.followSpeed = jsonData.value("followSpeed", preset.followSpeed);
    preset.aimOffset = jsonData.value("aimOffset", preset.aimOffset);
    preset.pivotHeight  = jsonData.value("pivotHeight", preset.pivotHeight);
    preset.sideOffset  = jsonData.value("sideOffset", preset.sideOffset);
    preset.distance  = jsonData.value("distance", preset.distance);
    preset.heightOffset  = jsonData.value("heightOffset", preset.heightOffset);
    preset.lookHeight  = jsonData.value("lookHeight", preset.lookHeight);
    preset.pitchLookScale  = jsonData.value("pitchLookScale", preset.pitchLookScale);
    preset.pitchMax  = jsonData.value("pitchMax", preset.pitchMax);
    preset.pitchMin  = jsonData.value("pitchMin", preset.pitchMin);
    preset.mouseSensor = jsonData.value("mouseSensor",preset.mouseSensor);
    preset.mouseSensitivity = jsonData.value("mouseSensitivity", preset.mouseSensitivity);
    preset.yawOffset = jsonData.value("yawOffset", 0.f);
    preset.pitchOffset = jsonData.value ("pitchOffset", 0.f);
    preset.rollOffset = jsonData.value("rollOffset", 0.f);
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

    preset.lookOffset.x = jsonData["lookOffset"].value("x", 0);
    preset.lookOffset.y = jsonData["lookOffset"].value("y", 0);
    preset.lookOffset.z = jsonData["lookOffset"].value("z", 0);
    preset.followSpeed = jsonData.value("followSpeed", 0.f);
    preset.distance = jsonData.value("distance", 0.f);

    outPreset = preset;

    return true;
}
