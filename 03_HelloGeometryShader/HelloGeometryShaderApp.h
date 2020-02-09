#pragma once
#include "D3D12AppBase.h"
#include "DirectXMath.h"
#include "Camera.h"

#include <array>
#include <unordered_map>

class HelloGeometryShaderApp : public D3D12AppBase {
public:
  HelloGeometryShaderApp();

  virtual void Prepare();
  virtual void Cleanup();
  virtual void Render();

  virtual void OnMouseButtonDown(UINT msg);
  virtual void OnMouseButtonUp(UINT msg);
  virtual void OnMouseMove(UINT msg, int dx, int dy);

  struct ShaderParameters
  {
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 proj;
    DirectX::XMFLOAT4 lightDir;
  };
  ShaderParameters m_scenePatameters;

private:
  void CreateRootSignatures();
  void PrepareTeapot();
  void PreparePipeline();

  void RenderHUD();
private:
  ModelData m_model;
  Camera m_camera;

  ComPtr<ID3D12RootSignature> m_rootSignature;
  std::vector<Buffer> m_sceneParameterCB;

  using PipelineState = ComPtr<ID3D12PipelineState>;
  std::unordered_map<std::string, PipelineState> m_pipelines;

  enum DrawMode
  {
    DrawMode_Flat,
    DrawMode_NormalVector,
  };
  DrawMode m_mode;
};