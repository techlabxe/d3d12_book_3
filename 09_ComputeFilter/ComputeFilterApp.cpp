#include "ComputeFilterApp.h"

#include "imgui.h"
#include "examples/imgui_impl_dx12.h"
#include "examples/imgui_impl_win32.h"

#include <DirectXTex.h>
#include <fstream>

using namespace std;
using namespace DirectX;

ComputeFilterApp::ComputeFilterApp()  
{
  m_mode = Mode_Sepia;
}

void ComputeFilterApp::Prepare()
{
  SetTitle("ComputeFilter");
  CreateRootSignatures();

  m_commandList->Reset(m_commandAllocators[0].Get(), nullptr);
  ID3D12DescriptorHeap* heaps[] = { m_heap->GetHeap().Get() };
  m_commandList->SetDescriptorHeaps(1, heaps);
  m_commandList->Close();

  PrepareSimpleModel();
  PrepareComputeFilter();
  PreparePipeline();

  auto cbDesc = CD3DX12_RESOURCE_DESC::Buffer(
    sizeof(SceneParameters)
  );
  m_mainSceneCB = CreateConstantBuffers(cbDesc);
}

void ComputeFilterApp::CreateRootSignatures()
{
  // RootSignature
  D3D12_DESCRIPTOR_RANGE descSrv{};
  descSrv.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
  descSrv.BaseShaderRegister = 0;
  descSrv.NumDescriptors = 1;
  array<CD3DX12_ROOT_PARAMETER, 2> rootParams;
  rootParams[0].InitAsConstantBufferView(0);
  rootParams[1].InitAsDescriptorTable(1, &descSrv);

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

void ComputeFilterApp::PreparePipeline()
{
  D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
    { "POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
  };

  std::vector<wstring> flags;
  std::vector<Shader::DefineMacro> defines;

  Shader shaderVS, shaderPS;
  shaderVS.load(L"ComputeFilter.hlsl", Shader::Vertex, L"mainVS", flags, defines);
  shaderPS.load(L"ComputeFilter.hlsl", Shader::Pixel, L"mainPS", flags, defines);


  auto rasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  rasterizerState.CullMode = D3D12_CULL_MODE_NONE;

  auto psoDesc = book_util::CreateDefaultPsoDesc(
    DXGI_FORMAT_R8G8B8A8_UNORM,
    rasterizerState,
    inputElementDesc, _countof(inputElementDesc),
    m_rootSignature,
    shaderVS.getCode(), shaderPS.getCode()
  );

  HRESULT hr;
  PipelineState pipeline;
  hr = m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipeline));
  ThrowIfFailed(hr, "CreateGraphicsPipelineState Failed.");
  m_pipelines["default"] = pipeline;
}

void ComputeFilterApp::Cleanup()
{
}

