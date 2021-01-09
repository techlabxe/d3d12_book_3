#include "CubemapRenderingApp.h"
#include "TeapotModel.h"

#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

#include <DirectXTex.h>
#include <array>
#include <fstream>

using namespace std;
using namespace DirectX;

CubemapRenderingApp::CubemapRenderingApp()  
{
  m_camera.SetLookAt(
    XMFLOAT3(-8.0f, 6.0f, 12.0f),
    XMFLOAT3(0.0f, 0.0f, 0.0f)
  );
  m_mode = Mode_StaticCubemap;
  const auto dir = XMFLOAT3(1.0f, 1.0f, 1.0f);
  m_lightDirection = XMVector3Normalize(XMLoadFloat3(&dir));
}

void CubemapRenderingApp::Prepare()
{
  SetTitle("CubemapRendering");

  m_commandList->Reset(m_commandAllocators[0].Get(), nullptr);
  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(1, heaps);
  m_commandList->Close();
  
  CreateRootSignatures();
  SetInfoQueueFilter();

  PrepareTeapot();
  PrepareSceneResource();

  CreatePipelines();
}

void CubemapRenderingApp::Cleanup()
{
}

void CubemapRenderingApp::OnMouseButtonDown(UINT msg)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseButtonDown(msg);
}
void CubemapRenderingApp::OnMouseButtonUp(UINT msg)
{
  m_camera.OnMouseButtonUp();
}

void CubemapRenderingApp::OnMouseMove(UINT msg, int dx, int dy)
{
  auto io = ImGui::GetIO();
  if (io.WantCaptureMouse)
  {
    return;
  }
  m_camera.OnMouseMove(dx, dy);
}

