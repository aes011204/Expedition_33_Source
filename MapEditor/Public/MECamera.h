#pragma once
#include "MapEditorDefines.h"

NS_BEGIN(MapEditor)

class MECamera
{
public:
    MECamera() = default;
    ~MECamera() = default;

public:
    const Matrix& GetView() const { return _view; }
    const Matrix& GetProj() const { return _proj; }
    const Vec3& GetPosition() const { return _position; }
    float GetFar() const { return _farZ; }
    float GetMoveSpeed() const { return _moveSpeed; }
    float GetRotateSensitivity() const { return _rotateSensitivity; }
    float& GetRotateSensitivityRef() { return _rotateSensitivity; }
    float GetYaw() const { return _yaw; }
    float GetPitch() const { return _pitch; }
    void ApplyBookmark(const Vec3& pos, float yaw, float pitch);

    void SetAspect(float aspect) { _aspect = aspect; }

public:
    void Initialize(HWND hWnd, float fov, float aspect, float nearZ, float farZ);
    void UpdateInput(float timeDelta);

private:
    Vec3 _position = Vec3(0.f, 5.f, -10.f);
    float _yaw = {};
    float _pitch = {};

    float _fov = 60.f;
    float _aspect = 16.f / 9.f;
    float _nearZ = 0.1f;
    float _farZ = 1000.f;
    float _moveSpeed = 10.f;

    float _rotateSensitivity = 0.003f;

    bool _rotating = false;
    POINT _savedCursorPos = {};

    Matrix _view = Matrix::Identity;
    Matrix _proj = Matrix::Identity;

    HWND _hWnd{};

private:
    void ComputeMatrices();
    Vec3 GetForward() const;
    Vec3 GetRight() const;

public:
    void Apply();
    void FocusOn(const Vec3& target);

public:
    static Shared<MECamera> Create(HWND hWnd, float fov, float aspect, float nearZ, float farZ);
};

NS_END
