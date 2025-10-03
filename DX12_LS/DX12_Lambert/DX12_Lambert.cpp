// main.cpp - DX12 Rotating Cube (Win32, x64)
// Compilar con: d3d12.lib, dxgi.lib, d3dcompiler.lib
// Requiere Windows 10 SDK+
// No usa d3dx12.h (todo structs/manual)

#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

//--------------------------------------------------------------------------------------
// Config
//--------------------------------------------------------------------------------------
static const UINT FrameCount = 2;
static const UINT Width = 1280;
static const UINT Height = 720;

//--------------------------------------------------------------------------------------
// Util
//--------------------------------------------------------------------------------------
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) if(p){ (p)->Release(); (p)=nullptr; }
#endif

inline void ThrowIfFailed(HRESULT hr) { if (FAILED(hr)) { assert(false); ExitProcess((UINT)hr); } }

// Alinear a múltiplos de 256 (para CBVs)
inline UINT Align256(UINT size) { return (size + 255) & ~255u; }

//--------------------------------------------------------------------------------------
// Vertex / Const Buffer
//--------------------------------------------------------------------------------------
struct Vertex {
    XMFLOAT3 pos;
    XMFLOAT3 color;
    XMFLOAT3 normal;
};

struct alignas(256) CBData {
    XMMATRIX mvp;
    XMMATRIX world;   // para transformar normales
    XMFLOAT3 lightDir;// dirección de luz en world (unitaria)
    float _pad0;      // padding a 16 bytes
};

//--------------------------------------------------------------------------------------
// Globals DX12
//--------------------------------------------------------------------------------------
HWND                                g_hWnd = nullptr;
UINT                                g_frameIndex = 0;
UINT                                g_rtvDescriptorSize = 0;
UINT                                g_dsvDescriptorSize = 0;
D3D12_VIEWPORT                      g_viewport = { 0.0f, 0.0f, (float)Width, (float)Height, 0.0f, 1.0f };
D3D12_RECT                          g_scissorRect = { 0, 0, (LONG)Width, (LONG)Height };

ComPtr<IDXGIFactory7>               g_factory;
ComPtr<ID3D12Device>                g_device;
ComPtr<ID3D12CommandQueue>          g_cmdQueue;
ComPtr<IDXGISwapChain3>             g_swapChain;
ComPtr<ID3D12DescriptorHeap>        g_rtvHeap;
ComPtr<ID3D12Resource>              g_renderTargets[FrameCount];
ComPtr<ID3D12CommandAllocator>      g_cmdAlloc[FrameCount];
ComPtr<ID3D12GraphicsCommandList>   g_cmdList;
ComPtr<ID3D12Fence>                 g_fence;
UINT64                              g_fenceValue = 0;
HANDLE                              g_fenceEvent = nullptr;

ComPtr<ID3D12DescriptorHeap>        g_dsvHeap;
ComPtr<ID3D12Resource>              g_depthTex;

ComPtr<ID3D12RootSignature>         g_rootSig;
ComPtr<ID3D12PipelineState>         g_pso;

ComPtr<ID3D12Resource>              g_vb;
ComPtr<ID3D12Resource>              g_ib;
D3D12_VERTEX_BUFFER_VIEW            g_vbView = {};
D3D12_INDEX_BUFFER_VIEW             g_ibView = {};

ComPtr<ID3D12Resource>              g_cb;    // Upload persistente (mapeado)
CBData* g_cbMapped = nullptr;

XMMATRIX                            g_proj;
XMMATRIX                            g_view;

// Timing
auto g_t0 = std::chrono::high_resolution_clock::now();

//--------------------------------------------------------------------------------------
// Shaders (HLSL) embebidos
//--------------------------------------------------------------------------------------
const char* g_vsHlsl = R"(
cbuffer CB : register(b0)
{
    float4x4 mvp;
    float4x4 world;
    float3   lightDir;
    float    _pad0;
};

struct VSIn {
    float3 pos : POSITION;
    float3 col : COLOR0;
    float3 nrm : NORMAL;
};

struct PSIn {
    float4 pos : SV_POSITION;
    float3 col : COLOR0;
    float3 nrmWS : NORMAL;
};