void CubemapRenderingApp::CreateRootSignatures()
{
  // キューブマップ描画時に使用する RootSignature.
  {
    std::array<CD3DX12_ROOT_PARAMETER, 2> rootParams;
    rootParams[0].InitAsConstantBufferView(0);
    rootParams[1].InitAsConstantBufferView(1);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.Init(
      UINT(rootParams.size()), rootParams.data(),
      0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
    );
    ComPtr<ID3DBlob> signature, errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1_0,
      &signature, &errBlob);
    ThrowIfFailed(hr, "D3D12SerializeRootSignature failed.");

    RootSignature rootSignature;
    hr = m_device->CreateRootSignature(
      0,
      signature->GetBufferPointer(),
      signature->GetBufferSize(),
      IID_PPV_ARGS(&rootSignature)
    );
    ThrowIfFailed(hr, "CreateRootSignature failed.");
    m_rootSignatures["teapots"] = rootSignature;
    rootSignature->SetName(L"teapots");
  }

  // キューブマップ参照して描画するための RootSignature.
  {
    array<CD3DX12_ROOT_PARAMETER, 2> rootParams;
    array<D3D12_DESCRIPTOR_RANGE, 1> texRanges;
    texRanges[0].NumDescriptors = 1;
    texRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    texRanges[0].RegisterSpace = 0;
    texRanges[0].OffsetInDescriptorsFromTableStart = 0;
    texRanges[0].BaseShaderRegister = 0;

    rootParams[0].InitAsConstantBufferView(0);
    rootParams[1].InitAsDescriptorTable(UINT(texRanges.size()), texRanges.data());

    array<CD3DX12_STATIC_SAMPLER_DESC, 1> samplerDesc;
    samplerDesc[0].Init(0);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Init(
      UINT(rootParams.size()), rootParams.data(),
      UINT(samplerDesc.size()), samplerDesc.data(),
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature, errBlob;
    HRESULT hr = D3D12SerializeRootSignature(&rootSignatureDesc,
      D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
    ThrowIfFailed(hr, "D3D12SerializeRootSignature failed.");

    RootSignature rootSignature;
    hr = m_device->CreateRootSignature(
      0, 
      signature->GetBufferPointer(), 
      signature->GetBufferSize(), 
      IID_PPV_ARGS(&rootSignature));
    ThrowIfFailed(hr, "CreateRootSignature failed.");
    m_rootSignatures["default"] = rootSignature;
    rootSignature->SetName(L"default");
  }
}

void CubemapRenderingApp::PrepareTeapot()
{
  std::vector<TeapotModel::Vertex> vertices(std::begin(TeapotModel::TeapotVerticesPN), std::end(TeapotModel::TeapotVerticesPN));
  std::vector<UINT> indices(std::begin(TeapotModel::TeapotIndices), std::end(TeapotModel::TeapotIndices));
  m_model = CreateSimpleModel(vertices, indices);
}

void CubemapRenderingApp::PrepareSceneResource()
{
  // 静的なキューブマップの準備.
  m_staticCubemap = LoadCubeTextureFromFile(L"yokohama_cube.dds");

  // 描画先となるキューブマップの準備.
  PrepareRenderCubemap();

  // 周辺オブジェクト配置用の定数バッファの準備.
  // 毎フレーム書き換えることはしないため、バッファリングをしない.
  auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(TeapotInstanceParameter));
  m_teapotInstanceParameters = CreateResource(vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);
  HRESULT hr;
  void* p = nullptr;
  hr = m_teapotInstanceParameters->Map(0, nullptr, &p);
  if (SUCCEEDED(hr))
  {
    TeapotInstanceParameter params;
    XMStoreFloat4x4(&params.world[0], XMMatrixTranspose(XMMatrixTranslation(5.0f, 0.0f, 0.0f)));
    XMStoreFloat4x4(&params.world[1], XMMatrixTranspose(XMMatrixTranslation(-5.0f, 0.0f, 0.0f)));
    XMStoreFloat4x4(&params.world[2], XMMatrixTranspose(XMMatrixTranslation(0.0f, 0.0f, 5.0f)));
    XMStoreFloat4x4(&params.world[3], XMMatrixTranspose(XMMatrixTranslation(0.0f, 0.0f, -5.0f)));
    XMStoreFloat4x4(&params.world[4], XMMatrixTranspose(XMMatrixTranslation(0.0f, 5.0f, 0.0f)));
    XMStoreFloat4x4(&params.world[5], XMMatrixTranspose(XMMatrixTranslation(0.0f, -5.0f, 0.0f)));

    params.color[0] = XMFLOAT4(0.6f, 1.0f, 0.6f, 1.0f);
    params.color[1] = XMFLOAT4(0.0f, 0.75f, 1.0f, 1.0f);
    params.color[2] = XMFLOAT4(1.0f, 0.1f, 0.6f, 1.0f);
    params.color[3] = XMFLOAT4(1.0f, 0.55f, 0.0f, 1.0f);
    params.color[4] = XMFLOAT4(0.0f, 0.5f, 1.0f, 1.0f);
    params.color[5] = XMFLOAT4(0.5f, 0.5f, 0.25f, 1.0f);

    memcpy(p, &params, sizeof(params));
    m_teapotInstanceParameters->Unmap(0, nullptr);
  }

  // 定数バッファの準備.
  CD3DX12_RESOURCE_DESC cbDesc;
  cbDesc = CD3DX12_RESOURCE_DESC::Buffer(
    sizeof(CubeSceneParameters)
  );
  m_renderCubemapCB = CreateConstantBuffers(cbDesc);

  cbDesc = CD3DX12_RESOURCE_DESC::Buffer(
    sizeof(FaceSceneParameters)
  );
  m_renderCubemapFacesCB = CreateConstantBuffers(cbDesc, FrameBufferCount * 6);

  cbDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(SceneParameters));
  m_renderMainCB = CreateConstantBuffers(cbDesc);
}


