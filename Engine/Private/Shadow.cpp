#include "pch.h"
#include "Shadow.h"
#include "ShaderCom.h"
#include "Shader_CBuffer_Define.h"
#include "Camera.h"

HRESULT Shadow::Update_ShadowLight(const ShadowLightDesc& desc)
{
    _desc = desc;

    _viewMatrix = XMMatrixLookAtLH(desc.eye, desc.at, Vec3::Up);
    _projMatrix = XMMatrixPerspectiveFovLH(desc.fovy, desc.aspect, desc.zn, desc.zf);

    return S_OK;
}

HRESULT Shadow::Update_FromCameraFrustum(const ShadowLightDesc& desc, const Shared<Camera> camera, const Vec3& lightDir)
{
    if (camera == nullptr || camera->Get_Transform() == nullptr)
        return E_FAIL;

    _desc = desc;

    Vec3 dir = lightDir;
    if (dir.LengthSquared() < 0.0001f)
        dir = Vec3(1.f, -1.f, -1.f);

    dir.Normalize();

    const float aspect = /* x / y */
        static_cast<float>(GAME->Get_ViewportWidth()) /
        static_cast<float>(max(1u, GAME->Get_ViewportHeight()));

    const float fovY = camera->Get_FovY();
    const float tanHalfFovY = tanf(fovY * 0.5f);

    const float nearZ = max(desc.focusNear, camera->Get_Near());
    const float farZ = max(nearZ + 1.f, min(desc.focusFar, camera->Get_Far()));

    const Matrix cameraWorld = camera->Get_Transform()->Get_WorldMatrix();

    Vec3 corners[8]{};
    uint32 index = 0;
    const float depthList[2] = { nearZ, farZ };

    for (uint32 depthIndex = 0; depthIndex < 2; ++depthIndex)
    {
        const float z = depthList[depthIndex];

        const float halfY = z * tanHalfFovY;
        const float halfX = halfY * aspect;

        const Vec3 viewCorners[4] =
        {
            Vec3(-halfX,  halfY, z),    // left top
            Vec3(halfX,  halfY, z),     // right top
            Vec3(halfX, -halfY, z),     // right bottom
            Vec3(-halfX, -halfY, z),    // left bottom
        };

        for (uint32 cornerIndex = 0; cornerIndex < 4; ++cornerIndex)
        {
            corners[index++] = Vec3::Transform(viewCorners[cornerIndex], cameraWorld);
        }
    }

    Vec3 center = Vec3::Zero;
    for (const Vec3& corner : corners)
        center += corner;

    center /= 8.f;

    float radius = 0.f;
    for (const Vec3& corner : corners)
        radius = max(radius, (corner - center).Length());

    const Vec3 eye = center - dir * (radius + desc.lightDistancePadding);

    Vec3 up = Vec3::Up;
    if (fabsf(dir.Dot(up)) > 0.95f)
        up = Vec3::Forward;

    _viewMatrix = XMMatrixLookAtLH(eye, center, up);

    Vec3 minLS(FLT_MAX, FLT_MAX, FLT_MAX);
    Vec3 maxLS(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const Vec3& corner : corners)
    {
        const Vec3 lightSpace = Vec3::Transform(corner, _viewMatrix);
        minLS = Vec3::Min(minLS, lightSpace);
        maxLS = Vec3::Max(maxLS, lightSpace);
    }

    minLS.x -= desc.xyPadding;
    maxLS.x += desc.xyPadding;
    minLS.y -= desc.xyPadding;
    maxLS.y += desc.xyPadding;
    minLS.z -= desc.zPadding;
    maxLS.z += desc.zPadding;

    const float extentX = max(1.f, maxLS.x - minLS.x);
    const float extentY = max(1.f, maxLS.y - minLS.y);

    const float texelX = extentX / static_cast<float>(max(1u, desc.shadowMapWidth));
    const float texelY = extentY / static_cast<float>(max(1u, desc.shadowMapHeight));

    // Shadow shimmering 완화용 texel snapping.
    minLS.x = floorf(minLS.x / texelX) * texelX;
    maxLS.x = ceilf(maxLS.x / texelX) * texelX;
    minLS.y = floorf(minLS.y / texelY) * texelY;
    maxLS.y = ceilf(maxLS.y / texelY) * texelY;

    if (maxLS.z <= minLS.z)
        maxLS.z = minLS.z + 1.f;

    _projMatrix = XMMatrixOrthographicOffCenterLH(
        minLS.x,
        maxLS.x,
        minLS.y,
        maxLS.y,
        minLS.z,
        maxLS.z
    );

    _desc.eye = eye;
    _desc.at = center;
    _desc.zn = minLS.z;
    _desc.zf = maxLS.z;

    return S_OK;
}

HRESULT Shadow::Bind_TransformStateMatrix(Shared<ShaderCom> shader, const char* viewName, const char* projName)
{
    CHECK_NULL(shader, E_FAIL);
    CHECK_FAILED(shader->Bind_Matrix(viewName, &_viewMatrix), E_FAIL);
    CHECK_FAILED(shader->Bind_Matrix(projName, &_projMatrix), E_FAIL);

    return S_OK;
}

HRESULT Shadow::Bind_ShadowCB(Shared<ShaderCom> shader)
{
    CHECK_NULL(shader, E_FAIL);

    ShadowCB shadowCB{};
    shadowCB.lightViewMatrix = _viewMatrix;
    shadowCB.lightProjMatrix = _projMatrix;
    shadowCB.lightFar = _desc.zf;
    shadowCB.bias = _desc.bias;
    shadowCB.intensity = _desc.intensity;

    return shader->Bind_CBufferData(shadowCB);
}

HRESULT Shadow::Bind_ShadowLightDesc(Shared<ShaderCom> shader, const char* lightFarName, const char* biasName, const char* intensityName)
{
    if (lightFarName)   
        CHECK_FAILED(shader->Bind_RawValue(lightFarName, &_desc.zf, sizeof _desc.zf), E_FAIL);
    if (biasName)
        CHECK_FAILED(shader->Bind_RawValue(biasName, &_desc.bias, sizeof _desc.bias), E_FAIL);
    if (intensityName)
        CHECK_FAILED(shader->Bind_RawValue(intensityName, &_desc.intensity, sizeof _desc.intensity), E_FAIL);

    return S_OK;
}

void Shadow::Clear()
{
    _viewMatrix = Matrix::Identity;
    _projMatrix = Matrix::Identity;
}

Unique<Shadow> Shadow::Create()
{
    return make_unique<Shadow>();
}

void Shadow::Free()
{
    Base::Free();
}

const Matrix* Shadow::Get_ViewMatrix() const
{
    return &_viewMatrix;
}

const Matrix* Shadow::Get_ProjMatrix() const
{
    return &_projMatrix;
}

const ShadowLightDesc* Shadow::Get_ShadowLightDesc() const
{
    return &_desc;
}
