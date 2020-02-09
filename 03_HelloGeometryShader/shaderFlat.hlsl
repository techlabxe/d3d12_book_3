struct VSInput
{
  float4 Position : POSITION;
  float3 Normal : NORMAL;
};

struct GSInput
{
  float4 Position : POSITION;
  float4 Color : COLOR;
};
struct GSOutput
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

GSInput mainVS(VSInput In)
{
  GSInput result = (GSInput)0;
  result.Position = float4(In.Position.xyz, 1);
  result.Color = float4(1, 1, 1, 1);
  return result;
}

[maxvertexcount(3)]
void mainGS(
  triangle GSInput In[3],
  inout TriangleStream<GSOutput> stream)
{
  float4 v0 = In[0].Position;
  float4 v1 = In[1].Position;
  float4 v2 = In[2].Position;

  // 面を構成する２辺を求める.
  float3 e1 = normalize(v1 - v0).xyz;
  float3 e2 = normalize(v2 - v0).xyz;
  
  // 外積により面法線を求める.
  float3 normal = normalize(cross(e1, e2));

  // 求めた法線とでライティング計算.
  float4 color = saturate(dot(normal, normalize(lightdir.xyz)));

  float4x4 mtxVP = mul(view, proj);

  for (int i = 0; i < 3; ++i)
  {
    GSOutput v;
    v.Position = mul(In[i].Position, mtxVP);
    v.Color = color;
    stream.Append(v);
  }
  stream.RestartStrip();
}

float4 mainPS(GSOutput In) : SV_Target
{
  return In.Color;
}
