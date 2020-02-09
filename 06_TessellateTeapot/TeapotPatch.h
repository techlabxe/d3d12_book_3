#pragma once

#include <DirectXMath.h>
#include <vector>

namespace TeapotPatch
{
  using DirectX::XMFLOAT3;
  struct ControlPoint {
    XMFLOAT3 Position;
  };

  std::vector<XMFLOAT3> GetTeapotPatchPoints();
  std::vector<unsigned int> GetTeapotPatchIndices();
}