PSIn main(VSIn vin) {
    PSIn vout;
    vout.pos = mul(float4(vin.pos,1.0), mvp);

    // transformar normal a world (sin traslación)
    float3 n = mul((float3x3)world, vin.nrm);
    vout.nrmWS = normalize(n);

    vout.col = vin.col;
    return vout;
}
)";

const char* g_psHlsl = R"(
cbuffer CB : register(b0)
{
    float4x4 mvp;
    float4x4 world;
    float3   lightDir;
    float    _pad0;
};

struct PSIn {
    float4 pos   : SV_POSITION;
    float3 col   : COLOR0;
    float3 nrmWS : NORMAL;
};

float4 main(PSIn pin) : SV_TARGET {
    float3 L = normalize(lightDir);
    float NdotL = max(0.0, dot(pin.nrmWS, -L)); // luz direccional
    float ambient = 0.15;                        // un toque de ambiente
    float lambert = saturate(ambient + NdotL);
    return float4(pin.col * lambert, 1.0);
}
)";

//--------------------------------------------------------------------------------------
// DX helpers
//--------------------------------------------------------------------------------------
void WaitForGPU()
{
    const UINT64 fenceToWait = ++g_fenceValue;
    ThrowIfFailed(g_cmdQueue->Signal(g_fence.Get(), fenceToWait));
    if (g_fence->GetCompletedValue() < fenceToWait) {
        ThrowIfFailed(g_fence->SetEventOnCompletion(fenceToWait, g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

void Transition(ID3D12GraphicsCommandList* cl, ID3D12Resource* res,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = res;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &b);
}

DXGI_FORMAT ChooseBackbufferFormat() { return DXGI_FORMAT_R8G8B8A8_UNORM; }
DXGI_FORMAT ChooseDepthFormat() { return DXGI_FORMAT_D32_FLOAT; }

//--------------------------------------------------------------------------------------
// Create Window
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

void CreateAppWindow(HINSTANCE hInst)
{
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"DX12CubeWndClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    RECT rc = { 0,0,(LONG)Width,(LONG)Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindow(wc.lpszClassName, L"DX12 Rotating Cube (no d3dx12.h)",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hWnd, SW_SHOW);
}

//--------------------------------------------------------------------------------------
// Init DX12 core
//--------------------------------------------------------------------------------------
void CreateFactoryAndDevice()
{
#if _DEBUG
    {
        ComPtr<ID3D12Debug> dbg;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dbg))))
            dbg->EnableDebugLayer();
    }
#endif

    UINT flags = 0;
#if _DEBUG
    flags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    ThrowIfFailed(CreateDXGIFactory2(flags, IID_PPV_ARGS(&g_factory)));

    // Device (preferir hardware adapter)
    ComPtr<IDXGIAdapter1> adapter;
    for (UINT i = 0; g_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device))))
            break;
    }
    if (!g_device) {
        // WARP fallback
        ComPtr<IDXGIAdapter> warp;
        ThrowIfFailed(g_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)));
    }

    // Command queue
    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(g_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&g_cmdQueue)));
}

void CreateSwapchainAndRTVs()
{
    // Swap chain
    ComPtr<IDXGISwapChain1> sc1;
    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.BufferCount = FrameCount;
    sd.Width = Width;
    sd.Height = Height;
    sd.Format = ChooseBackbufferFormat();
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.SampleDesc.Count = 1;

    ThrowIfFailed(g_factory->CreateSwapChainForHwnd(
        g_cmdQueue.Get(), g_hWnd, &sd, nullptr, nullptr, &sc1));

    ThrowIfFailed(sc1.As(&g_swapChain));
    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    // RTV heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = FrameCount;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(g_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_rtvHeap)));
    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Crear RTVs
    //CD3DX12_CPU_DESCRIPTOR_HANDLE; // dummy to remind - not available; manual handle math below
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < FrameCount; ++i)
    {
        ThrowIfFailed(g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_renderTargets[i])));
        g_device->CreateRenderTargetView(g_renderTargets[i].Get(), nullptr, rtvHandle);
        rtvHandle.ptr += g_rtvDescriptorSize;
        ThrowIfFailed(g_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_cmdAlloc[i])));
    }
}

