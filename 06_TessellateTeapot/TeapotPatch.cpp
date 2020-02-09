#include "TeapotPatch.h"
#include <DirectXMath.h>

using namespace TeapotPatch;
using namespace DirectX;

#include "TeapotPatch2.inc"

std::vector<XMFLOAT3> TeapotPatch::GetTeapotPatchPoints()
{
  std::vector<XMFLOAT3> controls(teapot_points, teapot_points + _countof(teapot_points));
  return controls;
}

std::vector<unsigned int> TeapotPatch::GetTeapotPatchIndices()
{
  std::vector<unsigned int> indices(teapot_patches, teapot_patches + _countof(teapot_patches));
  return indices;
}


