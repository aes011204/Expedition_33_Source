#pragma once

#include "Client_Defines.h"
#include "Camera.h"

NS_BEGIN(Client)

class FreeCamera final : public Camera
{
    GENERATED_GAMEOBJECT(FreeCamera, ObjectType::FreeCamera)

public:
	typedef struct tagFreeCamera final : public Camera::CAMERA_DESC
	{
	}FREECAMERA_DESC;

public:
	FreeCamera(const ComPtr<Device>& device, const ComPtr<Context>& context);
	FreeCamera(const FreeCamera& prototype);
	virtual ~FreeCamera() = default;

public:
	virtual HRESULT Initialize_Prototype() override;
	virtual HRESULT Initialize(void* arg) override;
	virtual void    Priority_Update(float timeDelta) override;
	virtual void    Update(float timeDelta) override;
	virtual void    Late_Update(float timeDelta) override;
	virtual HRESULT Render() override;

private:
    bool    _firstUpdate = true;
    bool    _lock = false;

public:
	static Shared<FreeCamera> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
	virtual Shared<GameObject> Clone(void* arg) override;
	virtual void Free() override;

};

NS_END

//public:
//struct Preset
//{
//    Vec3 offset{};
//    Vec3 lookAtOffset{};
//    float followSpeed = 5.f;
//    float fovy = XMConvertToRadians(60.f);
//    float nearPlane = 0.1f;
//    float farPlane = 500.f;
//};
//
//public:
//Preset MakePreset() const;
//void ApplyPreset(const Preset& preset);

//#include <fstream>
//#include <filesystem>
//
//namespace fs = std::filesystem;
//
//bool SaveTargetCameraPreset(
//    const fs::path& path,
//    const TargetCamera::Preset& preset)
//{
//    json data = TargetCameraPresetToJson(preset);
//
//    fs::create_directories(path.parent_path());
//
//    std::ofstream file(path);
//    if (!file.is_open())
//        return false;
//
//    file << data.dump(4);
//    return true;
//}
