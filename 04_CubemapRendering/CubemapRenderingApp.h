#pragma once
#include "D3D12AppBase.h"
#include "DirectXMath.h"
#include "Camera.h"

#include <unordered_map>

class CubemapRenderingApp : public D3D12AppBase {
public:
  CubemapRenderingApp();

  virtual void Prepare();
  virtual void Cleanup();

  virtual void Render();

  virtual void OnMouseButtonDown(UINT msg);
  virtual void OnMouseButtonUp(UINT msg);
  virtual void OnMouseMove(UINT msg, int dx, int dy);
private:
  void CreateRootSignatures();
  void PrepareTeapot();
  void PrepareSceneResource();
  void PrepareRenderCubemap();
  void CreatePipelines();

  void RenderToMain();
  void RenderHUD();
  void RenderToEachFace();
  void RenderToCubemapSinglePass();

  void SetInfoQueueFilter();

  struct StaticCubeTexture
  {
    ComPtr<ID3D12Resource1> resource;
    DescriptorHandle descriptorSRV;
  };
  StaticCubeTexture LoadCubeTextureFromFile(const std::wstring& fileName);

  DirectX::XMMATRIX GetViewMatrix(int faceIndex);
  DirectX::XMMATRIX GetProjectionMatrix(float fov, float aspect, float znear, float zfar);
private:
  using Buffer = ComPtr<ID3D12Resource1>;
  using Texture = ComPtr<ID3D12Resource1>;

  ModelData m_model;
  Camera m_camera;

  using RootSignature = ComPtr<ID3D12RootSignature>;
  std::unordered_map<std::string, RootSignature> m_rootSignatures;
  using PipelineState = ComPtr<ID3D12PipelineState>;
  std::unordered_map<std::string, PipelineState> m_pipelines;

  StaticCubeTexture m_staticCubemap;

  Texture m_renderCubemap;
  Texture m_renderCubemapDepth;

  DescriptorHandle m_cubeFaceRTV[6]; // each face.
  DescriptorHandle m_cubemapRTV; // whole.
  DescriptorHandle m_renderCubemapSRV;

  DescriptorHandle m_renderCubemapDSV;
  DescriptorHandle m_cubeFaceDSV[6]; // each face.

  struct SceneParameters
  {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT4 cameraPos;
    DirectX::XMFLOAT4 lightDir;
  };
  struct FaceSceneParameters
  {
    DirectX::XMFLOAT4X4 world;
    DirectX::XMFLOAT4X4 viewProj;
    DirectX::XMFLOAT4   cameraPos;
    DirectX::XMFLOAT4 lightDir;
  };
  struct CubeSceneParameters
  {
    DirectX::XMFLOAT4X4 viewProj[6];
    DirectX::XMFLOAT4   cameraPos;
    DirectX::XMFLOAT4   lightDir;
  };

  struct TeapotInstanceParameter
  {
    DirectX::XMFLOAT4X4 world[6];
    DirectX::XMFLOAT4 color[6];
  };

  const int InstanceCount = 6;
  const int CubeMapEdge = 512;

  Buffer m_teapotInstanceParameters;

  CD3DX12_VIEWPORT m_cubemapViewport;
  CD3DX12_RECT m_cubemapScissor;

  std::vector<Buffer> m_renderCubemapFacesCB;
  std::vector<Buffer> m_renderCubemapCB;
  std::vector<Buffer> m_renderMainCB;

  DirectX::XMVECTOR m_lightDirection;

  enum Mode {
    Mode_StaticCubemap,
    Mode_MultiPassCubemap,
    Mode_SinglePassCubemap,
  };
  Mode m_mode;
};