void CubemapRenderingApp::PrepareRenderCubemap()
{
  auto desc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    CubeMapEdge, CubeMapEdge, 6, 1, 1, 0,
    D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
  const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  HRESULT hr = m_device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &desc,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    nullptr,
    IID_PPV_ARGS(&m_renderCubemap)
  );

  // Cubemap としての RTV.
  {
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = desc.Format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Texture2DArray.ArraySize = 6;
    rtvDesc.Texture2DArray.FirstArraySlice = 0;
    m_cubemapRTV = m_heapRTV->Alloc();
    m_device->CreateRenderTargetView(
      m_renderCubemap.Get(),
      &rtvDesc,
      m_cubemapRTV
    );
  }

  // 各フェイス毎の RTV を準備.
  for (int i = 0; i < desc.ArraySize(); ++i)
  {
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = desc.Format;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
    rtvDesc.Texture2DArray.FirstArraySlice = i;
    rtvDesc.Texture2DArray.ArraySize = 1;
    auto handle = m_heapRTV->Alloc();
    m_device->CreateRenderTargetView(
      m_renderCubemap.Get(),
      &rtvDesc,
      handle
    );
    m_cubeFaceRTV[i] = handle;
  }

  auto cubeDepthDesc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_D32_FLOAT,
    CubeMapEdge, CubeMapEdge, 6, 1, 1, 0,
    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
  CD3DX12_CLEAR_VALUE clearDepthValue{};
  clearDepthValue.Format = cubeDepthDesc.Format;
  clearDepthValue.DepthStencil.Depth = 1.0f;
  
  hr = m_device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &cubeDepthDesc,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    nullptr,
    IID_PPV_ARGS(&m_renderCubemapDepth)
  );
  // Cubemap としての DSV.
  {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = cubeDepthDesc.Format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.ArraySize = 6;
    dsvDesc.Texture2DArray.FirstArraySlice = 0;
    m_renderCubemapDSV = m_heapDSV->Alloc();
    m_device->CreateDepthStencilView(
      m_renderCubemapDepth.Get(),
      &dsvDesc,
      m_renderCubemapDSV
    );
  }
  // Cubemap 各フェイス毎の DSV を準備.
  for (int i = 0; i < cubeDepthDesc.DepthOrArraySize; ++i)
  {
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = cubeDepthDesc.Format;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.ArraySize = 1;
    dsvDesc.Texture2DArray.FirstArraySlice = i;
    m_cubeFaceDSV[i] = m_heapDSV->Alloc();
    m_device->CreateDepthStencilView(
      m_renderCubemapDepth.Get(),
      &dsvDesc,
      m_cubeFaceDSV[i]
    );
  }

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = desc.Format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
  srvDesc.Texture2DArray.ArraySize = desc.ArraySize();
  srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  m_renderCubemapSRV = m_heap->Alloc();
  m_device->CreateShaderResourceView(
    m_renderCubemap.Get(),
    &srvDesc,
    m_renderCubemapSRV
  );

  m_cubemapViewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(CubeMapEdge), float(CubeMapEdge));
  m_cubemapScissor = CD3DX12_RECT(0, 0, LONG(CubeMapEdge), LONG(CubeMapEdge));
}

