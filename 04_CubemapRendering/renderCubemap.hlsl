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

struct GSOutput
{
  float4 Position : SV_POSITION;
  float4 Color : COLOR;
  float3 Normal : NORMAL;
  uint   RTIndex : SV_RenderTargetArrayIndex;
};

struct SceneParameters
{
  float4x4 viewProj[6];
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


VSOutput mainVS( VSInput In, uint instanceIndex : SV_InstanceID )
{
  VSOutput result = (VSOutput)0;
  float3 lightDir = normalize(sceneConstants.lightDir.xyz);

  result.Position = mul(In.Position, instanceParameters.world[instanceIndex]);
  result.Color.rgb = saturate(dot(In.Normal.xyz, lightDir)) * 0.5 + 0.5;
  result.Color.a = 1.0;
  result.Color *= instanceParameters.color[instanceIndex];

  result.Normal = In.Normal;
  return result;
}

[maxvertexcount(18)]
void mainGS(triangle VSOutput In[3], inout TriangleStream<GSOutput> stream)
{
  for (int f = 0; f < 6; ++f)
  {
    GSOutput v;
    v.RTIndex = f;
    float4x4 mtxVP = sceneConstants.viewProj[f];
    for (int i = 0; i < 3; ++i)
    {
      v.Position = mul(In[i].Position, mtxVP);
      v.Normal = In[i].Normal;
      v.Color = In[i].Color;

      stream.Append(v);
    }
    stream.RestartStrip();
  }
}
float4 mainPS(GSOutput In) : SV_TARGET
{
  return  In.Color;
}
