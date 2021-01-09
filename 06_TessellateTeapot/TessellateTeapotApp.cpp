#include "TessellateTeapotApp.h"
#include "TeapotModel.h"

#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

#include <DirectXTex.h>
#include <fstream>

#include "TeapotPatch.h"

using namespace std;
using namespace DirectX;

TessellateTeapotApp::TessellateTeapotApp()  
{
  m_camera.SetLookAt(
    XMFLOAT3(1.37f, 1.11f, 0.94f),
    XMFLOAT3(0.0f, 0.0f, 0.0f)
  );

  m_isWireframe = true;
  m_tessFactor = 2.0f;
}

void TessellateTeapotApp::Prepare()
{
  SetTitle("Tessellate Teapot");

  m_commandList->Reset(m_commandAllocators[0].Get(), nullptr);
  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(1, heaps);
  m_commandList->Close();
  
  CreateRootSignatures();
  PrepareTessellateTeapot();
  PreparePipeline();

  auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(
    sizeof(SceneParameters)
  );
  m_mainSceneCB = CreateConstantBuffers(cbDesc);
}

void TessellateTeapotApp::CreateRootSignatures()
{
  // RootSignature
  array<CD3DX12_ROOT_PARAMETER, 1> rootParams;
  rootParams[0].InitAsConstantBufferView(0);

  CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
  rootSignatureDesc.Init(
    UINT(rootParams.size()), rootParams.data(),
    0, nullptr,
    D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> signature, errBlob;
  D3D12SerializeRootSignature(&rootSignatureDesc,
    D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
  m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSigunature));

}

void TessellateTeapotApp::Cleanup()
{
}

void TessellateTeapotApp::OnMouseButtonDown(UINT msg)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseButtonDown(msg);
}
void TessellateTeapotApp::OnMouseButtonUp(UINT msg)
{
  m_camera.OnMouseButtonUp();
}

void TessellateTeapotApp::OnMouseMove(UINT msg, int dx, int dy)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseMove(dx, dy);
}

void TessellateTeapotApp::Render()
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

  RenderHUD();

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

void TessellateTeapotApp::RenderToMain()
{
  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // カラーバッファ(レンダーターゲットビュー)のクリア
  //float m_clearColor[4] = { 0.5f,0.75f,1.0f,0 };
  float m_clearColor[4] = { 0.25f,0.25f,0.25f,0 };
  m_commandList->ClearRenderTargetView(rtv, m_clearColor, 0, nullptr);

  // デプスバッファ(デプスステンシルビュー)のクリア
  m_commandList->ClearDepthStencilView(
    dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

  // 描画先をセット
  D3D12_CPU_DESCRIPTOR_HANDLE handleRtvs[] = { rtv };
  D3D12_CPU_DESCRIPTOR_HANDLE handleDsv = dsv;
  m_commandList->OMSetRenderTargets(1, handleRtvs, FALSE, &handleDsv);

  // ビューポートとシザーのセット
  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  auto sceneCB = m_mainSceneCB[m_frameIndex];
  auto mtxView = m_camera.GetViewMatrix();
  auto mtxProj = GetProjectionMatrix(45.0f, float(m_width) / float(m_height), 0.1f, 100.0f);

  SceneParameters sceneParams{};
  XMStoreFloat4x4(&sceneParams.world, XMMatrixIdentity());
  XMStoreFloat4x4(&sceneParams.viewProj, XMMatrixTranspose(mtxView * mtxProj));
  XMStoreFloat4(&sceneParams.cameraPos, m_camera.GetPosition());
  sceneParams.tessFactor.x = m_tessFactor;
  sceneParams.tessFactor.y = m_tessFactor;

  WriteToUploadHeapMemory(sceneCB.Get(), sizeof(sceneParams), &sceneParams);


  m_commandList->SetGraphicsRootSignature(m_rootSigunature.Get());
  m_commandList->SetGraphicsRootConstantBufferView(0, sceneCB->GetGPUVirtualAddress());
  
  if (m_isWireframe)
  {
    m_commandList->SetPipelineState(m_pipelines["wireframe"].Get());
  }
  else
  {
    m_commandList->SetPipelineState(m_pipelines["default"].Get());
  }

  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST);
  m_commandList->IASetVertexBuffers(0, 1, &m_tessTeapot.vbView);
  m_commandList->IASetIndexBuffer(&m_tessTeapot.ibView);
  m_commandList->DrawIndexedInstanced(m_tessTeapot.indexCount, 1, 0, 0, 0);
}

void TessellateTeapotApp::RenderHUD()
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

  ImGui::SliderFloat("Tessfactor", &m_tessFactor, 1.0f, 32.0f);
  ImGui::Checkbox("WireFrame", &m_isWireframe);
  ImGui::End();

  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}


DirectX::XMMATRIX TessellateTeapotApp::GetProjectionMatrix(float fov, float aspect, float znear, float zfar)
{
  auto mtxProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspect, znear, zfar);
  return mtxProj;
}

void TessellateTeapotApp::PrepareTessellateTeapot()
{
  auto teapotPoints = TeapotPatch::GetTeapotPatchPoints();
  auto teapotIndices = TeapotPatch::GetTeapotPatchIndices();
  m_tessTeapot = CreateSimpleModel(teapotPoints, teapotIndices);
}

void TessellateTeapotApp::PreparePipeline()
{
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  std::vector<wstring> flags;
  std::vector<Shader::DefineMacro> defines;

  Shader shaderVS, shaderPS, shaderHS, shaderDS;
  shaderVS.load(L"teapotTessellation.hlsl", Shader::Vertex, L"mainVS", flags, defines);
  shaderPS.load(L"teapotTessellation.hlsl", Shader::Pixel, L"mainPS", flags, defines);
  shaderHS.load(L"teapotTessellation.hlsl", Shader::Hull, L"mainHS", flags, defines);
  shaderDS.load(L"teapotTessellation.hlsl", Shader::Domain, L"mainDS", flags, defines);


  auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  rasterizerState.CullMode = D3D12_CULL_MODE_BACK;

  auto psoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    rasterizerState,
    inputElementDesc, _countof(inputElementDesc),
    m_rootSigunature.Get(),
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