void CubemapRenderingApp::CreatePipelines()
{
  HRESULT hr;
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  // メイン描画. キューブマップテクスチャを参照するパイプライン.
  {
    std::vector<wstring> flags;
    std::vector<Shader::DefineMacro> defines;

    Shader shaderVS, shaderPS;
    shaderVS.load(L"shaderDefault.hlsl", Shader::Vertex, L"mainVS", flags, defines);
    shaderPS.load(L"shaderDefault.hlsl", Shader::Pixel, L"mainPS", flags, defines);

    auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerState.FrontCounterClockwise = true;

    auto psoDesc = book_util::CreateDefaultPsoDesc(
      DXGI_FORMAT_R8G8B8A8_UNORM,
      rasterizerState,
      inputElementDesc, _countof(inputElementDesc),
      m_rootSignatures["default"],
      shaderVS.getCode(), shaderPS.getCode()
    );

    PipelineState pipeline;
    hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
    ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");
    m_pipelines["default"] = pipeline;
    pipeline->SetName(L"default");
  }

  // 各面描画用パイプライン.
  {
    std::vector<wstring> flags;
    std::vector<Shader::DefineMacro> defines;
    Shader renderFaceVS, renderFacePS;
    renderFaceVS.load(L"renderCubeFace.hlsl", Shader::Vertex, L"mainVS", flags, defines);
    renderFacePS.load(L"renderCubeFace.hlsl", Shader::Pixel, L"mainPS", flags, defines);

    auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    auto psoDesc = book_util::CreateDefaultPsoDesc(
      DXGI_FORMAT_R8G8B8A8_UNORM,
      rasterizerState,
      inputElementDesc, _countof(inputElementDesc),
      m_rootSignatures["teapots"],
      renderFaceVS.getCode(), renderFacePS.getCode()
    );

    PipelineState pipeline;
    hr = m_device->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&pipeline)
    );
    ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");
    m_pipelines["cubeface"] = pipeline;
    pipeline->SetName(L"MultiPass PSO");
  }

  // メイン描画、周囲ティーポット描画用パイプライン.
  {
    std::vector<wstring> flags;
    std::vector<Shader::DefineMacro> defines;
    Shader renderFaceVS, renderFacePS;
    renderFaceVS.load(L"renderCubeFace.hlsl", Shader::Vertex, L"mainVS", flags, defines);
    renderFacePS.load(L"renderCubeFace.hlsl", Shader::Pixel, L"mainPS", flags, defines);

    auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    auto psoDesc = book_util::CreateDefaultPsoDesc(
      DXGI_FORMAT_R8G8B8A8_UNORM,
      rasterizerState,
      inputElementDesc, _countof(inputElementDesc),
      m_rootSignatures["teapots"],
      renderFaceVS.getCode(), renderFacePS.getCode()
    );

    PipelineState pipeline;
    hr = m_device->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&pipeline)
    );
    ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");
    m_pipelines["teapots"] = pipeline;
    pipeline->SetName(L"main Teapots PSO");
  }

  // キューブマップ、シングルパス描画用パイプライン.
  {
    std::vector<wstring> flags;
    std::vector<Shader::DefineMacro> defines;
    Shader renderCubemapVS, renderCubemapGS, renderCubemapPS;
    renderCubemapVS.load(L"renderCubemap.hlsl", Shader::Vertex, L"mainVS", flags, defines);
    renderCubemapGS.load(L"renderCubemap.hlsl", Shader::Geometry, L"mainGS", flags, defines);
    renderCubemapPS.load(L"renderCubemap.hlsl", Shader::Pixel, L"mainPS", flags, defines);

    auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

    auto psoDesc = book_util::CreateDefaultPsoDesc(
      DXGI_FORMAT_R8G8B8A8_UNORM,
      rasterizerState,
      inputElementDesc, _countof(inputElementDesc),
      m_rootSignatures["teapots"],
      renderCubemapVS.getCode(), renderCubemapPS.getCode(), renderCubemapGS.getCode()
    );

    PipelineState pipeline;
    hr = m_device->CreateGraphicsPipelineState(
      &psoDesc, IID_PPV_ARGS(&pipeline)
    );
    ThrowIfFailed(hr, "CreateGraphicsPipelineState failed.");
    m_pipelines["singleCubemap"] = pipeline;
    pipeline->SetName(L"singlePass PSO");
  }
}

