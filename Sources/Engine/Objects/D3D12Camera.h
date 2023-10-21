#pragma once

#define CAMERA_DEFAULT_FOV XM_PI / 3.0f
#define CAMERA_DEFAULT_ASPECT_RATIO 16.0f / 9.0f
#define CAMERA_DEFAULT_NEAR_Z 0.03f
#define CAMERA_DEFAULT_FAR_Z 1000.0f

using namespace DirectX;

class D3D12Camera : public Transform
{
private:
    CD3DX12_VIEWPORT* pViewport;
    CD3DX12_RECT* pScissorRect;

    FLOAT width;
    FLOAT height;
    FLOAT fov;
    FLOAT aspectRatio;
    FLOAT nearZ;
    FLOAT farZ;

public:
    D3D12Camera(FLOAT width, FLOAT height);

    ~D3D12Camera();

    virtual void ResetTransform() override;

    void SetViewport(const FLOAT width, const FLOAT height);
    void SetScissorRect(const LONG width, const LONG height);

    const D3D12_VIEWPORT* GetViewport() const { return pViewport; }
    const D3D12_RECT* GetScissorRect() const { return pScissorRect; }
    const XMMATRIX GetVPMatrix();
};
