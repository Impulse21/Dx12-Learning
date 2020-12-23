#include "pch.h"
#include "Camera.h"

using namespace Core;
using namespace DirectX;


Camera::Camera()
    : m_viewDirty(true)
    , m_inverseViewDirty(true)
    , m_projectionDirty(true)
    , m_inverseProjectionDirty(true)
    , m_verticalFoV(45.0f)
    , m_aspectRatio(1.0f)
    , m_zNear(0.1f)
    , m_zFar(100.0f)
{
    m_pAllignedData = (AlignedData*)_aligned_malloc(sizeof(AlignedData), 16);
    m_pAllignedData->translation = XMVectorZero();
    m_pAllignedData->rotation = XMQuaternionIdentity();
}

Camera::~Camera()
{
    _aligned_free(m_pAllignedData);
}

void XM_CALLCONV Camera::SetLookAt(FXMVECTOR eye, FXMVECTOR target, FXMVECTOR up)
{
    this->m_pAllignedData->viewMatrix = XMMatrixLookAtLH(eye, target, up);

    this->m_pAllignedData->translation = eye;
    this->m_pAllignedData->rotation = XMQuaternionRotationMatrix(XMMatrixTranspose(m_pAllignedData->viewMatrix));

    this->m_inverseViewDirty = true;
    this->m_viewDirty = false;
}

XMMATRIX Camera::GetViewMatrix() const
{
    if (this->m_viewDirty)
    {
        this->UpdateViewMatrix();
    }

    return m_pAllignedData->viewMatrix;
}

XMMATRIX Camera::GetInverseViewMatrix() const
{
    if (this->m_inverseViewDirty)
    {
        this->m_pAllignedData->inverseViewMatrix = XMMatrixInverse(nullptr, this->m_pAllignedData->viewMatrix);
        this->m_inverseViewDirty = false;
    }

    return this->m_pAllignedData->inverseViewMatrix;
}

void Camera::SetProjection(float fovy, float aspect, float zNear, float zFar)
{
    this->m_verticalFoV = fovy;
    this->m_aspectRatio = aspect;
    this->m_zNear = zNear;
    this->m_zFar = zFar;

    this->m_projectionDirty = true;
    this->m_inverseProjectionDirty = true;
}

XMMATRIX Camera::GetProjectionMatrix() const
{
    if (this->m_projectionDirty)
    {
        this->UpdateProjectionMatrix();
    }

    return this->m_pAllignedData->projectionMatrix;
}

XMMATRIX Camera::GetInverseProjectionMatrix() const
{
    if (this->m_inverseProjectionDirty)
    {
        this->UpdateInverseProjectionMatrix();
    }

    return this->m_pAllignedData->inverseProjectionMatrix;
}

void Camera::SetFoV(float fovy)
{
    if (this->m_verticalFoV != fovy)
    {
        this->m_verticalFoV = fovy;
        this->m_projectionDirty = true;
        this->m_inverseProjectionDirty = true;
    }
}

float Camera::GetFoV() const
{
    return this->m_verticalFoV;
}


void XM_CALLCONV Camera::SetTranslation(FXMVECTOR translation)
{
    this->m_pAllignedData->translation = translation;
    this->m_viewDirty = true;
}

XMVECTOR Camera::GetTranslation() const
{
    return this->m_pAllignedData->translation;
}

void Camera::SetRotation(FXMVECTOR rotation)
{
    this->m_pAllignedData->rotation = rotation;
}

XMVECTOR Camera::GetRotation() const
{
    return this->m_pAllignedData->rotation;
}

void XM_CALLCONV Camera::Translate(FXMVECTOR translation, Space space)
{
    switch (space)
    {
    case Space::Local:
    {
        this->m_pAllignedData->translation += XMVector3Rotate(translation, this->m_pAllignedData->rotation);
    }
    break;
    case Space::World:
    {
        this->m_pAllignedData->translation += translation;
    }
    break;
    }

    this->m_pAllignedData->translation = XMVectorSetW(this->m_pAllignedData->translation, 1.0f);

    this->m_viewDirty = true;
    this->m_inverseViewDirty = true;
}

void Camera::Rotate(FXMVECTOR quaternion)
{
    this->m_pAllignedData->rotation = XMQuaternionMultiply(this->m_pAllignedData->rotation, quaternion);

    this->m_viewDirty = true;
    this->m_inverseViewDirty = true;
}

void Camera::UpdateViewMatrix() const
{
    XMMATRIX rotationMatrix = XMMatrixTranspose(XMMatrixRotationQuaternion(this->m_pAllignedData->rotation));
    XMMATRIX translationMatrix = XMMatrixTranslationFromVector(-(this->m_pAllignedData->translation));

    this->m_pAllignedData->viewMatrix = translationMatrix * rotationMatrix;

    this->m_inverseViewDirty = true;
    this->m_viewDirty = false;
}

void Camera::UpdateInverseViewMatrix() const
{
    if (m_viewDirty)
    {
        this->UpdateViewMatrix();
    }

    this->m_pAllignedData->inverseViewMatrix = XMMatrixInverse(nullptr, this->m_pAllignedData->viewMatrix);
    this->m_inverseViewDirty = false;
}

void Camera::UpdateProjectionMatrix() const
{
    this->m_pAllignedData->projectionMatrix =
        XMMatrixPerspectiveFovLH(
            XMConvertToRadians(this->m_verticalFoV), this->m_aspectRatio, this->m_zNear, this->m_zFar);

    this->m_projectionDirty = false;
    this->m_inverseProjectionDirty = true;
}

void Camera::UpdateInverseProjectionMatrix() const
{
    if (m_projectionDirty)
    {
        UpdateProjectionMatrix();
    }

    this->m_pAllignedData->inverseProjectionMatrix = XMMatrixInverse(nullptr, m_pAllignedData->projectionMatrix);
    this->m_inverseProjectionDirty = false;
}