void ComputeFilterApp::PrepareSimpleModel()
{
  using VertexData = std::vector<Vertex>;
  using IndexData = std::vector<UINT>;
  VertexData quadVertices;
  IndexData  quadIndices;

  float offset = 10.0f;

  quadVertices = {
    { XMFLOAT3(-480.0f - offset,  135.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
    { XMFLOAT3(   0.0f - offset,  135.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
    { XMFLOAT3(-480.0f - offset, -135.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
    { XMFLOAT3(   0.0f - offset, -135.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
  };
  quadIndices = { 0, 1, 2, 3, };

  m_quad = CreateSimpleModel(quadVertices, quadIndices);

  quadVertices = {
    { XMFLOAT3(0.0f + offset,  135.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },
    { XMFLOAT3( 480.0f + offset,  135.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },
    { XMFLOAT3(0.0f + offset, -135.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },
    { XMFLOAT3( 480.0f + offset, -135.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) },
  };
  m_quad2 = CreateSimpleModel(quadVertices, quadIndices);
}

void ComputeFilterApp::Render()
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

void ComputeFilterApp::RenderToMain()
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
  D3D12_CPU_DESCRIPTOR_HANDLE handleRtvs[] = { rtv };
  D3D12_CPU_DESCRIPTOR_HANDLE handleDsv = dsv;
  m_commandList->OMSetRenderTargets(1, handleRtvs, FALSE, &handleDsv);

  // ビューポートとシザーのセット
  auto viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, float(m_width), float(m_height));
  auto scissorRect = CD3DX12_RECT(0, 0, LONG(m_width), LONG(m_height));
  m_commandList->RSSetViewports(1, &viewport);
  m_commandList->RSSetScissorRects(1, &scissorRect);

  auto sceneCB = m_mainSceneCB[m_frameIndex];
  auto mtxProj = XMMatrixOrthographicLH(1280.0f, 720.0f, 0.0f, 10.0f);

  SceneParameters sceneParams{};
  XMStoreFloat4x4(&sceneParams.proj, XMMatrixTranspose(mtxProj));

  WriteToUploadHeapMemory(sceneCB.Get(), sizeof(sceneParams), &sceneParams);

  m_commandList->SetComputeRootSignature(m_csSignature.Get());
  if (m_mode == Mode_Sepia)
  {
    m_commandList->SetPipelineState(m_pipelines["sepiaCS"].Get());
  }
  if (m_mode == Mode_Sobel)
  {
    m_commandList->SetPipelineState(m_pipelines["sobelCS"].Get());
  }

  m_commandList->SetComputeRootDescriptorTable(
    0, m_texture.handleRead
  );
  m_commandList->SetComputeRootDescriptorTable(
    1, m_uavTexture.handleWrite
  );
  int groupX = 1280 / 16 + 1;
  int groupY = 720 / 16 + 1;
  m_commandList->Dispatch(1280, 720, 1);

  // UAV -> SRV へステート変更.
  auto barrierUAVtoSRV = CD3DX12_RESOURCE_BARRIER::Transition(
    m_uavTexture.texture.Get(),
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
  );
  m_commandList->ResourceBarrier(1, &barrierUAVtoSRV);

  m_commandList->SetPipelineState(m_pipelines["default"].Get());
  m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  m_commandList->IASetIndexBuffer(&m_quad.ibView);
  m_commandList->IASetVertexBuffers(0, 1, &m_quad.vbView);
  m_commandList->SetGraphicsRootConstantBufferView(0, sceneCB->GetGPUVirtualAddress());
  m_commandList->SetGraphicsRootDescriptorTable(1, m_texture.handleRead);
  m_commandList->DrawIndexedInstanced(4, 1, 0, 0, 0);

  m_commandList->IASetIndexBuffer(&m_quad2.ibView);
  m_commandList->IASetVertexBuffers(0, 1, &m_quad2.vbView);
  m_commandList->SetGraphicsRootConstantBufferView(0, sceneCB->GetGPUVirtualAddress());
  m_commandList->SetGraphicsRootDescriptorTable(1, m_uavTexture.handleRead);
  m_commandList->DrawIndexedInstanced(4, 1, 0, 0, 0);

  // SRV -> UAV へステートを戻す
  auto barrierSRVtoUAV = CD3DX12_RESOURCE_BARRIER::Transition(
    m_uavTexture.texture.Get(),
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS
  );
  m_commandList->ResourceBarrier(1, &barrierSRVtoUAV);
}

void ComputeFilterApp::RenderHUD()
{
  ImGui_ImplDX12_NewFrame();
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();

  auto framerate = ImGui::GetIO().Framerate;
  ImGui::Begin("Information");
  ImGui::Text("Framerate %.3f ms", 1000.0f / framerate);

  ImGui::Combo("Filter", (int*)&m_mode, "Sepia Filter\0Sobel Filter\0\0");
  ImGui::Spacing();
  ImGui::End();

  ImGui::Render();
  ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_commandList.Get());
}

void ComputeFilterApp::PrepareComputeFilter()
{
  m_texture = LoadTextureFromFile(L"dx12_vol1-alicia.tga");

  const UINT width = 1280, height = 720;
  auto texDesc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
  texDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

  const auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
  HRESULT hr = m_device->CreateCommittedResource(
    &heapProps,
    D3D12_HEAP_FLAG_NONE,
    &texDesc,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    nullptr,
    IID_PPV_ARGS(&m_uavTexture.texture)
  );
  ThrowIfFailed(hr, "CreateCommittedResource failed.");

  D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
  uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
  uavDesc.Format = texDesc.Format;
  uavDesc.Texture2D.MipSlice = 0;
  uavDesc.Texture2D.PlaneSlice = 0;
  m_uavTexture.handleWrite = m_heap->Alloc();
  m_device->CreateUnorderedAccessView(
    m_uavTexture.texture.Get(),
    nullptr,
    &uavDesc,
    m_uavTexture.handleWrite
  );

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = texDesc.Format;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  srvDesc.Texture2D.MipLevels = 1;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  m_uavTexture.handleRead = m_heap->Alloc();
  m_device->CreateShaderResourceView(
    m_uavTexture.texture.Get(),
    &srvDesc,
    m_uavTexture.handleRead
  );

  {
    D3D12_DESCRIPTOR_RANGE descRange0{}, descRange1{};
    descRange0.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descRange0.NumDescriptors = 1;
    descRange0.BaseShaderRegister = 0;
    descRange1.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    descRange1.NumDescriptors = 1;
    descRange1.BaseShaderRegister = 0;
    array<CD3DX12_ROOT_PARAMETER, 2> rootParams;
    rootParams[0].InitAsDescriptorTable(1, &descRange0);
    rootParams[1].InitAsDescriptorTable(1, &descRange1);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.Init(
      UINT(rootParams.size()), rootParams.data(),
      0, nullptr,
      D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> signature, errBlob;
    D3D12SerializeRootSignature(&rootSignatureDesc,
      D3D_ROOT_SIGNATURE_VERSION_1_0, &signature, &errBlob);
    hr = m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_csSignature));
    ThrowIfFailed(hr, "CreateRootSignature failed.");
  }

  std::vector<std::wstring> flags;
  std::vector<Shader::DefineMacro> defines;
  Shader shaderCS0;
  shaderCS0.load(L"ComputeFilter.hlsl", Shader::Compute, L"mainSepia", flags, defines);
  {
    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
    computeDesc.CS = CD3DX12_SHADER_BYTECODE(shaderCS0.getCode().Get());
    computeDesc.pRootSignature = m_csSignature.Get();

    PipelineState pipeline;
    hr = m_device->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&pipeline));
    ThrowIfFailed(hr, "CreateComputePipelineState failed.");
    m_pipelines["sepiaCS"] = pipeline;
  }

  Shader shaderCS1;
  shaderCS1.load(L"ComputeFilter.hlsl", Shader::Compute, L"mainSobel", flags, defines);
  {
    D3D12_COMPUTE_PIPELINE_STATE_DESC computeDesc{};
    computeDesc.CS = CD3DX12_SHADER_BYTECODE(shaderCS1.getCode().Get());
    computeDesc.pRootSignature = m_csSignature.Get();
    
    PipelineState pipeline;
    hr = m_device->CreateComputePipelineState(&computeDesc, IID_PPV_ARGS(&pipeline));
    ThrowIfFailed(hr, "CreateComputePipelineState failed.");
    m_pipelines["sobelCS"] = pipeline;
  }
}

ComputeFilterApp::TextureData ComputeFilterApp::LoadTextureFromFile(const std::wstring& name)
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

  D3D12_RESOURCE_FLAGS resFlags = D3D12_RESOURCE_FLAG_NONE;
  resFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  ComPtr<ID3D12Resource> texture;
  CreateTextureEx(m_device.Get(), metadata, resFlags,false, &texture);

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
  texData.handleRead = m_heap->Alloc();

  D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
  srvDesc.Format = metadata.format;
  srvDesc.TextureCube.MipLevels = UINT(metadata.mipLevels);
  srvDesc.TextureCube.MostDetailedMip = 0;
  srvDesc.TextureCube.ResourceMinLODClamp = 0;
  srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
  m_device->CreateShaderResourceView(texture.Get(), &srvDesc, texData.handleRead);

  return texData;
}
