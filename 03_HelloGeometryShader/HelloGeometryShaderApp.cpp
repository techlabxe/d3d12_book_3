#include "HelloGeometryShaderApp.h"
#include "TeapotModel.h"

#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

#include <DirectXTex.h>
#include <fstream>

using namespace std;
using namespace DirectX;

HelloGeometryShaderApp::HelloGeometryShaderApp()
{
  m_camera.SetLookAt(
    XMFLOAT3(4.0f, 3.0f, 2.0f),
    XMFLOAT3(0.0f, 0.0f, 0.0f)
  );
}

void HelloGeometryShaderApp::CreateRootSignatures()
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
  m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature));
}

void HelloGeometryShaderApp::Prepare()
{
  SetTitle("HelloGeometryShader");
  CreateRootSignatures();

  m_commandList->Reset(m_commandAllocators[0].Get(), nullptr);
  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(1, heaps);
  m_commandList->Close();

  PrepareTeapot();
  PreparePipeline();
}

void HelloGeometryShaderApp::Cleanup()
{
}

void HelloGeometryShaderApp::OnMouseButtonDown(UINT msg)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseButtonDown(msg);
}
void HelloGeometryShaderApp::OnMouseButtonUp(UINT msg)
{
  m_camera.OnMouseButtonUp();
}

void HelloGeometryShaderApp::OnMouseMove(UINT msg, int dx, int dy)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseMove(dx, dy);
}

void HelloGeometryShaderApp::PrepareTeapot()
{
  std::vector<TeapotModel::Vertex> vertices(std::begin(TeapotModel::TeapotVerticesPN), std::end(TeapotModel::TeapotVerticesPN));
  std::vector<UINT> indices(std::begin(TeapotModel::TeapotIndices), std::end(TeapotModel::TeapotIndices));
  m_model = CreateSimpleModel(vertices, indices);

  auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(ShaderParameters));
  m_sceneParameterCB = CreateConstantBuffers(cbDesc);
}

void HelloGeometryShaderApp::PreparePipeline()
{
  ComPtr<ID3DBlob> errBlob;

  auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
  rasterizerState.FrontCounterClockwise = true;

  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  HRESULT hr;
  // 通常モデル描画のパイプラインの構築.
  {
    ComPtr<ID3D12PipelineState> pipelineState;
    Shader shaderVS, shaderPS;
    std::vector<wstring> flags;
    std::vector<Shader::DefineMacro> defines;

    shaderVS.load(L"shaderDefault.hlsl", Shader::Vertex, L"mainVS", flags, defines);
    shaderPS.load(L"shaderDefault.hlsl", Shader::Pixel, L"mainPS", flags, defines);

    auto psoDesc = book_util::CreateDefaultPsoDesc(
      DXGI_FORMAT_R8G8B8A8_UNORM,
      rasterizerState,
      inputElementDesc, _countof(inputElementDesc),
      m_rootSignature,
      shaderVS.getCode(), shaderPS.getCode()
    );

    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed.");
    m_pipelines["drawTeapot"] = pipelineState;
  }

  // フラットシェーディングパイプラインの構築.
  {
    ComPtr<ID3D12PipelineState> pipelineState;
    Shader shaderVS, shaderPS, shaderGS;
    std::vector<wstring> flags;
    std::vector<Shader::DefineMacro> defines;

    shaderVS.load(L"shaderFlat.hlsl", Shader::Vertex, L"mainVS", flags, defines);
    shaderGS.load(L"shaderFlat.hlsl", Shader::Geometry, L"mainGS", flags, defines);
    shaderPS.load(L"shaderFlat.hlsl", Shader::Pixel, L"mainPS", flags, defines);

    auto psoDesc = book_util::CreateDefaultPsoDesc(
      m_surfaceFormat,
      rasterizerState,
      inputElementDesc, _countof(inputElementDesc),
      m_rootSignature,
      shaderVS.getCode(), shaderPS.getCode(), shaderGS.getCode()
    );

    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed.");
    m_pipelines["drawFlat"] = pipelineState;  }

  // 法線描画用パイプラインの構築.
  {
    ComPtr<ID3D12PipelineState> pipelineState;
    Shader shaderVS, shaderPS, shaderGS;
    std::vector<wstring> flags;
    std::vector<Shader::DefineMacro> defines;

    shaderVS.load(L"shaderDrawNormal.hlsl", Shader::Vertex, L"mainVS", flags, defines);
    shaderGS.load(L"shaderDrawNormal.hlsl", Shader::Geometry, L"mainGS", flags, defines);
    shaderPS.load(L"shaderDrawNormal.hlsl", Shader::Pixel, L"mainPS", flags, defines);

    auto psoDesc = book_util::CreateDefaultPsoDesc(
      m_surfaceFormat,
      rasterizerState,
      inputElementDesc, _countof(inputElementDesc),
      m_rootSignature,
      shaderVS.getCode(), shaderPS.getCode(), shaderGS.getCode()
    );

    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed.");
    m_pipelines["drawNormalLine"] = pipelineState;
  }
}


void HelloGeometryShaderApp::Render()
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

  m_scenePatameters.lightDir = XMFLOAT4(0.0f, 20.0f,20.0f, 0.0f);
  auto mtxProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(45.0f), float(m_width) / float(m_height), 0.1f, 100.0f);
  XMStoreFloat4x4(&m_scenePatameters.view, XMMatrixTranspose(m_camera.GetViewMatrix()));
  XMStoreFloat4x4(&m_scenePatameters.proj, XMMatrixTranspose(mtxProj));
  

  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // カラーバッファ(レンダーターゲットビュー)のクリア
  float m_clearColor[4] = { 0.5f,0.75f,1.0f,0 };
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

  auto imageIndex = m_swapchain->GetCurrentBackBufferIndex();
  m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  WriteToUploadHeapMemory(m_sceneParameterCB[imageIndex].Get(), sizeof(ShaderParameters), &m_scenePatameters);
  m_commandList->SetGraphicsRootConstantBufferView(0, m_sceneParameterCB[imageIndex]->GetGPUVirtualAddress());

  if (m_mode == DrawMode_Flat)
  {
    m_commandList->SetPipelineState(m_pipelines["drawFlat"].Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
    m_commandList->IASetIndexBuffer(&m_model.ibView);
    m_commandList->DrawIndexedInstanced(m_model.indexCount, 1, 0, 0, 0);
  }

  if( m_mode == DrawMode_NormalVector )
  {
    m_commandList->SetPipelineState(m_pipelines["drawTeapot"].Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
    m_commandList->IASetIndexBuffer(&m_model.ibView);
    m_commandList->DrawIndexedInstanced(m_model.indexCount, 1, 0, 0, 0);

    m_commandList->SetPipelineState(m_pipelines["drawNormalLine"].Get());
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
    m_commandList->IASetIndexBuffer(&m_model.ibView);
    m_commandList->DrawIndexedInstanced(m_model.indexCount, 1, 0, 0, 0);
  }

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

void HelloGeometryShaderApp::RenderHUD()
{
  // ImGui
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  // ImGui ウィジェットを描画する.
  auto framerate = ImGui::GetIO().Framerate;
  ImGui::Begin("Information");
  ImGui::Text("Framerate %.3f ms", 1000.0f / framerate);
  ImGui::Combo("Mode", (int*)&m_mode, "Flat\0NormalVector\0\0");
  ImGui::End();

  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}

