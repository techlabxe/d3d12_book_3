struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
};

struct VSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
  float3 Normal : NORMAL;
};

struct SceneParameters
{
  float4x4 world;
  float4x4 viewProj;
  float4  cameraPos;
  float4  lightDir;
};
struct InstanceParameters
{
  float4x4 world[6];
  float4   color[6];
};

ConstantBuffer<SceneParameters> sceneConstants : register(b0);
ConstantBuffer<InstanceParameters> instanceParameters : register(b1);

VSOutput mainVS( VSInput In, uint instanceIndex : SV_InstanceID)
{
  VSOutput result = (VSOutput)0;
  float4x4 world = instanceParameters.world[instanceIndex];
  float4x4 mtxWVP = mul(world, sceneConstants.viewProj);
  float3 lightDir = normalize(sceneConstants.lightDir.xyz);
  
  result.Position = mul(In.Position, mtxWVP);
  result.Color.rgb = saturate(dot(In.Normal.xyz, lightDir)) * 0.5 + 0.5;
  result.Color.a = 1.0;
  result.Color *= instanceParameters.color[instanceIndex];

  result.Normal = In.Normal;
  return result;
}

float4 mainPS(VSOutput In) : SV_TARGET
{
  return In.Color;
}