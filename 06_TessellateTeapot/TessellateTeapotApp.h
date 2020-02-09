#pragma once
#include "D3D12AppBase.h"
#include "DirectXMath.h"
#include "Camera.h"

#include <array>
#include <unordered_map>

class TessellateTeapotApp : public D3D12AppBase {
public:
  TessellateTeapotApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();

  virtual void OnMouseButtonDown(UINT msg);
  virtual void OnMouseButtonUp(UINT msg);
  virtual void OnMouseMove(UINT msg, int dx, int dy);
private:
  void CreateRootSignatures();
  void PrepareTessellateTeapot();
  void PreparePipeline();

  void RenderToMain();
  void RenderHUD();

  DirectX::XMMATRIX GetProjectionMatrix(float fov, float aspect, float znear, float zfar);


  Camera m_camera;

  struct SceneParameters
  {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT4   cameraPos;
    DirectX::XMFLOAT4   tessFactor; // x: outside, y: inside
  };

  using PipelineState = ComPtr<ID3D12PipelineState>;
  std::unordered_map<std::string, PipelineState> m_pipelines;

  ComPtr<ID3D12RootSignature> m_rootSigunature;

  using Buffer = ComPtr<ID3D12Resource1>;
  std::vector<Buffer> m_mainSceneCB;

  float m_tessFactor;
  ModelData m_tessTeapot;
  bool m_isWireframe;
};