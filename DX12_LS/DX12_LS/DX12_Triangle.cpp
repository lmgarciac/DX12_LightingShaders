// main.cpp - DX12 Hello Triangle (Win32, x64)
// Compilar con: d3d12.lib, dxgi.lib, d3dcompiler.lib
// Requiere Windows 10 SDK o superior.

#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <stdexcept>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) if (p) { (p)->Release(); (p) = nullptr; }
#endif

// --- Config ---
static const UINT FrameCount = 2;
static UINT g_Width = 1280;
static UINT g_Height = 720;

// --- Win32 ---
HINSTANCE g_hInst = nullptr;
HWND g_hWnd = nullptr;

// --- DX12 core ---
ComPtr<ID3D12Device>            g_Device;
ComPtr<IDXGISwapChain3>         g_SwapChain;
ComPtr<ID3D12CommandQueue>      g_CommandQueue;
ComPtr<ID3D12DescriptorHeap>    g_RTVHeap;
UINT                            g_RTVDescriptorSize = 0;
ComPtr<ID3D12Resource>          g_RenderTargets[FrameCount];
ComPtr<ID3D12CommandAllocator>  g_CommandAllocators[FrameCount];
ComPtr<ID3D12GraphicsCommandList> g_CommandList;
ComPtr<ID3D12Fence>             g_Fence;
UINT64                          g_FenceValues[FrameCount] = {};
HANDLE                          g_FenceEvent = nullptr;
UINT                            g_FrameIndex = 0;

// --- Pipeline state ---
ComPtr<ID3D12RootSignature>     g_RootSignature;
ComPtr<ID3D12PipelineState>     g_PSO;

// --- Geometry ---
struct Vertex { XMFLOAT3 pos; XMFLOAT3 color; };
ComPtr<ID3D12Resource>          g_VertexBuffer;
D3D12_VERTEX_BUFFER_VIEW        g_VBView = {};

// --- Viewport/Scissor ---
D3D12_VIEWPORT                  g_Viewport = {};
D3D12_RECT                      g_ScissorRect = {};

// --------------------------------- Helpers ---------------------------------

inline void ThrowIfFailed(HRESULT hr)
{
    if (FAILED(hr)) throw std::runtime_error("HRESULT failed.");
}

#if defined(_DEBUG)
void EnableDebugLayer()
{
    ComPtr<ID3D12Debug> debug;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
    }
}
#endif

static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        // (Opcional) Manejar resize. Para simplicity, lo ignoramos en este sample.
        return 0;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
}

void WaitForGPU()
{
    const UINT64 fenceToWait = ++g_FenceValues[g_FrameIndex];
    ThrowIfFailed(g_CommandQueue->Signal(g_Fence.Get(), fenceToWait));

    if (g_Fence->GetCompletedValue() < fenceToWait)
    {
        ThrowIfFailed(g_Fence->SetEventOnCompletion(fenceToWait, g_FenceEvent));
        WaitForSingleObject(g_FenceEvent, INFINITE);
    }
}

void MoveToNextFrame()
{
    const UINT64 currentFenceValue = ++g_FenceValues[g_FrameIndex];
    ThrowIfFailed(g_CommandQueue->Signal(g_Fence.Get(), currentFenceValue));

    g_FrameIndex = g_SwapChain->GetCurrentBackBufferIndex();

    if (g_Fence->GetCompletedValue() < g_FenceValues[g_FrameIndex])
    {
        ThrowIfFailed(g_Fence->SetEventOnCompletion(g_FenceValues[g_FrameIndex], g_FenceEvent));
        WaitForSingleObject(g_FenceEvent, INFINITE);
    }
}

// ------------------------------- Init DX12 ----------------------------------

