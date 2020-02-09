#include "TessellateGroundApp.h"

#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

#include <DirectXTex.h>
#include <fstream>

using namespace std;
using namespace DirectX;

TessellateGroundApp::TessellateGroundApp()  
{
  m_camera.SetLookAt(
    XMFLOAT3(60.0f, 50.0f, 60.0f),
    XMFLOAT3(0.0f, 12.0f, 0.0f)
  );

  m_isWireframe = false;
  m_tessRangeNear = 16.0f;
  m_tessRangeFar = 100.0f;
  m_tessRangeNormalFactor = 4.0f;
}

void TessellateGroundApp::Prepare()
{
  SetTitle("Ground Tessellation");
  CreateRootSignatures();

  m_commandList->Reset(m_commandAllocators[0].Get(), nullptr);
  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(1, heaps);
  m_commandList->Close();

  PrepareGroundPatch();
  PreparePipeline();

  auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(
    sizeof(MainSceneParameters)
  );
  m_mainSceneCB = CreateConstantBuffers(cbDesc);

  m_heightMap = LoadTextureFromFile(L"heightmap.png");
  m_normalMap = LoadTextureFromFile(L"normalmap.png");
}

void TessellateGroundApp::CreateRootSignatures()
{
  // RootSignature
  array<CD3DX12_ROOT_PARAMETER, 3> rootParams;
  array<CD3DX12_DESCRIPTOR_RANGE, 2> texParams;
  rootParams[0].InitAsConstantBufferView(0);

  rootParams[1].InitAsDescriptorTable(1, &texParams[0]);
  rootParams[2].InitAsDescriptorTable(1, &texParams[1]);

  texParams[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
  texParams[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);

  array<CD3DX12_STATIC_SAMPLER_DESC, 1> samplerDesc;
  samplerDesc[0].Init(0);


  CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
  rootSignatureDesc.Init(
    UINT(rootParams.size()), rootParams.data(),
    UINT(samplerDesc.size()), samplerDesc.data(),
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> signature, errBlob;
  D3D12SerializeRootSignature(&rootSignatureDesc,
    D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
  m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
}

void TessellateGroundApp::PreparePipeline()
{
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  std::vector<wstring> flags;
  std::vector<Shader::DefineMacro> defines;
  defines.push_back(
    Shader::DefineMacro{ L"PATCH_QUAD", L"1" }
  );

  Shader shaderVS, shaderPS, shaderHS, shaderDS;
  shaderVS.load(L"groundTessellation.hlsl", Shader::Vertex, L"mainVS", flags, defines);
  shaderPS.load(L"groundTessellation.hlsl", Shader::Pixel, L"mainPS", flags, defines);
  shaderHS.load(L"groundTessellation.hlsl", Shader::Hull, L"mainHS", flags, defines);
  shaderDS.load(L"groundTessellation.hlsl", Shader::Domain, L"mainDS", flags, defines);


  auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  auto psoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    rasterizerState,
    inputElementDesc, _countof(inputElementDesc),
    m_rootSignature.Get(),
    shaderVS.getCode(),
    shaderPS.getCode(),
    nullptr,
    shaderHS.getCode(),
    shaderDS.getCode()
  );
  psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;

  HRESULT hr;
  PipelineState pipeline;
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed.");
  m_pipelines["default"] = pipeline;
  
  psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed.");
  m_pipelines["wireframe"] = pipeline;
}

void TessellateGroundApp::Cleanup()
{
}

void TessellateGroundApp::OnMouseButtonDown(UINT msg)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseButtonDown(msg);
}
void TessellateGroundApp::OnMouseButtonUp(UINT msg)
{
  m_camera.OnMouseButtonUp();
}

void TessellateGroundApp::OnMouseMove(UINT msg, int dx, int dy)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseMove(dx, dy);
}



void TessellateGroundApp::PrepareGroundPatch()
{
  using VertexData = std::vector<Vertex>;
  using IndexData = std::vector<UINT>;
  const float edge = 200.0f;
  const int divide = 10;
  std::vector<Vertex> vertices;
  for (int z = 0; z < divide+1; ++z)
  {
    for (int x = 0; x < divide+1; ++x)
    {
      Vertex v;
      v.Position = XMFLOAT3(
        edge * x / divide,
        0.0f,
        edge * z / divide
      );
      v.UV = XMFLOAT2(
        v.Position.x / edge,
        v.Position.z / edge
      );
      vertices.push_back(v);
    }
  }
  std::vector<UINT> indices;

  for (int z = 0; z < divide; ++z) {
    for (int x = 0; x < divide; ++x) {
      const UINT rows = divide + 1;
      UINT v0 = x, v1 = x + 1;

      v0 = v0 + rows * z;
      v1 = v1 + rows * z;
      indices.push_back(v0 + rows);
      indices.push_back(v1 + rows);
      indices.push_back(v0);
      indices.push_back(v1);
    }
  }
  // 中心補正
  for (auto& v : vertices)
  {
    v.Position.x -= edge * 0.5f;
    v.Position.z -= edge * 0.5f;
  }
  m_ground = CreateSimpleModel(vertices, indices);
}

void TessellateGroundApp::Render()
{
  m_frameIndex = m_swapchain->GetCurrentBackBufferIndex();
  m_commandAllocators[m_frameIndex]->Reset();
  m_commandList->Reset(
    m_commandAllocators[m_frameIndex].Get(), nullptr
  );

  // スワップチェイン表示可能からレンダーターゲット描画可能へ
  auto barrierToRT = m_swapchain->GetBarrierToRenderTarget();
  m_commandList->ResourceBarrier(1, &barrierToRT);

  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(_countof(heaps), heaps);

  RenderToMain();

  RenderImGui();

  // レンダーターゲットからスワップチェイン表示可能へ
  {
    auto barrierToPresent = m_swapchain->GetBarrierToPresent();
    CD3DX12_RESOURCE_BARRIER barriers[] = {
      barrierToPresent,
    };

    m_commandList->ResourceBarrier(_countof(barriers), barriers);
  }
  m_commandList->Close();
  ID3D12CommandList* lists[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);
}

