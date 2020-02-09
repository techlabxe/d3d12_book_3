#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <d3d12.h>
#include <dxgi1_6.h>

#include "d3dx12.h"
#include <wrl.h>

#include "DescriptorManager.h"
#include "Swapchain.h"
#include <memory>


#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

class D3D12AppBase
{
public:
  template<class T>
  using ComPtr = Microsoft::WRL::ComPtr<T>;

  D3D12AppBase();
  virtual ~D3D12AppBase();

  void Initialize(HWND hWnd, DXGI_FORMAT format, bool isFullScreen);
  void Terminate();

  virtual void Render();// = 0;

  virtual void Prepare() { }
  virtual void Cleanup() { }

  const UINT GpuWaitTimeout = (10 * 1000);  // 10s
  static const UINT FrameBufferCount = 2;

  virtual void OnSizeChanged(UINT width, UINT height, bool isMinimized);
  virtual void OnMouseButtonDown(UINT msg) { }
  virtual void OnMouseButtonUp(UINT msg) { }
  virtual void OnMouseMove(UINT msg, int dx, int dy) { }


  void SetTitle(const std::string& title);
  void ToggleFullscreen();

  ComPtr<ID3D12Device> GetDevice() { return m_device; }
  std::shared_ptr<Swapchain> GetSwapchain() { return m_swapchain; }

  // リソース生成
  ComPtr<ID3D12Resource1> CreateResource(
    const CD3DX12_RESOURCE_DESC& desc, 
    D3D12_RESOURCE_STATES resourceStates, 
    const D3D12_CLEAR_VALUE* clearValue,
    D3D12_HEAP_TYPE heapType
  );
  std::vector<ComPtr<ID3D12Resource1>> CreateConstantBuffers(
    const CD3DX12_RESOURCE_DESC& desc,
    int count = FrameBufferCount
  );

  // コマンドバッファ関連
  ComPtr<ID3D12GraphicsCommandList>  CreateCommandList();
  void FinishCommandList(ComPtr<ID3D12GraphicsCommandList>& command);
  ComPtr<ID3D12GraphicsCommandList> CreateBundleCommandList();

  void WriteToUploadHeapMemory(ID3D12Resource1* resource, uint32_t size, const void* pData);

  std::shared_ptr<DescriptorManager> GetDescriptorManager() { return m_heap; }

  using Buffer = ComPtr<ID3D12Resource1>;
  using Texture = ComPtr<ID3D12Resource1>;
  struct ModelData
  {
    UINT indexCount;
    UINT vertexCount;
    D3D12_VERTEX_BUFFER_VIEW vbView;
    D3D12_INDEX_BUFFER_VIEW  ibView;
    Buffer resourceVB;
    Buffer resourceIB;
  };

  // 単純モデルのデータをGPUへ転送.
  template<class T>
  ModelData CreateSimpleModel(const std::vector<T>& vertices, const std::vector<uint32_t>& indices)
  {
    ModelData model;
    auto bufferSize = uint32_t(sizeof(T)*vertices.size());
    auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    auto srcHeapType = D3D12_HEAP_TYPE_UPLOAD;
    auto dstHeapType = D3D12_HEAP_TYPE_DEFAULT;

    model.resourceVB = CreateResource(vbDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, dstHeapType);
    auto uploadVB = CreateResource(vbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, srcHeapType);
    WriteToUploadHeapMemory(uploadVB.Get(), bufferSize, vertices.data());

    bufferSize = UINT(sizeof(UINT)*indices.size());
    auto ibDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
    model.resourceIB = CreateResource(ibDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, dstHeapType);
    auto uploadIB = CreateResource(ibDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, srcHeapType);
    WriteToUploadHeapMemory(uploadIB.Get(), bufferSize, indices.data());

    auto command = CreateCommandList();
    command->CopyResource(model.resourceVB.Get(), uploadVB.Get());
    command->CopyResource(model.resourceIB.Get(), uploadIB.Get());

    D3D12_RESOURCE_BARRIER barriers[] = {
      CD3DX12_RESOURCE_BARRIER::Transition(
        model.resourceVB.Get(), 
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
      CD3DX12_RESOURCE_BARRIER::Transition(
        model.resourceIB.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST,
        D3D12_RESOURCE_STATE_INDEX_BUFFER)
    };
    command->ResourceBarrier(_countof(barriers), barriers);
    FinishCommandList(command);

    model.indexCount = UINT(indices.size());
    model.vertexCount = UINT(vertices.size());

    model.vbView.BufferLocation = model.resourceVB->GetGPUVirtualAddress();
    model.vbView.StrideInBytes = sizeof(T);
    model.vbView.SizeInBytes = UINT(model.vbView.StrideInBytes * vertices.size());
    model.ibView.BufferLocation = model.resourceIB->GetGPUVirtualAddress();
    model.ibView.Format = DXGI_FORMAT_R32_UINT;
    model.ibView.SizeInBytes = UINT(sizeof(UINT) * indices.size());

    return model;
  }

protected:

  void PrepareDescriptorHeaps();
  
  void CreateDefaultDepthBuffer(int width, int height);
  void CreateCommandAllocators();
  void WaitForIdleGPU();

  // ImGui
  void PrepareImGui();
  void CleanupImGui();

  ComPtr<ID3D12Device> m_device;
  ComPtr<ID3D12CommandQueue> m_commandQueue;
 
  std::shared_ptr<Swapchain> m_swapchain;

  std::vector<ComPtr<ID3D12Resource1>> m_renderTargets;
  ComPtr<ID3D12Resource1> m_depthBuffer;

  CD3DX12_VIEWPORT  m_viewport;
  CD3DX12_RECT m_scissorRect;
  DXGI_FORMAT  m_surfaceFormat;

  
  std::vector<ComPtr<ID3D12CommandAllocator>> m_commandAllocators;
  ComPtr<ID3D12CommandAllocator> m_oneshotCommandAllocator;
  ComPtr<ID3D12CommandAllocator> m_bundleCommandAllocator;

  std::shared_ptr<DescriptorManager> m_heapRTV;
  std::shared_ptr<DescriptorManager> m_heapDSV;
  std::shared_ptr<DescriptorManager> m_heap;

  DescriptorHandle m_defaultDepthDSV;
  ComPtr<ID3D12GraphicsCommandList> m_commandList;
  HANDLE m_waitFence;

  UINT m_frameIndex;


  UINT m_width;
  UINT m_height;
  bool m_isAllowTearing;
  HWND m_hwnd;
};

class Shader
{
public:
  Shader() = default;
  enum Stage
  {
    Vertex, Geometry, Pixel,
    Domain, Hull,
    Compute,
  };

  struct DefineMacro {
    std::wstring Name;
    std::wstring Value;
  };

  void load(const std::wstring& fileName, Stage stage,
    const std::wstring& entryPoint,
    const std::vector<std::wstring>& flags,
    const std::vector<DefineMacro>& defines);

  const Microsoft::WRL::ComPtr<ID3DBlob>& getCode() const { return m_code; }

private:
  Microsoft::WRL::ComPtr<ID3DBlob> m_code;
};
