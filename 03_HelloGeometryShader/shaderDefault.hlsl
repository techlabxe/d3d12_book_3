struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
};

struct PSInput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
};

cbuffer SceneParameter : register(b0)
{
  float4x4 view;
  float4x4 proj;
  float4   lightdir;
}

PSInput mainVS(VSInput In)
{
  PSInput result = (PSInput)0;
  float4x4 mtxVP = mul(view, proj);

  result.Position = mul(float4(In.Position.xyz, 1), mtxVP);
  result.Color.rgb = saturate(dot(In.Normal.xyz, normalize(lightdir.xyz)));
  result.Color.a = 1.0;
   return result;
}

float4 mainPS(PSInput In) : SV_TARGET
{
  return In.Color;
}