void CreateDepthBuffer()
{
    // DSV heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(g_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&g_dsvHeap)));
    g_dsvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // Depth texture
    D3D12_RESOURCE_DESC tex = {};
    tex.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex.Alignment = 0;
    tex.Width = Width;
    tex.Height = Height;
    tex.DepthOrArraySize = 1;
    tex.MipLevels = 1;
    tex.Format = ChooseDepthFormat();
    tex.SampleDesc = { 1, 0 };
    tex.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    tex.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE clear = {};
    clear.Format = ChooseDepthFormat();
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;

    ThrowIfFailed(g_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &tex,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
        IID_PPV_ARGS(&g_depthTex)));

    // DSV
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = ChooseDepthFormat();
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Flags = D3D12_DSV_FLAG_NONE;
    g_device->CreateDepthStencilView(g_depthTex.Get(), &dsv, g_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void CreateCmdListAndFence()
{
    ThrowIfFailed(g_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_cmdAlloc[g_frameIndex].Get(), nullptr, IID_PPV_ARGS(&g_cmdList)));
    ThrowIfFailed(g_cmdList->Close()); // arrancamos cerrada

    ThrowIfFailed(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
    g_fenceValue = 0;
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
}

//--------------------------------------------------------------------------------------
// RootSig + PSO
//--------------------------------------------------------------------------------------
void CreateRootSigAndPSO()
{
    // Root parameter: 1 CBV (b0)
    D3D12_ROOT_PARAMETER rootParams[1] = {};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Root signature flags
    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters = 1;
    rsDesc.pParameters = rootParams;
    rsDesc.NumStaticSamplers = 0;
    rsDesc.pStaticSamplers = nullptr;
    rsDesc.Flags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob));
    ThrowIfFailed(g_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&g_rootSig)));

    // Compilar shaders
    UINT compileFlags =
#if _DEBUG
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        0;
#endif
    ComPtr<ID3DBlob> vs, ps;
    ThrowIfFailed(D3DCompile(g_vsHlsl, strlen(g_vsHlsl), nullptr, nullptr, nullptr, "main", "vs_5_0", compileFlags, 0, &vs, &errBlob));
    ThrowIfFailed(D3DCompile(g_psHlsl, strlen(g_psHlsl), nullptr, nullptr, nullptr, "main", "ps_5_0", compileFlags, 0, &ps, &errBlob));

    // Input layout
    D3D12_INPUT_ELEMENT_DESC il[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,pos),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,color),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // Rasterizer/Depth/Blend
    D3D12_RASTERIZER_DESC rast = {};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_BACK;
    rast.FrontCounterClockwise = FALSE;
    rast.DepthClipEnable = TRUE;

    D3D12_DEPTH_STENCIL_DESC dss = {};
    dss.DepthEnable = TRUE;
    dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dss.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    dss.StencilEnable = FALSE;

    D3D12_BLEND_DESC blend = {};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    auto& rt = blend.RenderTarget[0];
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // PSO
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso = {};
    pso.pRootSignature = g_rootSig.Get();
    pso.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    pso.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    pso.BlendState = blend;
    pso.SampleMask = UINT_MAX;
    pso.RasterizerState = rast;
    pso.DepthStencilState = dss;
    pso.InputLayout = { il, _countof(il) };
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.NumRenderTargets = 1;
    pso.RTVFormats[0] = ChooseBackbufferFormat();
    pso.DSVFormat = ChooseDepthFormat();
    pso.SampleDesc.Count = 1;
    ThrowIfFailed(g_device->CreateGraphicsPipelineState(&pso, IID_PPV_ARGS(&g_pso)));
}

