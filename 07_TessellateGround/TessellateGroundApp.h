#pragma once
#include "D3D12AppBase.h"
#include "DirectXMath.h"
#include "Camera.h"

#include <array>
#include <unordered_map>

class TessellateGroundApp : public D3D12AppBase {
public:
  TessellateGroundApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();

  virtual void OnMouseButtonDown(UINT msg);
  virtual void OnMouseButtonUp(UINT msg);
  virtual void OnMouseMove(UINT msg, int dx, int dy);
private:
  void CreateRootSignatures();
  void PrepareGroundPatch();
  void PreparePipeline();

  void RenderToMain();
  void RenderImGui();

  DirectX::XMMATRIX GetProjectionMatrix(float fov, float aspect, float znear, float zfar);
  struct TextureData
  {
    Texture texture;
    DescriptorHandle handle;
  };
  TextureData LoadTextureFromFile(const std::wstring& name);


  using Buffer = ComPtr<ID3D12Resource1>;

  struct Vertex
  {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT2 UV;
  };
  ModelData m_ground;

  Camera m_camera;

  struct MainSceneParameters
  {
    DirectX::XMFLOAT4   cameraPos;
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT4   tessRange;
  };

  ComPtr<ID3D12RootSignature> m_rootSignature;

  using PipelineState = ComPtr<ID3D12PipelineState>;
  std::unordered_map<std::string, PipelineState> m_pipelines;

  std::vector<Buffer> m_mainSceneCB;

  TextureData m_heightMap;
  TextureData m_normalMap;
  bool m_isWireframe;
  float m_tessRangeNear, m_tessRangeFar;
  float m_tessRangeNormalFactor;

};