void TessellateGroundApp::RenderToMain()
{
  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // カラーバッファ(レンダーターゲットビュー)のクリア
  float m_clearColor[4] = { 0.1f,0.5f,0.75f,0 };
  m_commandList->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);

  // デプスバッファ(デプスステンシルビュー)のクリア
  m_commandList->ClearDepthStencilView(
    dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // 描画先をセット
  m_commandList->OMSetRenderTargets(1, &(D3D12_CPU_DESCRIPTOR_HANDLE)rtv,
    FALSE, &(D3D12_CPU_DESCRIPTOR_HANDLE)dsv);

  // ビューポートとシザーのセット
  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  auto sceneCB = m_mainSceneCB[m_frameIndex];
  auto mtxView = m_camera.GetViewMatrix();
  auto mtxProj = GetProjectionMatrix(45.0f, float(m_width) / float(m_height), 0.1f, 500.0f);

  MainSceneParameters sceneParams{};
  XMStoreFloat4x4(&sceneParams.world, XMMatrixIdentity());
  XMStoreFloat4x4(&sceneParams.viewProj, XMMatrixTranspose(mtxView * mtxProj));
  XMStoreFloat4(&sceneParams.cameraPos, m_camera.GetPosition());
  sceneParams.tessRange.x = m_tessRangeNear;
  sceneParams.tessRange.y = m_tessRangeFar;
  sceneParams.tessRange.z = m_tessRangeNormalFactor;

  WriteToUploadHeapMemory(sceneCB.Get(), sizeof(sceneParams), &sceneParams);


  m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  m_commandList->SetGraphicsRootConstantBufferView(0, sceneCB->GetGPUVirtualAddress());
  m_commandList->SetGraphicsRootDescriptorTable(1, m_heightMap.handle);
  m_commandList->SetGraphicsRootDescriptorTable(2, m_normalMap.handle);

  if (m_isWireframe)
  {
    m_commandList->SetPipelineState(m_pipelines["wireframe"].Get());
  }
  else
  {
    m_commandList->SetPipelineState(m_pipelines["default"].Get());
  }

  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
  m_commandList->IASetVertexBuffers(0, 1, &m_ground.vbView);
  m_commandList->IASetIndexBuffer(&m_ground.ibView);
  m_commandList->DrawIndexedInstanced(m_ground.indexCount, 1, 0, 0, 0);
}

void TessellateGroundApp::RenderImGui()
{
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  auto framerate = ImGui::GetIO().Framerate;
  ImGui::Begin("Information");
  ImGui::Text("Framerate %.3f ms", 1000.0f / framerate);
  XMFLOAT3 cameraPos;
  XMStoreFloat3(&cameraPos, m_camera.GetPosition());
  ImGui::Text("CameraPos (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);

  ImGui::Checkbox("WireFrame", &m_isWireframe);
  ImGui::Spacing();
  ImGui::InputFloat("RangeNear", &m_tessRangeNear, 0.5f, 5.0f, "%.1f");
  ImGui::InputFloat("RangeFar", &m_tessRangeFar, 0.5f, 5.0f, "%.1f");
  ImGui::InputFloat("NormalFactor", &m_tessRangeNormalFactor, 0.1f, 0.2f, "%.1f");
  ImGui::End();

  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}

DirectX::XMMATRIX TessellateGroundApp::GetProjectionMatrix(float fov, float aspect, float znear, float zfar)
{
  auto mtxProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspect, znear, zfar);
  return mtxProj;
}

TessellateGroundApp::TextureData TessellateGroundApp::LoadTextureFromFile(const std::wstring& name)
{
  DirectX::TexMetadata metadata;
  DirectX::ScratchImage image;

  HRESULT hr;
  if (name.find(L".png") != std::wstring::npos)
  {
    hr = DirectX::LoadFromWICFile(name.c_str(), 0, &metadata, image);
  }
  if (name.find(L".dds") != std::wstring::npos)
  {
    hr = DirectX::LoadFromDDSFile(name.c_str(), 0, &metadata, image);
  }
  if (name.find(L".tga") != std::wstring::npos)
  {
    hr = DirectX::LoadFromTGAFile(name.c_str(), &metadata, image);
  }

  ComPtr<ID3D12Resource> texture;
  CreateTexture(m_device.Get(), metadata, &texture);

  Buffer srcBuffer;
  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  PrepareUpload(m_device.Get(), image.GetImages(), image.GetImageCount(), metadata, subresources);
  const auto totalBytes = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));

  auto staging = CreateResource(CD3DX12_RESOURCE_DESC::Buffer(totalBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);
  auto command = CreateCommandList();
  UpdateSubresources(command.Get(),
    texture.Get(), staging.Get(), 0, 0, UINT(subresources.size()), subresources.data());
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    texture.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  command->ResourceBarrier(1, &barrier);
  FinishCommandList(command);

  TextureData texData;
  texture.As(&texData.texture);
  texData.handle = m_heap->Alloc();

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = metadata.format;
  srvDesc.TextureCube.MipLevels = UINT(metadata.mipLevels);
  srvDesc.TextureCube.MostDetailedMip = 0;
  srvDesc.TextureCube.ResourceMinLODClamp = 0;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  m_device->CreateShaderResourceView(texture.Get(), &srvDesc, texData.handle);

  return texData;
}
