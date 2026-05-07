#pragma once
#include "Camera.h"

NS_BEGIN(ModelEditor)

class Camera_Editor final : public Engine::Camera
{
public:
	Camera_Editor(const ComPtr<Device>& device, const ComPtr<Context>& context);
	Camera_Editor(const Camera_Editor& prototype);
	~Camera_Editor() override = default;

public:
	HRESULT Initialize_Prototype() override;
	HRESULT Initialize(void* arg) override;
	void Priority_Update(float timeDelta) override;
	void Update(float timeDelta) override;
	void Late_Update(float timeDelta) override;
	HRESULT Render() override;

private:
	float _camSpeed = 10.f;
	float _mouseSensitivity = 0.1f;

public:
	Shared<GameObject> Clone(void* arg) override;
	void Free() override;
	static Shared<Camera_Editor> Create(const ComPtr<Device>& device, const ComPtr<Context>& context);
};

NS_END