void CubemapRenderingApp::Render()
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

  if (m_mode == Mode_MultiPassCubemap)
  {
    RenderToEachFace();
  }
  if (m_mode == Mode_SinglePassCubemap)
  {
    RenderToCubemapSinglePass();
  }

  // リソースバリア.
  auto barrierToSRV = CD3DX12_RESOURCE_BARRIER::Transition(
    m_renderCubemap.Get(),
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
  m_commandList->ResourceBarrier(1, &barrierToSRV);

  RenderToMain();

  RenderHUD();

  // レンダーターゲットからスワップチェイン表示可能へ
  {
    auto barrierToPresent = m_swapchain->GetBarrierToPresent();
    auto barrierToCubeRT = CD3DX12_RESOURCE_BARRIER::Transition(
      m_renderCubemap.Get(),
      D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_RENDER_TARGET
    );
    CD3DX12_RESOURCE_BARRIER barriers[] = {
      barrierToPresent,
      barrierToCubeRT,
    };

    m_commandList->ResourceBarrier(_countof(barriers), barriers);
  }
  m_commandList->Close();
  ID3D12CommandList* lists[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(1, lists);

  m_swapchain->Present(1, 0);
  m_swapchain->WaitPreviousFrame(m_commandQueue, m_frameIndex, GpuWaitTimeout);
}

void CubemapRenderingApp::RenderToEachFace()
{
  float clearColor[6][4] = {
    { 1.0f, 0.0f, 0.0f, 1.0f },
    { 0.5f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.5f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f, 1.0f },
    { 0.0f, 0.0f, 0.5f, 1.0f },
  };

  m_commandList->SetGraphicsRootSignature(m_rootSignatures["teapots"].Get());
  m_commandList->SetPipelineState(m_pipelines["cubeface"].Get());

  // ビューポートとシザーのセット
  m_commandList->RSSetViewports(1, &m_cubemapViewport);
  m_commandList->RSSetScissorRects(1, &m_cubemapScissor);

  for (int face = 0; face < 6; ++face)
  {
    m_commandList->ClearRenderTargetView(
      m_cubeFaceRTV[face],
      clearColor[face], 0, nullptr
    );
    m_commandList->ClearDepthStencilView(
      m_cubeFaceDSV[face],
      D3D12_CLEAR_FLAG_DEPTH,
      1.0f, 0, 0, nullptr
    );

    auto mtxView = GetViewMatrix(face);
    auto mtxProj = GetProjectionMatrix(45.0f, float(256) / float(256), 0.05f, 100.0f);

    auto renderTarget = (D3D12_CPU_DESCRIPTOR_HANDLE)m_cubeFaceRTV[face];
    auto dsv = (D3D12_CPU_DESCRIPTOR_HANDLE)m_cubeFaceDSV[face];
    m_commandList->OMSetRenderTargets(1, &renderTarget, FALSE, &dsv);

    FaceSceneParameters cbParams;
    XMStoreFloat4x4(&cbParams.world, XMMatrixIdentity());
    XMStoreFloat4x4(&cbParams.viewProj, XMMatrixTranspose(mtxView * mtxProj));
    XMStoreFloat4(&cbParams.cameraPos, m_camera.GetPosition());
    XMStoreFloat4(&cbParams.lightDir, m_lightDirection);

    auto cb = m_renderCubemapFacesCB[m_frameIndex * 6 + face];
    WriteToUploadHeapMemory(cb.Get(), sizeof(cbParams), &cbParams);

    // 周囲のティーポット描画.
    m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
    m_commandList->IASetIndexBuffer(&m_model.ibView);
    m_commandList->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
    m_commandList->SetGraphicsRootConstantBufferView(1, m_teapotInstanceParameters.Get()->GetGPUVirtualAddress());
    m_commandList->DrawIndexedInstanced(m_model.indexCount, InstanceCount, 0, 0, 0);

  }
}

void CubemapRenderingApp::RenderToCubemapSinglePass()
{
  float clearColor[6][4] = {
    { 0.75f, 0.75f, 1.0f, 1.0f },
    { 0.5f, 0.0f, 0.0f, 1.0f },
    { 0.0f, 1.0f, 0.0f, 1.0f },
    { 0.0f, 0.5f, 0.0f, 1.0f },
    { 0.0f, 0.0f, 1.0f, 1.0f },
    { 0.0f, 0.0f, 0.5f, 1.0f },
  };
  m_commandList->SetGraphicsRootSignature(m_rootSignatures["teapots"].Get());
  m_commandList->SetPipelineState(m_pipelines["singleCubemap"].Get());

  // ビューポートとシザーのセット
  m_commandList->RSSetViewports(1, &m_cubemapViewport);
  m_commandList->RSSetScissorRects(1, &m_cubemapScissor);

  XMMATRIX mtxViews[6];
  XMMATRIX mtxProj = GetProjectionMatrix(45.0f, float(256) / float(256), 0.05f, 100.0f);
  for (int face = 0; face < 6; ++face)
  {
    mtxViews[face] = GetViewMatrix(face);
  }

  CubeSceneParameters sceneParams{};
  for (int face = 0; face < 6; ++face)
  {
    XMStoreFloat4x4(&sceneParams.viewProj[face],
      XMMatrixTranspose(mtxViews[face] * mtxProj)
    );
  }
  XMStoreFloat4(&sceneParams.lightDir, m_lightDirection);

  auto cb = m_renderCubemapCB[m_frameIndex].Get();

  WriteToUploadHeapMemory(cb, sizeof(sceneParams), &sceneParams);

  D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_cubemapRTV;
  D3D12_CPU_DESCRIPTOR_HANDLE dsv = m_renderCubemapDSV;

  m_commandList->ClearRenderTargetView(rtv, clearColor[0], 0, nullptr);
  m_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  
  m_commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

  // 周囲のティーポット描画.
  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
  m_commandList->IASetIndexBuffer(&m_model.ibView);
  m_commandList->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
  m_commandList->SetGraphicsRootConstantBufferView(1, m_teapotInstanceParameters.Get()->GetGPUVirtualAddress());
  m_commandList->DrawIndexedInstanced(m_model.indexCount, InstanceCount, 0, 0, 0);
}

void CubemapRenderingApp::RenderToMain()
{
  auto rtv = m_swapchain->GetCurrentRTV();
  auto dsv = m_defaultDepthDSV;

  // カラーバッファ(レンダーターゲットビュー)のクリア
  float m_clearColor[4] = { 0.5f,0.75f,1.0f,0 };
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

  auto cb = m_renderMainCB[m_frameIndex];
  auto mtxView = m_camera.GetViewMatrix();
  auto mtxProj = GetProjectionMatrix(45.0f, float(m_width) / float(m_height), 0.1f, 100.0f);

  SceneParameters sceneParams{};
  XMStoreFloat4x4(&sceneParams.world, XMMatrixIdentity());
  XMStoreFloat4x4(&sceneParams.viewProj, XMMatrixTranspose(mtxView * mtxProj));
  XMStoreFloat4(&sceneParams.cameraPos, m_camera.GetPosition());
  XMStoreFloat4(&sceneParams.lightDir, m_lightDirection);
  WriteToUploadHeapMemory(cb.Get(), sizeof(sceneParams), &sceneParams);

  m_commandList->SetGraphicsRootSignature(m_rootSignatures["default"].Get());

  if (m_mode == Mode_StaticCubemap)
  {
    m_commandList->SetGraphicsRootDescriptorTable(1, m_staticCubemap.descriptorSRV);
  }
  else
  {
    m_commandList->SetGraphicsRootDescriptorTable(1, m_renderCubemapSRV);
  }

  m_commandList->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
  m_commandList->SetPipelineState(m_pipelines["default"].Get());

  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
  m_commandList->IASetIndexBuffer(&m_model.ibView);
  m_commandList->DrawIndexedInstanced(m_model.indexCount, 1, 0, 0, 0);

  // 周囲の Teapot を描画する.
  m_commandList->SetGraphicsRootSignature(m_rootSignatures["teapots"].Get());
  m_commandList->SetPipelineState(m_pipelines["teapots"].Get());
  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_commandList->IASetVertexBuffers(0, 1, &m_model.vbView);
  m_commandList->IASetIndexBuffer(&m_model.ibView);
  m_commandList->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
  m_commandList->SetGraphicsRootConstantBufferView(1, m_teapotInstanceParameters.Get()->GetGPUVirtualAddress());
  m_commandList->DrawIndexedInstanced(m_model.indexCount, InstanceCount, 0, 0, 0);
}

void CubemapRenderingApp::RenderHUD()
{
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  auto framerate = ImGui::GetIO().Framerate;
  ImGui::Begin("Control");
  ImGui::Text("Framerate %.3f ms", 1000.0f / framerate);
  XMFLOAT3 cameraPos;
  XMStoreFloat3(&cameraPos, m_camera.GetPosition());
  ImGui::Text("CameraPos (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);
  ImGui::Combo("Mode", (int*)&m_mode, "Static\0MultiPass\0SinglePass\0\0");
  ImGui::End();

  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}

DirectX::XMMATRIX CubemapRenderingApp::GetViewMatrix(int faceIndex)
{
  XMFLOAT3 up[6] = {
    XMFLOAT3(0.0f, 1.0f, 0.0f), // +X
    XMFLOAT3(0.0f, 1.0f, 0.0f), // -X
    XMFLOAT3(0.0f, 0.0f,-1.0f), // +Y
    XMFLOAT3(0.0f, 0.0f, 1.0f), // -Y
    XMFLOAT3(0.0f, 1.0f, 0.0f), // +Z
    XMFLOAT3(0.0f, 1.0f, 0.0f), // -Z
  };
  XMFLOAT3 target[6] = {
    XMFLOAT3(1.0f, 0.0f, 0.0f), // +X
    XMFLOAT3(-1.0f, 0.0f, 0.0f), // -X
    XMFLOAT3(0.0f, 1.0f, 0.0f), // +Y
    XMFLOAT3(0.0f,-1.0f, 0.0f), // -Y
    XMFLOAT3(0.0f, 0.0f, 1.0f), // +Z
    XMFLOAT3(0.0f, 0.0f,-1.0f), // -Z
  };

  auto eye = XMFLOAT3(0.0f, 0.0f, 0.0f);

  XMMATRIX mtxView = XMMatrixLookAtLH(
    XMLoadFloat3(&eye),
    XMLoadFloat3(&target[faceIndex]),
    XMLoadFloat3(&up[faceIndex])
  );

  return mtxView;
}

DirectX::XMMATRIX CubemapRenderingApp::GetProjectionMatrix(float fov, float aspect, float znear, float zfar)
{
  auto mtxProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fov), aspect, znear, zfar);
  return mtxProj;
}