void CreateWindowClassAndWindow()
{
    const wchar_t* CLASS_NAME = L"DX12WindowClass";

    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInst;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassEx(&wc);

    RECT rc = { 0, 0, (LONG)g_Width, (LONG)g_Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindowEx(
        0, CLASS_NAME, L"DX12 Hello Triangle",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, g_hInst, nullptr);

    ShowWindow(g_hWnd, SW_SHOW);
}

void CreateDeviceAndSwapchain()
{
    UINT dxgiFactoryFlags = 0;

#if defined(_DEBUG)
    // Si el Debug Layer está habilitado y fallan validaciones, se mostrarán en Output.
    EnableDebugLayer();
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    ComPtr<IDXGIFactory6> factory;
    ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    // Crear dispositivo (usar el adaptador por defecto)
    ThrowIfFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_Device)));

    // Command queue
    D3D12_COMMAND_QUEUE_DESC qdesc = {};
    qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    qdesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    qdesc.NodeMask = 0;
    ThrowIfFailed(g_Device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(&g_CommandQueue)));

    // Swap chain
    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.BufferCount = FrameCount;
    scDesc.Width = g_Width;
    scDesc.Height = g_Height;
    scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    ThrowIfFailed(factory->CreateSwapChainForHwnd(
        g_CommandQueue.Get(), g_hWnd, &scDesc, nullptr, nullptr, &swapChain1));

    // Deshabilitar Alt-Enter full screen toggle (opcional)
    ThrowIfFailed(factory->MakeWindowAssociation(g_hWnd, DXGI_MWA_NO_ALT_ENTER));

    ThrowIfFailed(swapChain1.As(&g_SwapChain));
    g_FrameIndex = g_SwapChain->GetCurrentBackBufferIndex();
}

void CreateRTVsAndAllocators()
{
    // RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FrameCount;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(g_Device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_RTVHeap)));
    g_RTVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Helper para obtener el handle i-ésimo (sin d3dx12.h)
    auto getCPUHandle = [](ID3D12DescriptorHeap* heap, UINT increment, UINT idx) {
        D3D12_CPU_DESCRIPTOR_HANDLE h = heap->GetCPUDescriptorHandleForHeapStart();
        h.ptr += SIZE_T(increment) * idx;
        return h;
        };

    for (UINT i = 0; i < FrameCount; ++i)
    {
        ThrowIfFailed(g_SwapChain->GetBuffer(i, IID_PPV_ARGS(&g_RenderTargets[i])));
        g_Device->CreateRenderTargetView(
            g_RenderTargets[i].Get(),
            nullptr,
            getCPUHandle(g_RTVHeap.Get(), g_RTVDescriptorSize, i)
        );

        ThrowIfFailed(g_Device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&g_CommandAllocators[i])
        ));
    }
}

void CreateRootSignatureAndPSO()
{
    // Root signature mínima (IA permitido)
    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> sigBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        &sigBlob, &errBlob));

    ThrowIfFailed(g_Device->CreateRootSignature(
        0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&g_RootSignature)));

    // Shaders (HLSL embebido)
    const char* HLSL =
        "struct VSInput { float3 pos: POSITION; float3 color: COLOR; };"
        "struct PSInput { float4 pos: SV_POSITION; float3 color: COLOR; };"
        "PSInput VSMain(VSInput input){ PSInput o; o.pos=float4(input.pos,1); o.color=input.color; return o; }"
        "float4 PSMain(PSInput input): SV_TARGET { return float4(input.color,1); }";

    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, err;
    ThrowIfFailed(D3DCompile(HLSL, strlen(HLSL), nullptr, nullptr, nullptr,
        "VSMain", "vs_5_0", compileFlags, 0, &vsBlob, &err));
    ThrowIfFailed(D3DCompile(HLSL, strlen(HLSL), nullptr, nullptr, nullptr,
        "PSMain", "ps_5_0", compileFlags, 0, &psBlob, &err));

    // Input layout
    D3D12_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
          D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    // Defaults manuales (sin d3dx12.h)
    D3D12_RASTERIZER_DESC rast = {};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_BACK;
    rast.FrontCounterClockwise = FALSE;
    rast.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rast.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rast.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rast.DepthClipEnable = TRUE;
    rast.MultisampleEnable = FALSE;
    rast.AntialiasedLineEnable = FALSE;
    rast.ForcedSampleCount = 0;
    rast.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_BLEND_DESC blend = {};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    D3D12_RENDER_TARGET_BLEND_DESC rt = {};
    rt.BlendEnable = FALSE;
    rt.LogicOpEnable = FALSE;
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.RenderTarget[0] = rt;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { layout, _countof(layout) };
    psoDesc.pRootSignature = g_RootSignature.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.RasterizerState = rast;
    psoDesc.BlendState = blend;
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    ThrowIfFailed(g_Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&g_PSO)));

    // Command list
    ThrowIfFailed(g_Device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        g_CommandAllocators[g_FrameIndex].Get(),
        g_PSO.Get(), IID_PPV_ARGS(&g_CommandList)));
    ThrowIfFailed(g_CommandList->Close());
}

