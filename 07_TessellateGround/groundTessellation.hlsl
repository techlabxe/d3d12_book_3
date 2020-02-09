struct VSInput
{
  float4 Position : POSITION;
  float2 UV : TEXCOORD0;
};

struct HSInput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
};

struct DSInput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
};

struct PSInput
{
  float4 Position : SV_POSITION;
  float2 UV : TEXCOORD0;
  float3 Normal : TEXCOORD1;
};

struct SceneParameters
{
  float4  cameraPos;
  float4x4 world;
  float4x4 viewProj;
  float4 tessRange;
};

ConstantBuffer<SceneParameters> sceneConstants : register(b0);
Texture2D texHeightMap : register(t0);
Texture2D texNormalMap : register(t1);
sampler mapSampler: register(s0);

HSInput mainVS( VSInput In )
{
  HSInput result = (HSInput)0;
  float4x4 mtxWVP = mul(sceneConstants.world, sceneConstants.viewProj);

  result.Position = In.Position;
  result.UV = In.UV;
  return result;
}

struct HSParameters
{
  float tessFactor[4] : SV_TessFactor;
  float insideFactor[2] : SV_InsideTessFactor;
};

float CalcTessFactor(float4 v)
{
  float dist = length(mul(v, sceneConstants.world).xyz - sceneConstants.cameraPos.xyz);
  float tessNear = sceneConstants.tessRange.x;
  float tessFar = sceneConstants.tessRange.y;

  const float MaxTessFactor = 32.0;
  float val = MaxTessFactor - (MaxTessFactor - 1) * (dist - tessNear) / (tessFar - tessNear);
  val = max(1, min(MaxTessFactor, val));
  return val;
}
float CalcNormalBias(float3 p, float3 n)
{
  //const float normalThreshold = 0.34; // 約70度.
  //const float normalThreshold = 0.5; // 約60度.
  const float normalThreshold = 0.85; // 約60度.
  float3 camPos = sceneConstants.cameraPos.xyz;
  float3 fromCamera = normalize(p - camPos);
  float cos2 = dot(n, fromCamera);
  cos2 *= cos2;
  float normalFactor = 1.0 - cos2;
  float bias = max(normalFactor - normalThreshold, 0) / (1.0 - normalThreshold);
  return bias * clamp(sceneConstants.tessRange.z, 0, 64);
}

HSParameters ComputePatchConstants(InputPatch<HSInput, 4> patch)
{
  // 分割用パラメータを決定する.
  HSParameters outParams;

  { // 各エッジの中点を求め、その点とカメラの距離で係数を計算する.
    // この中点で法線を取得し、その向きでさらに係数を補正する
    float4 v[4];
    float3 n[4];
    int indices[][2] = { 
      { 2, 0 }, {0, 1}, {1, 3}, {2, 3}
    };

    for (int i = 0; i < 4; ++i)
    {
      int idx0 = indices[i][0];
      int idx1 = indices[i][1];
      v[i] = 0.5 * (patch[idx0].Position + patch[idx1].Position);
      float2 uv = 0.5*(patch[idx0].UV + patch[idx1].UV);
      n[i] = texNormalMap.SampleLevel(mapSampler, uv, 0).xyz;
      n[i] = normalize(n[i] - 0.5);
    }

    outParams.tessFactor[0] =  CalcTessFactor(v[0]);
    outParams.tessFactor[2] =  CalcTessFactor(v[2]);
    outParams.tessFactor[0] += CalcNormalBias(v[0].xyz, n[0].xyz);
    outParams.tessFactor[2] += CalcNormalBias(v[2].xyz, n[2].xyz);
    outParams.insideFactor[0] = 0.5 * (outParams.tessFactor[0] + outParams.tessFactor[2]);

    outParams.tessFactor[1] =  CalcTessFactor(v[1]);
    outParams.tessFactor[3] =  CalcTessFactor(v[3]);
    outParams.tessFactor[1] += CalcNormalBias(v[1].xyz, n[1].xyz);
    outParams.tessFactor[3] += CalcNormalBias(v[3].xyz, n[3].xyz);
    outParams.insideFactor[1] = 0.5 * (outParams.tessFactor[1] + outParams.tessFactor[3]);
  }
  
  return outParams;
}

[domain("quad")]
[partitioning("fractional_even")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(4)]
[patchconstantfunc("ComputePatchConstants")]
DSInput mainHS(InputPatch<HSInput, 4> inPatch, uint pointID : SV_OutputControlPointID)
{
  DSInput result;
  result.Position = inPatch[pointID].Position;
  result.UV = inPatch[pointID].UV;
  return result;
}

[domain("quad")]
PSInput mainDS(HSParameters In, float2 loc : SV_DomainLocation, const OutputPatch<DSInput, 4> patch)
{
  PSInput result;
  float4x4 mtxWVP = mul(sceneConstants.world, sceneConstants.viewProj);

  float3 p0 = lerp(patch[0].Position, patch[1].Position, loc.x).xyz;
  float3 p1 = lerp(patch[2].Position, patch[3].Position, loc.x).xyz;
  float3 pos = lerp(p0, p1, loc.y);

  float2 c0 = lerp(patch[0].UV, patch[1].UV, loc.x);
  float2 c1 = lerp(patch[2].UV, patch[3].UV, loc.x);
  float2 uv = lerp(c0, c1, loc.y);

  float height = texHeightMap.SampleLevel(mapSampler, uv, 0).x;
  pos.y = height*30;

  float3 n = normalize((texNormalMap.SampleLevel(mapSampler, uv, 0).xyz - 0.5));
  result.Normal = n;
  result.UV = uv;
  result.Position = mul(float4(pos.xyz, 1), mtxWVP);

  return result;
}



float4 mainPS(PSInput In) : SV_TARGET
{
  float3 norm = normalize((texNormalMap.SampleLevel(mapSampler, In.UV, 0).xyz - 0.5));

  float3 lightDir = normalize(float3(1, 1, 0));
  float l = saturate(dot(norm, lightDir));

  float3 diffuse = float3(0.1, 0.9, 0.1) * l;
  return float4(diffuse,1);
}

