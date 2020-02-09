#pragma once
#include "D3D12AppBase.h"
#include "DirectXMath.h"
#include <DirectXPackedVector.h>
#include "Camera.h"

#include <array>
#include <unordered_map>

class ComputeFilterApp : public D3D12AppBase {
public:
  ComputeFilterApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();

private:
  void CreateRootSignatures();
  void PrepareComputeFilter();
  void PrepareSimpleModel();
  void PreparePipeline();

  void RenderToMain();
  void RenderHUD();

  using Buffer = ComPtr<ID3D12Resource1>;

  struct Vertex
  {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 UV;
  };
  
  struct TextureData
  {
    Texture texture;
    DescriptorHandle handleRead;
    DescriptorHandle handleWrite;
  };
  TextureData LoadTextureFromFile(const std::wstring& name);

  ModelData m_quad, m_quad2;

  struct SceneParameters
  {
    DirectX::XMFLOAT4X4 proj;
  };
  using PipelineState = ComPtr<ID3D12PipelineState>;

  ComPtr<ID3D12RootSignature> m_rootSignature;
  std::unordered_map<std::string, PipelineState> m_pipelines;

  std::vector<Buffer> m_mainSceneCB;

  ComPtr<ID3D12RootSignature> m_csSignature;
  TextureData m_texture;
  TextureData m_uavTexture;

  enum Mode
  {
    Mode_Sepia, 
    Mode_Sobel,
  };
  Mode m_mode;
};