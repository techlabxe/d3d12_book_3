struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
};

struct PSInput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
  float3 Reflect : TEXCOORD0;
};

struct SceneParameters
{
  float4x4 world;
  float4x4 viewProj;
  float4  cameraPos;
  float4  lightDir;
};

ConstantBuffer<SceneParameters> sceneConstants : register(b0);
TextureCube texCube : register(t0);
SamplerState samp : register(s0);


PSInput mainVS( VSInput In )
{
  PSInput result = (PSInput)0;
  float4x4 mtxWVP = mul(sceneConstants.world, sceneConstants.viewProj);
  float3 lightDir = normalize(sceneConstants.lightDir.xyz);

  result.Position = mul(In.Position, mtxWVP);
  result.Color.rgb = saturate(dot(In.Normal.xyz, lightDir)) * 0.5 + 0.5;
  result.Color.a = 1.0;

  float3 eyeDir = normalize(In.Position.xyz - sceneConstants.cameraPos.xyz);
  result.Reflect = reflect(eyeDir, In.Normal.xyz);

  return result;
}

float4 mainPS(PSInput In) : SV_TARGET
{
  return In.Color * texCube.Sample(samp,In.Reflect);
}
