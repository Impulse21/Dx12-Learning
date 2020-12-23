#pragma once
#include <DirectXMath.h>

namespace Core
{
    // When performing transformations on the camera, 
    // it is sometimes useful to express which space this 
    // transformation should be applied.
    enum class Space
    {
        Local,
        World,
    };

    class Camera
    {
    public:
        Camera();
        virtual ~Camera();

        void XM_CALLCONV SetLookAt(DirectX::FXMVECTOR eye, DirectX::FXMVECTOR target, DirectX::FXMVECTOR up);
        DirectX::XMMATRIX GetViewMatrix() const;
        DirectX::XMMATRIX GetInverseViewMatrix() const;

        /**
         * Set the camera to a perspective projection matrix.
         * @param fovy The vertical field of view in degrees.
         * @param aspect The aspect ratio of the screen.
         * @param zNear The distance to the near clipping plane.
         * @param zFar The distance to the far clipping plane.
         */
        void SetProjection(float fovy, float aspect, float zNear, float zFar);
        DirectX::XMMATRIX GetProjectionMatrix() const;
        DirectX::XMMATRIX GetInverseProjectionMatrix() const;

        /**
         * Set the field of view in degrees.
         */
        void SetFoV(float fovy);

        /**
         * Get the field of view in degrees.
         */
        float GetFoV() const;

        /**
         * Set the camera's position in world-space.
         */
        void XM_CALLCONV SetTranslation(DirectX::FXMVECTOR translation);
        DirectX::XMVECTOR GetTranslation() const;

        /**
         * Set the camera's rotation in world-space.
         * @param rotation The rotation quaternion.
         */
        void XM_CALLCONV SetRotation(DirectX::FXMVECTOR rotation);

        /**
         * Query the camera's rotation.
         * @returns The camera's rotation quaternion.
         */
        DirectX::XMVECTOR GetRotation() const;

        void XM_CALLCONV Translate(DirectX::FXMVECTOR translation, Space space = Space::Local);
        void Rotate(DirectX::FXMVECTOR quaternion);

    protected:
        virtual void UpdateViewMatrix() const;
        virtual void UpdateInverseViewMatrix() const;
        virtual void UpdateProjectionMatrix() const;
        virtual void UpdateInverseProjectionMatrix() const;

        // This data must be aligned otherwise the SSE intrinsics fail
        // and throw exceptions.
        __declspec(align(16)) struct AlignedData
        {
            // World-space position of the camera.
            DirectX::XMVECTOR translation;
            // World-space rotation of the camera.
            // THIS IS A QUATERNION!!!!
            DirectX::XMVECTOR rotation;

            DirectX::XMMATRIX viewMatrix, inverseViewMatrix;
            DirectX::XMMATRIX projectionMatrix, inverseProjectionMatrix;
        };
        AlignedData* m_pAllignedData;

        // projection parameters
        float m_verticalFoV;   // Vertical field of view.
        float m_aspectRatio; // Aspect ratio
        float m_zNear;      // Near clip distance
        float m_zFar;       // Far clip distance.

        // True if the view matrix needs to be updated.
        mutable bool m_viewDirty, m_inverseViewDirty;
        // True if the projection matrix needs to be updated.
        mutable bool m_projectionDirty, m_inverseProjectionDirty;

    private:

    };
}