//--------------------------------------------------------------------------------------
// Geometry (cube) + CB
//--------------------------------------------------------------------------------------
void CreateCubeGeometryAndCB()
{
    // Cubo unitario centrado
    const float s = 0.5f;

    // 24 vértices (4 por cara) con normal plana por cara
    Vertex v[] = {
        // Frente (z+), n=(0,0,1)
        {{-s,-s, s},{1,0,0},{0,0,1}}, {{ s,-s, s},{0,1,0},{0,0,1}},
        {{ s, s, s},{0,0,1},{0,0,1}}, {{-s, s, s},{1,1,0},{0,0,1}},
        // Atrás (z-), n=(0,0,-1)
        {{-s,-s,-s},{1,0,1},{0,0,-1}}, {{-s, s,-s},{0,1,1},{0,0,-1}},
        {{ s, s,-s},{1,1,1},{0,0,-1}}, {{ s,-s,-s},{0,0,0},{0,0,-1}},
        // Izquierda (x-), n=(-1,0,0)
        {{-s,-s,-s},{1,0,1},{-1,0,0}}, {{-s,-s, s},{1,0,0},{-1,0,0}},
        {{-s, s, s},{1,1,0},{-1,0,0}}, {{-s, s,-s},{0,1,1},{-1,0,0}},
        // Derecha (x+), n=(1,0,0)
        {{ s,-s, s},{0,1,0},{1,0,0}}, {{ s,-s,-s},{0,0,0},{1,0,0}},
        {{ s, s,-s},{1,1,1},{1,0,0}}, {{ s, s, s},{0,0,1},{1,0,0}},
        // Arriba (y+), n=(0,1,0)
        {{-s, s, s},{1,1,0},{0,1,0}}, {{ s, s, s},{0,0,1},{0,1,0}},
        {{ s, s,-s},{1,1,1},{0,1,0}}, {{-s, s,-s},{0,1,1},{0,1,0}},
        // Abajo (y-), n=(0,-1,0)
        {{-s,-s,-s},{1,0,1},{0,-1,0}}, {{ s,-s,-s},{0,0,0},{0,-1,0}},
        {{ s,-s, s},{0,1,0},{0,-1,0}}, {{-s,-s, s},{1,0,0},{0,-1,0}},
    };

    uint16_t i[] = {
        // 6 caras * 2 triángulos
        0,1,2, 0,2,3,       // frente
        4,5,6, 4,6,7,       // atrás
        8,9,10, 8,10,11,    // izquierda
        12,13,14, 12,14,15, // derecha
        16,17,18, 16,18,19, // arriba
        20,21,22, 20,22,23  // abajo
    };

    const UINT vbSize = sizeof(v);
    const UINT ibSize = sizeof(i);

    // VB (upload para simplificar)
    {
        D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = vbSize; rd.Height = 1; rd.DepthOrArraySize = 1;
        rd.MipLevels = 1; rd.SampleDesc = { 1,0 }; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(g_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_vb)));

        void* data = nullptr;
        D3D12_RANGE rr = { 0,0 };
        ThrowIfFailed(g_vb->Map(0, &rr, &data));
        memcpy(data, v, vbSize);
        g_vb->Unmap(0, nullptr);

        g_vbView.BufferLocation = g_vb->GetGPUVirtualAddress();
        g_vbView.StrideInBytes = sizeof(Vertex);
        g_vbView.SizeInBytes = vbSize;
    }

    // IB (upload)
    {
        D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = ibSize; rd.Height = 1; rd.DepthOrArraySize = 1;
        rd.MipLevels = 1; rd.SampleDesc = { 1,0 }; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(g_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_ib)));

        void* data = nullptr;
        D3D12_RANGE rr = { 0,0 };
        ThrowIfFailed(g_ib->Map(0, &rr, &data));
        memcpy(data, i, ibSize);
        g_ib->Unmap(0, nullptr);

        g_ibView.BufferLocation = g_ib->GetGPUVirtualAddress();
        g_ibView.Format = DXGI_FORMAT_R16_UINT;
        g_ibView.SizeInBytes = ibSize;
    }

    // Constant Buffer (mapeado persistente)
    {
        UINT cbSize = Align256(sizeof(CBData));
        D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = cbSize; rd.Height = 1; rd.DepthOrArraySize = 1;
        rd.MipLevels = 1; rd.SampleDesc = { 1,0 }; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(g_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_cb)));

        D3D12_RANGE rr = { 0,0 };
        ThrowIfFailed(g_cb->Map(0, &rr, reinterpret_cast<void**>(&g_cbMapped)));
    }
}

