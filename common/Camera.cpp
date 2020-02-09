#include "Camera.h"

using namespace DirectX;

Camera::Camera() : m_isDragged(false)
{
}

void Camera::SetLookAt(XMFLOAT3 vPos, XMFLOAT3 vTarget, XMFLOAT3 vUp)
{
  m_view = XMMatrixLookAtLH(
    XMLoadFloat3(&vPos), XMLoadFloat3(&vTarget), XMLoadFloat3(&vUp)
  );
}

void Camera::OnMouseButtonDown(int buttonType)
{
  m_isDragged = true;
  m_buttonType = buttonType;
}

void Camera::OnMouseButtonUp()
{
  m_isDragged = false;
}

void Camera::OnMouseMove(int dx, int dy)
{
  if (!m_isDragged)
  {
    return;
  }
  if (m_buttonType == 0)
  {
    auto invMat = XMMatrixInverse(nullptr, m_view);
    auto dirRight = invMat.r[0];
    auto matX = XMMatrixRotationAxis(dirRight, dy * -0.01f);
    auto matY = XMMatrixRotationY(dx * -0.01f);
    m_view = matY * matX * m_view;
  }
  if (m_buttonType == 1)
  {
    auto invMat = XMMatrixInverse(nullptr, m_view);
    auto forward = invMat.r[2];
    auto pos = invMat.r[3];
    pos += forward * float(dy*0.1f);
    pos = XMVectorSetW(pos, 1.0);
    invMat.r[3] = pos;
    m_view = XMMatrixInverse(nullptr,invMat);
  }

  if (m_buttonType == 2)
  {
    auto invMat = XMMatrixInverse(nullptr, m_view);
    auto dirRight = invMat.r[0] * float(dx) * -0.05f;
    auto dirUp = invMat.r[1] * float(dy) * 0.05f;
    auto m1 = XMMatrixTranslationFromVector(dirRight);
    auto m2 = XMMatrixTranslationFromVector(dirUp);
    auto m = m1 * m2;
    m_view = m * m_view;
  }

}

XMVECTOR Camera::GetPosition() const
{
  auto m = XMMatrixInverse(nullptr, m_view);
  return m.r[3];
}
