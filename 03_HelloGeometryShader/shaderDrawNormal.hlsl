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

[maxvertexcount(2)]
void mainGS(
  triangle GSInput In[3],
  inout LineStream<GSOutput> stream)
{
  float4 v0 = In[0].Position;
  float4 v1 = In[1].Position;
  float4 v2 = In[2].Position;

  // ñ Çç\ê¨Ç∑ÇÈÇQï”ÇãÅÇﬂÇÈ.
  float3 e1 = normalize(v1 - v0).xyz;
  float3 e2 = normalize(v2 - v0).xyz;
  
  // äOêœÇ…ÇÊÇËñ ñ@ê¸ÇãÅÇﬂÇÈ.
  float3 normal = normalize(cross(e1, e2));

  float3 center = (v0.xyz + v1.xyz + v2.xyz) / 3.0;
  float4x4 mtxVP = mul(view, proj);

  for (int i = 0; i < 2; ++i)
  {
    GSOutput v;
    float4 p = float4(center, 1);
    p.xyz += normal * 0.1 * i;
    v.Position = mul(p, mtxVP);
    v.Color = float4(0, 0, 1, 1);
    stream.Append(v);
  }
  stream.RestartStrip();
}

float4 mainPS(GSOutput In) : SV_Target
{
  return In.Color;
}
