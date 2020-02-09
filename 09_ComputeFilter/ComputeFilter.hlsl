struct VSInput
{
  float4 Position : POSITION;
  float4 UV : TEXCOORD0;
};

struct PSInput
{
  float4 Position : SV_POSITION;
  float4 UV : TEXCOORD0;
};

struct SceneParameters
{
  float4x4 proj;
};

ConstantBuffer<SceneParameters> sceneConstants : register(b0);
Texture2D imageTex : register(t0);
SamplerState imageSampler : register(s0);

PSInput mainVS( VSInput In, uint instanceID : SV_InstanceID)
{
  PSInput result = (PSInput)0;

  result.Position = mul(In.Position, sceneConstants.proj);
  result.UV = In.UV;
  return result;
}

float4 mainPS(PSInput In) : SV_TARGET
{
  return imageTex.Sample(imageSampler, In.UV.xy);
}


Texture2D<float4> sourceImage : register(t0);
RWTexture2D<float4> destinationImage : register(u0);

[numthreads(16,16,1)]
void mainSepia( uint3 dtid : SV_DispatchThreadID)
{
  if (dtid.x < 1280 && dtid.y < 720)
  {
    float3x3 toSepia = float3x3(
      0.393, 0.349, 0.272,
      0.769, 0.686, 0.534,
      0.189, 0.168, 0.131);
    float3 color = mul(sourceImage[dtid.xy].xyz, toSepia);
    destinationImage[dtid.xy] = float4(color, 1);
  }
}

[numthreads(1, 1, 1)]
void mainSobel(uint3 dtid : SV_DispatchThreadID)
{
  if (dtid.x < 1280 && dtid.y < 720)
  {
    int k = 0;
    float3 pixels[9];
    for (int y = -1; y <= 1; ++y)
    {
      for (int x = -1; x <= 1; ++x)
      {
        float2 index = dtid.xy;
        index += float2(x, y);

        pixels[k] = sourceImage[index].xyz;
        k++;
      }
    }

    float3 sobelH, sobelV;
    sobelH = pixels[0] * -1 + pixels[2] * 1
      + pixels[3] * -2 + pixels[5] * 2
      + pixels[6] * -1 + pixels[8] * 1;
    sobelV = pixels[0] * -1 + pixels[1] * -2 + pixels[2] * -1
      + pixels[6] * 1 + pixels[7] * 2 + pixels[8] * 1;

    float4 color = float4(sqrt(sobelV * sobelV + sobelH * sobelH), 1);
    destinationImage[dtid.xy] = color;
  }
}