//--------------------------------------------------------------------------------------
// Update + Record + Present
//--------------------------------------------------------------------------------------
void UpdateCB()
{
    // Tiempo
    auto t1 = std::chrono::high_resolution_clock::now();
    float seconds = std::chrono::duration<float>(t1 - g_t0).count();

    // Matrices
    XMMATRIX mWorld =
        XMMatrixRotationX(seconds * 0.7f) *
        XMMatrixRotationY(seconds * 1.1f);

    XMMATRIX mvp = mWorld * g_view * g_proj;

    // luz direccional fija en mundo (arriba-derecha-atrás)
    XMVECTOR L = XMVector3Normalize(XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));


    CBData cb; 
    cb.mvp = XMMatrixTranspose(mvp);
    cb.world = mWorld;                 
    XMStoreFloat3(&cb.lightDir, L);

    *g_cbMapped = cb;
}

void RecordRender()
{
    ThrowIfFailed(g_cmdAlloc[g_frameIndex]->Reset());
    ThrowIfFailed(g_cmdList->Reset(g_cmdAlloc[g_frameIndex].Get(), g_pso.Get()));

    // Seteo estado
    g_cmdList->SetGraphicsRootSignature(g_rootSig.Get());
    g_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_cmdList->IASetVertexBuffers(0, 1, &g_vbView);
    g_cmdList->IASetIndexBuffer(&g_ibView);
    g_cmdList->SetGraphicsRootConstantBufferView(0, g_cb->GetGPUVirtualAddress());
    g_cmdList->RSSetViewports(1, &g_viewport);
    g_cmdList->RSSetScissorRects(1, &g_scissorRect);

    auto bb = g_renderTargets[g_frameIndex].Get();

    // Transition BB a Render Target
    Transition(g_cmdList.Get(), bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // RTV/DSV handles
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += g_frameIndex * g_rtvDescriptorSize;
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = g_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    // Clear
    const float clearColor[4] = { 0.07f, 0.1f, 0.16f, 1.0f };
    g_cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    g_cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Bind RT/DS
    g_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    // Draw
    g_cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0);

    // Transition a Present
    Transition(g_cmdList.Get(), bb, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    ThrowIfFailed(g_cmdList->Close());
}

void Present()
{
    ThrowIfFailed(g_swapChain->Present(1, 0)); // vsync

    // Avanzar frame + fence
    const UINT64 fenceToSignal = ++g_fenceValue;
    ThrowIfFailed(g_cmdQueue->Signal(g_fence.Get(), fenceToSignal));

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    if (g_fence->GetCompletedValue() < fenceToSignal) {
        ThrowIfFailed(g_fence->SetEventOnCompletion(fenceToSignal, g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

//--------------------------------------------------------------------------------------
// Resize (si quisieras agregarlo luego) – omitimos WM_SIZE para mantener simple.
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Init de matrices de cámara
//--------------------------------------------------------------------------------------
void InitCamera()
{
    XMVECTOR eye = XMVectorSet(1.5f, 1.2f, -2.0f, 0.0f);
    XMVECTOR at = XMVectorSet(0, 0, 0, 0);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    g_view = XMMatrixLookAtLH(eye, at, up);
    g_proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.f), float(Width) / float(Height), 0.1f, 100.0f);
}

//--------------------------------------------------------------------------------------
// Main
//--------------------------------------------------------------------------------------
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int)
{
    CreateAppWindow(hInst);
    CreateFactoryAndDevice();
    CreateSwapchainAndRTVs();
    CreateDepthBuffer();
    CreateCmdListAndFence();
    CreateRootSigAndPSO();
    CreateCubeGeometryAndCB();
    InitCamera();

    // Loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0u, 0u, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            UpdateCB();
            ID3D12CommandList* lists[] = { g_cmdList.Get() };
            RecordRender();
            g_cmdQueue->ExecuteCommandLists(1, lists);
            Present();
        }
    }

    WaitForGPU();
    CloseHandle(g_fenceEvent);
    return 0;
}
