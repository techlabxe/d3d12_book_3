#define NUM_CONTROL_POINTS 16

struct SceneParameters
{
  float4x4 world;
  float4x4 viewProj;
  float4  cameraPos;
  float4  tessFactor; // x : outside, y: inside
};

ConstantBuffer<SceneParameters> sceneConstants : register(b0);

struct HSParameters
{
  float tessFactor[4] : SV_TessFactor;
  float insideFactor[2] : SV_InsideTessFactor;
};



struct VertexData
{
  float3 pos : POSITION;
};

struct HSInput
{
  float3 pos : POSITION;
};

HSInput mainVS(VertexData input)
{
  HSInput output;
  output.pos = input.pos;

  return output;
}

struct DSInput
{
  float3 pos : POSITION;
};


HSParameters calculatePatchConstants()
{
  HSParameters output;

  float outside = sceneConstants.tessFactor.x;
  float inside = sceneConstants.tessFactor.y;

  output.tessFactor[0] = outside;
  output.tessFactor[1] = outside;
  output.tessFactor[2] = outside;
  output.tessFactor[3] = outside;
  output.insideFactor[0] = inside;
  output.insideFactor[1] = inside;

  return output;
}

[domain("quad")]
[partitioning("integer")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("calculatePatchConstants")]
DSInput mainHS(InputPatch<HSInput, NUM_CONTROL_POINTS> input, uint i : SV_OutputControlPointID)
{
  DSInput output;
  output.pos = input[i].pos;

  return output;
}

struct PSInput
{
  float4 pos : SV_POSITION;
  float3 color : COLOR;
};

float4 bernsteinBasis(float t)
{
  float invT = 1.0f - t;
  return float4(invT * invT * invT,	// (1-t)3
    3.0f * t * invT * invT,			// 3t(1-t)2
    3.0f * t * t * invT,			// 3t2(1-t)
    t * t * t);						// t3
}

float3 CubicInterpolate(float3 p0, float3 p1, float3 p2, float3 p3, float4 t)
{
  return p0 * t.x + p1 * t.y + p2 * t.z + p3 * t.w;
}
float3 CubicTangent(float3 p1, float3 p2, float3 p3, float3 p4, float t)
{
  float T0 = -1 + 2.0 * t - t * t;
  float T1 = 1.0 - 4 * t + 3 * t * t;
  float T2 = 2 * t - 3 * t * t;
  float T3 = t * t;

  return p1 * T0 * 3 + p2 * T1 * 3 + p3 * T2 * 3 + p4 * T3 * 3;
}


[domain("quad")]
PSInput mainDS(
  HSParameters input, 
  float2 loc : SV_DomainLocation, 
  const OutputPatch<DSInput, NUM_CONTROL_POINTS> patch)
{
  float4x4 mtxWVP = mul(sceneConstants.world, sceneConstants.viewProj);

  float4 basisU = bernsteinBasis(loc.x);
  float4 basisV = bernsteinBasis(loc.y);

  // u 方向に関しての４点を求める
  float3 q1 = CubicInterpolate(patch[0].pos,  patch[1].pos,  patch[2].pos,  patch[3].pos, basisU);
  float3 q2 = CubicInterpolate(patch[4].pos,  patch[5].pos,  patch[6].pos,  patch[7].pos, basisU);
  float3 q3 = CubicInterpolate(patch[8].pos,  patch[9].pos,  patch[10].pos, patch[11].pos, basisU);
  float3 q4 = CubicInterpolate(patch[12].pos, patch[13].pos, patch[14].pos, patch[15].pos, basisU);
  
  // その４点から補間し、v での位置を求める.
  float3 localPos = CubicInterpolate(q1, q2, q3, q4, basisV);
  
  //(接線のための) v方向に関して4点を計算.
  float3 r1 = CubicInterpolate(patch[0].pos, patch[4].pos, patch[8].pos,  patch[12].pos, basisV);
  float3 r2 = CubicInterpolate(patch[1].pos, patch[5].pos, patch[9].pos,  patch[13].pos, basisV);
  float3 r3 = CubicInterpolate(patch[2].pos, patch[6].pos, patch[10].pos, patch[14].pos, basisV);
  float3 r4 = CubicInterpolate(patch[3].pos, patch[7].pos, patch[11].pos, patch[15].pos, basisV);

  float3 tangent1 = CubicTangent(r1, r2, r3, r4, loc.x);
  float3 tangent2 = CubicTangent(q1, q2, q3, q4, loc.y);
  float3 normal = cross(tangent1, tangent2);

  // フタと底のためのハック (teapot専用).
  if (length(normal) > 0.000001)
  {
    normal = normalize(normal);
  }
  else
  {
    if (localPos.y < 0.000)
    {
      normal = float3(0, -1, 0);
    }
    else
    {
      normal = float3(0, 1, 0);
    }
  }
  // 計算された位置で透視変換の処理を行う.
  PSInput output;
  output.pos = mul(float4(localPos, 1.0f), mtxWVP);
  output.color = float4(0, 0, 0, 1);
  output.color.xyz = normal.xyz *0.5 + 0.5;
  return output;
}

float4 mainPS(PSInput input) : SV_TARGET
{
  return float4(input.color, 1.0f);
}