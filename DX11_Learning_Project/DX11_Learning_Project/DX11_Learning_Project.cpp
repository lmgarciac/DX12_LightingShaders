#include <windows.h>
#include <d3d11.h>
#pragma comment(lib, "d3d11.lib")

#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

// Globals
HWND g_hWnd = nullptr;
ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swapChain = nullptr;
ID3D11RenderTargetView* g_renderTargetView = nullptr;

struct Vertex {
    float x, y, z;        // posición (clip space)
    float r, g, b, a;     // color
};

ID3D11Buffer* g_vertexBuffer = nullptr;

ID3D11InputLayout* g_inputLayout = nullptr;
ID3D11VertexShader* g_vertexShader = nullptr;
ID3D11PixelShader* g_pixelShader = nullptr;


// Vertex Shader (transforma vértices, por ahora identidad)
const char* g_VSCode =
"struct VS_IN { \
    float3 pos : POSITION; \
    float4 col : COLOR; \
}; \
struct PS_IN { \
    float4 pos : SV_POSITION; \
    float4 col : COLOR; \
}; \
PS_IN main(VS_IN input) { \
    PS_IN output; \
    output.pos = float4(input.pos, 1.0); \
    output.col = input.col; \
    return output; \
}";

// Pixel Shader (simplemente devuelve el color del vértice)
const char* g_PSCode =
"struct PS_IN { \
    float4 pos : SV_POSITION; \
    float4 col : COLOR; \
}; \
float4 main(PS_IN input) : SV_TARGET { \
    return input.col; \
}";


bool CreateTriangleVertexBuffer()
{
    // Triángulo en clip space (NDC): arriba rojo, abajo derecha verde, abajo izquierda azul
    const Vertex vertices[3] = {
        {  0.0f,  0.5f, 0.0f,   1.f, 0.f, 0.f, 1.f }, // top (rojo)
        {  0.5f, -0.5f, 0.0f,   0.f, 1.f, 0.f, 1.f }, // bottom-right (verde)
        { -0.5f, -0.5f, 0.0f,   0.f, 0.f, 1.f, 1.f }  // bottom-left (azul)
    };

    D3D11_BUFFER_DESC bd = {};
    bd.Usage = D3D11_USAGE_IMMUTABLE;              // datos fijos (no los vamos a actualizar)
    bd.ByteWidth = sizeof(vertices);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;       // es un vertex buffer
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = vertices;

    HRESULT hr = g_device->CreateBuffer(&bd, &initData, &g_vertexBuffer);
    return SUCCEEDED(hr);
}

// Window Procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Init DirectX
bool InitD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        0,
        nullptr,
        0,
        D3D11_SDK_VERSION,
        &scd,
        &g_swapChain,
        &g_device,
        nullptr,
        &g_context
    );

    if (FAILED(hr)) return false;

    ID3D11Texture2D* backBuffer = nullptr;
    g_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
    g_device->CreateRenderTargetView(backBuffer, nullptr, &g_renderTargetView);
    backBuffer->Release();

    g_context->OMSetRenderTargets(1, &g_renderTargetView, nullptr);

    D3D11_VIEWPORT vp = {};
    vp.Width = 800;
    vp.Height = 600;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    g_context->RSSetViewports(1, &vp);

    if (!CreateTriangleVertexBuffer()) {
        return false; // si falla, abortamos init
    }

    //Compile shaders

    ID3DBlob* vsBlob = nullptr;
    ID3DBlob* psBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    // Compile Vertex Shader
    hr = D3DCompile(g_VSCode, strlen(g_VSCode), nullptr, nullptr, nullptr,
        "main", "vs_4_0", 0, 0, &vsBlob, &errorBlob);
    if (FAILED(hr)) return false;

    g_device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &g_vertexShader);

    // Compile Pixel Shader
    hr = D3DCompile(g_PSCode, strlen(g_PSCode), nullptr, nullptr, nullptr,
        "main", "ps_4_0", 0, 0, &psBlob, &errorBlob);
    if (FAILED(hr)) return false;

    g_device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &g_pixelShader);


    //Create Input Layout

    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
          D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    g_device->CreateInputLayout(
        layoutDesc,
        ARRAYSIZE(layoutDesc),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        &g_inputLayout
    );

    vsBlob->Release();
    psBlob->Release();


    return true;
}

// Clean up
void CleanD3D() {
    if (g_renderTargetView) g_renderTargetView->Release();
    if (g_swapChain) g_swapChain->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
    if (g_vertexBuffer) g_vertexBuffer->Release();
    if (g_inputLayout) g_inputLayout->Release();
    if (g_vertexShader) g_vertexShader->Release();
    if (g_pixelShader) g_pixelShader->Release();
}

// Render loop
void RenderFrame() {
    float clearColor[4] = { 0.0f, 0.2f, 0.4f, 1.0f }; // dark blue
    g_context->ClearRenderTargetView(g_renderTargetView, clearColor);


    // --- Bind del pipeline ---

// Input Layout
    g_context->IASetInputLayout(g_inputLayout);

    // Vertex Buffer
    UINT stride = sizeof(Vertex); // tamaño de cada vértice
    UINT offset = 0;
    g_context->IASetVertexBuffers(0, 1, &g_vertexBuffer, &stride, &offset);

    // Topología de primitiva (triángulos)
    g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Shaders
    g_context->VSSetShader(g_vertexShader, nullptr, 0);
    g_context->PSSetShader(g_pixelShader, nullptr, 0);

    // --- Draw call ---
    g_context->Draw(3, 0);

    g_swapChain->Present(1, 0);
}

// Entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // Register window class
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = L"DX11WindowClass";
    RegisterClass(&wc);

    // Create window
    g_hWnd = CreateWindow(wc.lpszClassName, L"DX11 Hello World",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600, nullptr, nullptr, hInstance, nullptr);

    ShowWindow(g_hWnd, nCmdShow);

    // Init D3D
    if (!InitD3D(g_hWnd)) return -1;

    // Main loop
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            RenderFrame();
        }
    }

    // Cleanup
    CleanD3D();
    return 0;
}