CubemapRenderingApp::StaticCubeTexture CubemapRenderingApp::LoadCubeTextureFromFile(const std::wstring& fileName)
{
  DirectX::TexMetadata metadata;
  DirectX::ScratchImage image;
  HRESULT hr = DirectX::LoadFromDDSFile(fileName.c_str(), 0, &metadata, image);

  ComPtr<ID3D12Resource> cubemap;
  CreateTexture(m_device.Get(), metadata, &cubemap);

  ComPtr<ID3D12Resource> srcBuffer;
  std::vector<D3D12_SUBRESOURCE_DATA> subresources;
  PrepareUpload(m_device.Get(), image.GetImages(), image.GetImageCount(), metadata, subresources);
  const auto totalBytes = GetRequiredIntermediateSize(cubemap.Get(), 0, UINT(subresources.size()));

  auto staging = CreateResource(CD3DX12_RESOURCE_DESC::Buffer(totalBytes), D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, D3D12_HEAP_TYPE_UPLOAD);
  auto command = CreateCommandList();
  UpdateSubresources(command.Get(),
    cubemap.Get(), staging.Get(), 0, 0, UINT(subresources.size()), subresources.data());
  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
    cubemap.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
  command->ResourceBarrier(1, &barrier);
  FinishCommandList(command);

  auto descriptorSRV = GetDescriptorManager()->Alloc();
  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = metadata.format;
  srvDesc.TextureCube.MipLevels = UINT(metadata.mipLevels);
  srvDesc.TextureCube.MostDetailedMip = 0;
  srvDesc.TextureCube.ResourceMinLODClamp = 0;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
  m_device->CreateShaderResourceView(cubemap.Get(), &srvDesc, descriptorSRV);

  StaticCubeTexture ret;
  cubemap.As(&ret.resource);
  ret.descriptorSRV = descriptorSRV;
  return ret;
}

void CubemapRenderingApp::SetInfoQueueFilter()
{
  ComPtr<ID3D12InfoQueue> infoQueue;
  m_device.As(&infoQueue);

  D3D12_MESSAGE_ID denyIds[] = {
    D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
    D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
  };
  D3D12_MESSAGE_SEVERITY severities[] = {
    D3D12_MESSAGE_SEVERITY_INFO
  };
  D3D12_INFO_QUEUE_FILTER filter{};
  filter.DenyList.NumIDs = _countof(denyIds);
  filter.DenyList.pIDList = denyIds;
  filter.DenyList.NumSeverities = _countof(severities);
  filter.DenyList.pSeverityList = severities;

  if (infoQueue != nullptr)
  {
      infoQueue->PushStorageFilter(&filter);
      infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
  }
}