void CreateTriangle()
{
    // Triángulo RGB
    Vertex vertices[] =
    {
        { XMFLOAT3(0.0f,  0.25f, 0.0f), XMFLOAT3(1,0,0) },
        { XMFLOAT3(0.25f, -0.25f, 0.0f), XMFLOAT3(0,1,0) },
        { XMFLOAT3(-0.25f, -0.25f, 0.0f), XMFLOAT3(0,0,1) }
    };

    const UINT vbSize = sizeof(vertices);

    // Para simplificar: buffer en UPLOAD heap (CPU visible).
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = vbSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    ThrowIfFailed(g_Device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&g_VertexBuffer)));

    // Mapear y copiar datos
    void* pData = nullptr;
    D3D12_RANGE range = {}; // no leemos desde CPU
    ThrowIfFailed(g_VertexBuffer->Map(0, &range, &pData));
    memcpy(pData, vertices, vbSize);
    g_VertexBuffer->Unmap(0, nullptr);

    g_VBView.BufferLocation = g_VertexBuffer->GetGPUVirtualAddress();
    g_VBView.SizeInBytes = vbSize;
    g_VBView.StrideInBytes = sizeof(Vertex);
}

void CreateFenceAndViewport()
{
    ThrowIfFailed(g_Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_Fence)));
    g_FenceValues[g_FrameIndex] = 0;
    g_FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

    g_Viewport = { 0.0f, 0.0f, float(g_Width), float(g_Height), 0.0f, 1.0f };
    g_ScissorRect = { 0, 0, (LONG)g_Width, (LONG)g_Height };
}

// ------------------------------- Rendering ----------------------------------

void PopulateCommandList()
{
    // Reset allocator + list
    ThrowIfFailed(g_CommandAllocators[g_FrameIndex]->Reset());
    ThrowIfFailed(g_CommandList->Reset(g_CommandAllocators[g_FrameIndex].Get(), g_PSO.Get()));

    // Seteos básicos
    g_CommandList->SetGraphicsRootSignature(g_RootSignature.Get());
    g_CommandList->RSSetViewports(1, &g_Viewport);
    g_CommandList->RSSetScissorRects(1, &g_ScissorRect);

    // Transición: PRESENT -> RENDER_TARGET
    D3D12_RESOURCE_BARRIER toRT = {};
    toRT.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toRT.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    toRT.Transition.pResource = g_RenderTargets[g_FrameIndex].Get();
    toRT.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toRT.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    toRT.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    g_CommandList->ResourceBarrier(1, &toRT);

    // RTV handle actual
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_RTVHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += SIZE_T(g_RTVDescriptorSize) * g_FrameIndex;

    // Limpiar
    const float clearColor[] = { 0.1f, 0.1f, 0.4f, 1.0f };
    g_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    g_CommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

    // Dibujar triángulo
    g_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_CommandList->IASetVertexBuffers(0, 1, &g_VBView);
    g_CommandList->DrawInstanced(3, 1, 0, 0);

    // Transición: RENDER_TARGET -> PRESENT
    D3D12_RESOURCE_BARRIER toPresent = {};
    toPresent.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    toPresent.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    toPresent.Transition.pResource = g_RenderTargets[g_FrameIndex].Get();
    toPresent.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    toPresent.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    toPresent.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    g_CommandList->ResourceBarrier(1, &toPresent);

    ThrowIfFailed(g_CommandList->Close());
}

void Render()
{
    PopulateCommandList();

    ID3D12CommandList* lists[] = { g_CommandList.Get() };
    g_CommandQueue->ExecuteCommandLists(1, lists);

    ThrowIfFailed(g_SwapChain->Present(1, 0));

    MoveToNextFrame();
}

// ---------------------------------- Main ------------------------------------

void InitDX12()
{
    CreateDeviceAndSwapchain();
    CreateRTVsAndAllocators();
    CreateRootSignatureAndPSO();
    CreateTriangle();
    CreateFenceAndViewport();
}

void Cleanup()
{
    // Asegurar que GPU terminó
    WaitForGPU();

    CloseHandle(g_FenceEvent);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE, _In_ LPWSTR, _In_ int)
{
    g_hInst = hInstance;

    try
    {
        CreateWindowClassAndWindow();
        InitDX12();

        // Bucle principal
        MSG msg = {};
        while (msg.message != WM_QUIT)
        {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            else
            {
                Render();
            }
        }

        Cleanup();
        return 0;
    }
    catch (const std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "Fatal Error", MB_ICONERROR | MB_OK);
        return -1;
    }
}
