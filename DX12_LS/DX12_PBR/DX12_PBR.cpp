// main.cpp - DX12 Rotating Cube (Win32, x64)
// Compilar con: d3d12.lib, dxgi.lib, d3dcompiler.lib
// Requiere Windows 10 SDK+
// No usa d3dx12.h (todo structs/manual)

//--------------------------------------------------------------------------------------
// INSTRUCCIONES
//--------------------------------------------------------------------------------------

// PRESIONAR T PARA CAMBIAR ENTRE MODOS DE ILUMINACION

// 0 = Unlit                         // Sin iluminación: muestra solo el color base (debug de albedo).
// 1 = Ambient (placeholder)         // Luz ambiente homogénea (aprox.) para no dejar todo negro sin luces directas.
// 2 = Difuso Lambert only           // Solo componente difusa (Lambert): color “mate” según el ángulo N·L.
// 3 = Especular PBR only            // Solo brillo especular (Cook–Torrance GGX + Fresnel).
// 4 = Direct PBR (difuso+spec)      // Luz directa PBR (difuso + especular), sin ambiente/IBL.
// 5 = PBR completo básico           // Ambiente simple (placeholder) + luz directa PBR. (Luego se reemplaza por IBL).

// PRESIONAR M para alternar valores de Metallic
// Nota: Metallic representa si el material es dieléctrico (≈0) o metálico (≈1).
//       0 → hay difuso y el especular tiene F0 bajo (≈0.04, neutro).
//       1 → no hay difuso y el especular se tiñe con el baseColor (F0 alto y coloreado).

// PRESIONAR R para alternar valores de Roughness
// Nota: Roughness controla la microrrugosidad de la superficie.
//       Bajo → highlight chico y brillante (tipo espejo).
//       Alto → highlight ancho y tenue (superficie mate).

// PRESIONAR A para alternar valores de Ambient Occlusion
// Nota: Ambient Occlusion atenúa SOLO la luz ambiental/IBL en oquedades y contactos.
//       No afecta la luz directa; oscurece “huecos” donde el ambiente llega menos.

// PRESIONAR P para frenar la rotación del Cubo

// PRESIONAR G para alternar entre cubo y esfera

// PRESIONAR F para fijar la luz frente a la cámara

//--------------------------------------------------------------------------------------
// Ayuda teórica para DX12:
//--------------------------------------------------------------------------------------

// Render pipeline:
// Pasos secuenciales que transforman datos 3D (como vértices y texturas) en imagen final que se ve en la pantalla.
// Tipicamente:
// 1. Etapa de Entrada y Ensamblado (Input Assembler): Lee los datos del modelo
// 2. Vertex Shader: Aplica la matriz MVP a cada vértice para transformarlo del espacio local al clip space. 
//    También puede calcular datos por vértice como iluminación básica o coordenadas de textura.
// 3. Etapas Intermedias: 
//    a) Tesselation: Divide las primitivas grandes en muchas más pequeñas para añadir detalle dinámicamente.
//    b) Geometry Shader : Puede crear o destruir geometría sobre la marcha.
// 4. Rasterization: Convierte las primitivas geométricas (triángulos) en fragmentos (pixeles potenciales) que cubren la pantalla.
//                   Determina qué píxeles están cubiertos por cada triángulo y prepara datos interpolados(como el color o la coordenada UV)
//                   para el siguiente paso
// 5. Fragment Shader / Pixel Shader: Se ejecuta por cada pixel potencial. Determina el color final del píxel, utilizando texturas, 
//                                    iluminación compleja, cálculos de sombreado y parámetros de material.
// 6. Output Merger & Test: 
// a) Combina el píxel recién calculado con el píxel que ya existe en el búfer de imagen (el back buffer de la swap chain).
// b) Realiza pruebas de profundidad (Z-Buffering) y blending (mezcla de colores para la transparencia).
 
// Swap chain: 
// Front Buffer + Back Buffer

// DXGI: 
// DirectX Graphics Infrastructure. Puente entre la API de gráficos y 
// los controladores de hardware con el sistema operativo.

// Constant Buffer View (CBV):
// Buffer de memoria que puede ver el GPU (similar a un buffer de uniforms)
// Ejemplo: (matrices, colores, posiciones, otros parámetros).
// Un CBV solo puede apuntar a un buffer lineal con datos uniformes.

// Shader Resource View (SRV):
// Vista que permite al shader leer un recurso.
// Textura, cubemap, buffer, etc.
// Un SRV puede apuntar a recursos arbitrarios (texturas, buffers estructurados, MIP maps, arrays, etc.) y tiene lógica de muestreo.

// Matriz Model - View - Projection (MVP): 
// Convierte las coordenadas de los vértices de un modelo desde su espacio local (donde se define el objeto) 
// hasta el espacio de recorte (clip space), momento antes de ser proyectadas en la pantalla
// MVP = Proyeccion x Vista x Modelo

// Model Matrix: 
// Transforma las coordenadas de los vértices del objeto desde su espacio local 
// u objeto (model space), donde suele estar centrado en el origen, al espacio global o mundo (world space)

// View Matrix: 
// Transforma las coordenadas del espacio mundo al espacio de vista o espacio de cámara

// Projection Matrix: 
// Responsable de la perspectiva o de una proyección ortográfica

// Descriptor: 
// Una pequeña estructura en memoria (puntero + metadata) que describe a la GPU cómo debe interpretar un recurso (p ej. una textura).

// Descriptor RTV (Render Target View): 
// "Esta textura se puede usar como Render Target y así se la interpreta"

// DSV (Depth-Stencil View):
// “Esta textura sirve como depth buffer + stencil (si tuviera).”

// Descriptor Heap:
// Un array contiguo en memoria donde se guardan muchos descriptores.

// Recurso (ID3D12Resource):
// Bloque de memoria en la GPU que puede ser una textura, un buffer, un render target, depth buffer, vertex buffer, index buffer, constant buffer, etc.

// DEPTH (Z-buffer): Textura donde la GPU guarda, para cada píxel de la pantalla qué tan lejos está el fragmento de la cámara
//                   Sirve para realizar el Depth Test, que decide si un píxel debe dibujarse o descartarse.

// STENCIL Buffer: Guarda un valor entero por píxel (generalmente 8 bits).
//                 Sirve para efectos de enmascaramiento por píxel, por ejemplo: Portales, Dibujar sólo dentro de cierta región

// Tipos de memoria DX12:
// DX12 tiene tipos de memoria (heaps)
// a) DEFAULT: Optimizado para acceso por GPU. Para Texturas, vertex buffers finales, render targets
// b) UPLOAD: Optimizado para escrituras frecuentes de la CPU. Para	Constant buffers, staging, datos que la CPU escribe cada frame
// c) READBACK: Optimizado para lecturas eficientes por parte de la CPU. Para leer resultados que vienen desde la GPU

//--------------------------------------------------------------------------------------
// Ayuda teórica para PBR:
//--------------------------------------------------------------------------------------

// Cook-Torrance es: 
// Un modelo físico de iluminación especular. Describe cómo una superficie refleja la luz
// considerando micro-facetas (pequeñas inclinaciones), auto-sombras y reflejos angulares realistas.
// Es la base del PBR moderno (Physically-Based Rendering).

// GGX es: 
// Una función de distribución de micro-facetas usada dentro del modelo Cook-Torrance.
// Define cuántas micro-facetas están alineadas con la luz. Produce reflejos más suaves y realistas
// que modelos antiguos (como Phong o Beckmann).

// Fresnel es: 
// Un fenómeno óptico real que determina cuánta luz se refleja dependiendo del ángulo de visión.
// A ángulos rasantes, toda superficie refleja más. En PBR se usa la aproximación de Schlick,
// que simula este comportamiento de forma barata y eficiente.

// IBL es: 
// Image-Based Lighting. Es una técnica que usa un mapa del entorno (cubemap o panorama)
// para aportar iluminación ambiental difusa y especular realista,
// reemplazando la "ambient light" plana de los modelos clásicos.

// F0 es: 
// “Reflectancia en incidencia normal”. Indica cuánto refleja un material cuando la luz incide de frente.
// Los materiales no metálicos (dieléctricos) tienen un F0 bajo (~0.04), 
// mientras que los metales tienen un F0 alto y del color del material.

// Oquedades y contactos es: 
// Regiones donde la luz ambiental casi no llega (por ejemplo, un pliegue o la unión entre objetos).
// En PBR se simulan con el mapa de Ambient Occlusion (AO), que oscurece esas zonas.

//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Inicio: Includes, configuración global y tipos(Vertex, CBData, globals DX12, helpers)
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h> //Main API
#include <dxgi1_6.h> //DXGI: swap chains, enumerar adaptadores (GPU), formatos, presentación
#include <d3dcompiler.h> // Compilador de shaders HLSL
#include <DirectXMath.h>
#include <string>
#include <vector>
#include <chrono>
#include <cassert>
#include <cstdio> // sprintf_s

//Assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;

//--------------------------------------------------------------------------------------
// Config
//--------------------------------------------------------------------------------------
static const UINT FrameCount = 2; // Cantidad de back buffers del swap chain.
static const UINT Width = 1280;
static const UINT Height = 720;

int g_mode = 5;

//--------------------------------------------------------------------------------------
// Util
//--------------------------------------------------------------------------------------

// Macro heredado para liberar interfaces COM manualmente (no usado porque ComPtr libera solo)
#ifndef SAFE_RELEASE
#define SAFE_RELEASE(p) if(p){ (p)->Release(); (p)=nullptr; }
#endif

//Safety assert (debug only)
inline void ThrowIfFailed(HRESULT hr) { if (FAILED(hr)) { assert(false); ExitProcess((UINT)hr); } } 

// Alinear a múltiplos de 256 para (Constant Buffer Views - CBVs)
// DirectX 12 exige que todo Constant Buffer View tenga un tamaño múltiplo de 256 bytes
inline UINT Align256(UINT size) { return (size + 255) & ~255u; }

//--------------------------------------------------------------------------------------
// Vertex / Const Buffer Definition
//--------------------------------------------------------------------------------------
struct Vertex {
    XMFLOAT3 pos; //Espacio local
    XMFLOAT3 color; //RGB
    XMFLOAT3 normal;
};


// Definición del CBV - un CBV es un bloque de memoria en el GPU que tiene valores constantes por draw y es accesible desde shaders.
// Sería como un buffer de uniforms en OpenGL.
// Se usa alignas porque quiero que esta struct comience en memoria en una dirección múltiplo de 256 bytes.
// Esto es una regla de DX12.

struct alignas(256) CBData {
    // Aclaración: Cada variable dentro de un cbuffer debe ocupar un múltiplo de 16 bytes (Constant buffer packing rules):

    XMMATRIX mvp; // Matriz Model - View - Projection (XMMATRIX is auto aligned on a 16-byte boundary)
    XMMATRIX world; // Matriz World: necesaria para transformar normales y posición en PBR.
    
    XMFLOAT3 lightDir; // (12 bytes)
    float    ambient; // Tal vez no se use en PBR, queda para compatibilidad (4 bytes)
    
    int      mode; // Modo 1-5 (4 bytes)
    XMFLOAT3 _pad1; // (padding de 12 bytes)

    XMFLOAT3 viewPos;  // Posición de la cámara en mundo (p/ PBR y Fresnel).
    float    shininess; // Para especular
    float    specIntensity; // Para especular 
    XMFLOAT3 _pad2;

    XMFLOAT3 baseColor; // baseColor: F0 para metales y albedo difuso para dieléctricos.
    float metallic;  // metallic: 0 = dieléctrico, 1 = metal.
    float roughness; // controla la microrrugosidad de la superficie.
    float ao; // atenúa SOLO la luz ambiental en oquedades y contactos.
    XMFLOAT2 _pad3;

    // luz puntual física
    XMFLOAT3 lightPos;     
    float lightIntensity; // intensidad en unidades arbitrarias
    XMFLOAT3 lightColor;  // RGB de la luz
    float _pad4;          
};

//--------------------------------------------------------------------------------------
// Globals DX12
//--------------------------------------------------------------------------------------
HWND                                g_hWnd = nullptr; //Handler de la ventana
UINT                                g_frameIndex = 0; //Indice del backbuffer actual
UINT                                g_rtvDescriptorSize = 0; //Tamaño en bytes entre descriptores RTV dentro del descriptor heap (lo da el device)
UINT                                g_dsvDescriptorSize = 0; //Tamaño en bytes entre descriptores DSV dentro del descriptor heap (lo da el device)
D3D12_VIEWPORT                      g_viewport = { 0.0f, 0.0f, (float)Width, (float)Height, 0.0f, 1.0f }; // Viewport de rasterización: área de la pantalla donde se dibuja
D3D12_RECT                          g_scissorRect = { 0, 0, (LONG)Width, (LONG)Height }; //Rectángulo de scissor: recorta el dibujo a esta región

// Objetos base de la plataforma DX12 (device, factory, swapchain, cola, etc.)

ComPtr<IDXGIFactory7>               g_factory; // Factory DXGI: permite enumerar adaptadores (GPU) y crear el swap chain.
ComPtr<ID3D12Device>                g_device; // Device de D3D12: representa la conexión lógica con la GPU. Crea recursos (buffers, textures, heaps, PSO, etc.).
ComPtr<ID3D12CommandQueue>          g_cmdQueue; // Command Queue: cola donde se envían command lists ya grabadas para que la GPU las ejecute.
ComPtr<IDXGISwapChain3>             g_swapChain; // Swap chain: conjunto de backbuffers que se alternan entre render y presentación en pantalla.
ComPtr<ID3D12DescriptorHeap>        g_rtvHeap; // Descriptor heap que contiene los descriptores RTV (Render Target View) de los backbuffers.
ComPtr<ID3D12Resource>              g_renderTargets[FrameCount]; // Recursos de GPU para cada backbuffer del swap chain (las texturas donde se dibuja el frame).
ComPtr<ID3D12CommandAllocator>      g_cmdAlloc[FrameCount]; // Command allocator por frame: administra la memoria donde se graban las commands de la command list.
ComPtr<ID3D12GraphicsCommandList>   g_cmdList; // Command list de tipo gráfico: se graban aquí las órdenes de dibujo (set pipeline, draw, clears, etc.).
ComPtr<ID3D12Fence>                 g_fence; // Fence de sincronización CPU/GPU: permite saber cuándo la GPU terminó de procesar comandos.
UINT64                              g_fenceValue = 0; // Contador asociado a la fence. Cada submit incrementa este valor para trackear el progreso.
HANDLE                              g_fenceEvent = nullptr; // Event de Win32 usado para bloquear la CPU hasta que la fence llegue a un valor dado.

// Recursos de depth/stencil

ComPtr<ID3D12DescriptorHeap>        g_dsvHeap; // Descriptor heap para el DSV (depth-stencil view).
ComPtr<ID3D12Resource>              g_depthTex; // Textura de depth (Z-buffer) donde se almacena la información de profundidad del frame.

// Root Signature + PSO (estado de pipeline)

ComPtr<ID3D12RootSignature>         g_rootSig; // Root signature: describe qué recursos (CBVs (constant buffer view), SRVs (shader resolution view), samplers, etc.) puede ver el shader
                                               // y cómo se van a enlazar (root parameters, descriptor tables, etc.).
ComPtr<ID3D12PipelineState>         g_pso; // Pipeline State Object: estado completo del pipeline gráfico (shaders, input layout,
                                           // rasterizer, depth-stencil, blend, formatos RT/DS, topology, etc.).

// Constants buffer
ComPtr<ID3D12Resource>              g_cb;    // Recurso de tipo buffer usado como constant buffer (CBV). Está en memoria UPLOAD y se deja mapeado.
                                             // para poder escribirle datos cada frame.

CBData* g_cbMapped = nullptr; // Puntero CPU al constant buffer mapeado. Cada frame se escribe CBData aquí y la GPU lo lee en el shader.

XMMATRIX                            g_proj; // Matriz de proyección (perspectiva).
XMMATRIX                            g_view; // Matriz de vista (cámara).

XMFLOAT3 g_eyeWS; // Posición de la cámara en espacio mundo (world space). Se pasa al shader para PBR (Fresnel, view vector, etc.).



//--------------------------------------------------------------------------------------
// Parámetros de escena (material, luz, animación, selección de geometría)
//--------------------------------------------------------------------------------------

// Timing
auto g_t0 = std::chrono::high_resolution_clock::now();

// Presets de material y estado actual (para cambiar un runtime)
static const float g_metallicPresets[] = { 0.0f, 0.1f, 0.5f, 1.0f };
static const float g_roughnessPresets[] = { 0.08f, 0.35f, 0.6f, 0.9f };
static const float g_aoPresets[] = { 0.0f, 0.5f, 0.8f, 1.0f };

// Indices de presets
static int   g_metallicIdx = 0;
static int   g_roughnessIdx = 1;
static int   g_aoIdx = 3;

// Valores de presets
static float g_metallic = g_metallicPresets[g_metallicIdx];
static float g_roughness = g_roughnessPresets[g_roughnessIdx];
static float g_ao = g_aoPresets[g_aoIdx];

// Color base 
static XMFLOAT3 g_baseColor = XMFLOAT3(0.95f, 0.25f, 0.20f);

// Luz: parámetros base
static XMFLOAT3 g_lightColor = XMFLOAT3(1.0f, 1.0f, 1.0f);
static float    g_lightIntensity = 30.0f; //10 - 100 aprox

// control de rotación del cubo
static bool g_pauseRotation = false;
static float g_rotTime = 0.0f; // tiempo acumulado para la rotación del cubo
static std::chrono::high_resolution_clock::time_point g_prevTick; //ticker para control animación

// Buffers de geometría
ComPtr<ID3D12Resource>              g_vb; // Vertex buffer del cubo (posiciones, colores, normales).
ComPtr<ID3D12Resource>              g_ib; // Index buffer del cubo (definición de triángulos por índices).
D3D12_VERTEX_BUFFER_VIEW            g_vbView = {}; // Vista del vertex buffer: GPU address + stride (tamaño entre objetos) + tamaño total.
D3D12_INDEX_BUFFER_VIEW             g_ibView = {}; // Vista del index buffer: GPU address + formato (R16_UINT) + tamaño total.

// Creamos buffers de geometría adicionales para probar una esfera
ComPtr<ID3D12Resource> g_sphereVB;
ComPtr<ID3D12Resource> g_sphereIB;
D3D12_VERTEX_BUFFER_VIEW g_sphereVBView = {};
D3D12_INDEX_BUFFER_VIEW  g_sphereIBView = {};
UINT g_sphereIndexCount = 0;

// Creamos buffers de geometría adicionales para probar un modelo
ComPtr<ID3D12Resource> g_modelVB;
ComPtr<ID3D12Resource> g_modelIB;
D3D12_VERTEX_BUFFER_VIEW g_modelVBView = {};
D3D12_INDEX_BUFFER_VIEW  g_modelIBView = {};
UINT g_modelIndexCount = 0;

// Selector de geometría: 0=Cubo, 1=Esfera
static int g_geomMode = 0;

// Manipular fuente de luz
static bool  g_lightPinnedFront = false; // luz fija frente a cámara
static float g_lightFrontDist = 1.2f;  // distancia desde la cámara al origen

//--------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------
void WaitForGPU()
{
    //Bloquea la CPU hasta que la GPU termine todo lo que tiene pendiente en la command queue.
    const UINT64 fenceToWait = ++g_fenceValue;
    ThrowIfFailed(g_cmdQueue->Signal(g_fence.Get(), fenceToWait)); //Cuando se haya ejecutado todo lo que hay antes de este punto, marcar la fence con el valor

    if (g_fence->GetCompletedValue() < fenceToWait) { // La GPU ya alcanzó o pasó este valor de fence
        ThrowIfFailed(g_fence->SetEventOnCompletion(fenceToWait, g_fenceEvent)); //Registra un evento de Win32 que se disparará cuando la fence llegue a ese valor
        WaitForSingleObject(g_fenceEvent, INFINITE); //Bloquea el hilo actual (CPU) hasta que la GPU termine y la fence dispare el evento
    }
}

void Transition(ID3D12GraphicsCommandList* cl, ID3D12Resource* res,
    D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
    // En DX12 cada recurso tiene un “estado de uso”(READ, WRITE, PRESENT, etc.).
    // Esta función mete una barrera de transición para cambiar correctamente de un estado a otro
    
    D3D12_RESOURCE_BARRIER b = {};
    b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource = res;
    b.Transition.StateBefore = before;
    b.Transition.StateAfter = after;
    b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &b);
}

DXGI_FORMAT ChooseBackbufferFormat() { return DXGI_FORMAT_R8G8B8A8_UNORM; } //8 bits por canal(RGB) + alpha. Color “normalizado”[0..1].
DXGI_FORMAT ChooseDepthFormat() { return DXGI_FORMAT_D32_FLOAT; } //32 bits en float para profundidad.

void UpdateWindowTitle() //Just set title of window with current values
{
    wchar_t buffer[256];
    swprintf_s(buffer, L"DX12 PBR  |  Mode: %d  |  metallic=%.2f  roughness=%.2f  ao=%.2f",
        g_mode, g_metallic, g_roughness, g_ao); 
    SetWindowText(g_hWnd, buffer);
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Fin: Includes, configuración global y tipos(Vertex, CBData, globals DX12, helpers)
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------


//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Inicio: Plataforma y DX12 core
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Events / Windowing
//--------------------------------------------------------------------------------------

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) // Handle Window callbacks
{
    switch (msg)
    {
        case WM_DESTROY: PostQuitMessage(0); return 0; // Post WM_QUIT para finalizar end loop
        case WM_KEYDOWN: //Manejar inputs de usuario.
        {
            if (wParam == 'T') {
                g_mode = (g_mode + 1) % 6; // 0..5
                UpdateWindowTitle();
            }
            else if (wParam == 'M') {
                g_metallicIdx = (g_metallicIdx + 1) % (int)(sizeof(g_metallicPresets) / sizeof(float));
                g_metallic = g_metallicPresets[g_metallicIdx];
                UpdateWindowTitle();
            }
            else if (wParam == 'R') {
                g_roughnessIdx = (g_roughnessIdx + 1) % (int)(sizeof(g_roughnessPresets) / sizeof(float));
                g_roughness = g_roughnessPresets[g_roughnessIdx];
                UpdateWindowTitle();
            }
            else if (wParam == 'A') {
                g_aoIdx = (g_aoIdx + 1) % (int)(sizeof(g_aoPresets) / sizeof(float));
                g_ao = g_aoPresets[g_aoIdx];
                UpdateWindowTitle();
            }
            else if (wParam == 'P') {       // Pause/Resume cube rotation
                g_pauseRotation = !g_pauseRotation;
                UpdateWindowTitle();
            }
            else if (wParam == 'G') { // alternar geometría
                g_geomMode = (g_geomMode + 1) % 3; // 0..2
                UpdateWindowTitle();
            }
            else if (wParam == 'F') { // F = fijar/liberar luz frente a cámara
                g_lightPinnedFront = !g_lightPinnedFront;
                UpdateWindowTitle();
            }
            return 0;
        }
    }

    return DefWindowProc(hWnd, msg, wParam, lParam); //Deja que windows maneje el mensaje
}

void CreateAppWindow(HINSTANCE hInst)
{
    //Obtenemos un handler a una nueva ventana de windows en g_hWnd con una Width y un Height determinados

    WNDCLASS wc = {}; //Creamos tipo WNDCLASS
    wc.lpfnWndProc = WndProc; // Funcion handler de eventos
    wc.hInstance = hInst; // Handler a instancia
    wc.lpszClassName = L"DX12CubeWndClass"; // Nombre de clase a crear
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);

    RECT rc = { 0,0,(LONG)Width,(LONG)Height };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    g_hWnd = CreateWindow(wc.lpszClassName, L"DX12 Rotating Cube PBR (Press T to switch modes)",
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
    // Debug layer stuff, no funciona. Ver.
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

    // Crear DXGI factory para enumerar adaptadores.
    ThrowIfFailed(CreateDXGIFactory2(flags, IID_PPV_ARGS(&g_factory)));

    // Obtener Device (descarta adaptadores lógicos, solo hardware). Toma el primero.
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
        // WARP fallback (si no encuentro device uso WARP (rasterizador por software acelerado por CPU)
        ComPtr<IDXGIAdapter> warp;
        ThrowIfFailed(g_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp)));
        ThrowIfFailed(D3D12CreateDevice(warp.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&g_device)));
    }

    // Command queue. Crear la cola de comandos principal
    // Recibe command lists ya grabadas y las manda a la GPU en orden.

    D3D12_COMMAND_QUEUE_DESC qd = {};
    qd.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    qd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    ThrowIfFailed(g_device->CreateCommandQueue(&qd, IID_PPV_ARGS(&g_cmdQueue)));
}

void CreateSwapchainAndRTVs()
{
    // Creació de Swap chain (backbuffers) donde dibujar cada frame
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

    
    // RTV heap Render Target View     
    // Crear el descriptor heap RTV con FrameCount cantidad de descriptores.
    D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
    rtvDesc.NumDescriptors = FrameCount;
    rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(g_device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_rtvHeap)));
    g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV); //Guardo el stride

    //CD3DX12_CPU_DESCRIPTOR_HANDLE; // dummy to remind - not available; manual handle math below

    // Crea cada RTV.
    // Tambien crear un command allocator por frame buffer (Tener uno por frame permite grabar/ejecutar mientras otro está aún en uso por la GPU.)
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
    // Crea un descriptor heap de DSV con 1 descriptor
    D3D12_DESCRIPTOR_HEAP_DESC dsvDesc = {};
    dsvDesc.NumDescriptors = 1;
    dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(g_device->CreateDescriptorHeap(&dsvDesc, IID_PPV_ARGS(&g_dsvHeap)));
    g_dsvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV); //Guardo stride por si necesito más después.

    // Depth texture. Describo una textura 2D para depth
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

    //Determino Clear value por defecto
    D3D12_CLEAR_VALUE clear = {};
    clear.Format = ChooseDepthFormat();
    clear.DepthStencil.Depth = 1.0f;
    clear.DepthStencil.Stencil = 0;

    //Crea la textura de depth en DEFAULT memory (rápida para GPU).
    D3D12_HEAP_PROPERTIES hp = {};
    hp.Type = D3D12_HEAP_TYPE_DEFAULT;
    ThrowIfFailed(g_device->CreateCommittedResource(
        &hp, D3D12_HEAP_FLAG_NONE, &tex,
        D3D12_RESOURCE_STATE_DEPTH_WRITE, &clear,
        IID_PPV_ARGS(&g_depthTex)));

    // Crea un descriptor para Depth Stencil View (RTV)
    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = ChooseDepthFormat();
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Flags = D3D12_DSV_FLAG_NONE;
    g_device->CreateDepthStencilView(g_depthTex.Get(), &dsv, g_dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void CreateCmdListAndFence()
{
    // Crea la command list principal(g_cmdList) usando el command allocator del frame actual.

    ThrowIfFailed(g_device->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT, g_cmdAlloc[g_frameIndex].Get(), nullptr, IID_PPV_ARGS(&g_cmdList)));
    ThrowIfFailed(g_cmdList->Close()); // Arrancamos cerrada. Se cierra inmediatamente, porque el primer uso real la va a volver a abrir con Reset.

    // Crear fence usado para sincronización CPU/GPU en Present() y WaitForGPU().
    ThrowIfFailed(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence))); 
    g_fenceValue = 0;
    g_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr); //evento de Win32 asociado a la fence
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Fin: Plataforma y DX12 core
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Inicio: Pipeline y recursos de escena
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// RootSig + PSO
//--------------------------------------------------------------------------------------
void CreateRootSigAndPSO()
{
    // Define qué recursos ve el shader y crea el pipeline gráfico completo.

    // Root parameter: 1 CBV (b0)
    // Root signature con un solo parámetro: un CBV en b0 (CBData)
    // Lo ven todos los shaders (VS y PS)
    D3D12_ROOT_PARAMETER rootParams[1] = {};
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0;
    rootParams[0].Descriptor.RegisterSpace = 0;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Root signature flags (?)
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

    // Crear Root signature
    ComPtr<ID3DBlob> sigBlob, errBlob;
    ThrowIfFailed(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob));
    ThrowIfFailed(g_device->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&g_rootSig)));

    // Compilar shaders desde archivo PBR.hlsl
    UINT compileFlags =
#if _DEBUG
        D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
        0;
#endif
    ComPtr<ID3DBlob> vs, ps;

    ThrowIfFailed(D3DCompileFromFile(
        L"PBR.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "VSMain", "vs_5_0", compileFlags, 0, &vs, &errBlob));

    ThrowIfFailed(D3DCompileFromFile(
        L"PBR.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
        "PSMain", "ps_5_0", compileFlags, 0, &ps, &errBlob));

    // Input layout (Describe cómo está armado el Vertex en memoria)
    D3D12_INPUT_ELEMENT_DESC il[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,pos),    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,color),  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex,normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };


    // PIPELINE CONFIG:

    // Rasterizer (Relleno sólido, culling de back faces, clipping de depth.)
    D3D12_RASTERIZER_DESC rast = {};
    rast.FillMode = D3D12_FILL_MODE_SOLID;
    rast.CullMode = D3D12_CULL_MODE_BACK;
    rast.FrontCounterClockwise = FALSE;
    rast.DepthClipEnable = TRUE;

    // Depth test activado, escribe el Z, pasa si es menor
    D3D12_DEPTH_STENCIL_DESC dss = {};
    dss.DepthEnable = TRUE;
    dss.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    dss.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    dss.StencilEnable = FALSE;

    //Blend “simple”: escribe RGBA tal cual, sin blending especial
    D3D12_BLEND_DESC blend = {};
    blend.AlphaToCoverageEnable = FALSE;
    blend.IndependentBlendEnable = FALSE;
    auto& rt = blend.RenderTarget[0];
    rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // Creo Pipeline State Object (PSO)
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
    // Crea el vertex buffer, index buffer del cubo y el constant buffer mapeado.

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

    // VB (upload para simplificar lo hago aca)
    // Reserva un buffer en heap UPLOAD(memoria accesible por CPU).
    // Lo mapea, copia los vértices, lo desmapea.
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
        //Similar para el index buffer
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

    // Constant Buffer (mapeado persistente, g_cbMapped)
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
// Geometry (sphere)
//--------------------------------------------------------------------------------------

void CreateSphereGeometry(float radius = 0.5f, int stacks = 32, int slices = 32)
{
    // Opcional para visualizar una esfera en lugar de un cubo.

    std::vector<Vertex> verts;
    std::vector<uint16_t> inds;
    verts.reserve((stacks + 1) * (slices + 1));
    inds.reserve(stacks * slices * 6);

    for (int y = 0; y <= stacks; ++y)
    {
        float v = float(y) / stacks;           // [0,1]
        float phi = v * XM_PI;                 // [0,π]
        float sinPhi = sinf(phi), cosPhi = cosf(phi);

        for (int x = 0; x <= slices; ++x)
        {
            float u = float(x) / slices;       // [0,1]
            float theta = u * XM_2PI;          // [0,2π]
            float sinTheta = sinf(theta), cosTheta = cosf(theta);

            XMFLOAT3 n = XMFLOAT3(cosTheta * sinPhi, cosPhi, sinTheta * sinPhi);
            XMFLOAT3 p = XMFLOAT3(n.x * radius, n.y * radius, n.z * radius);

            // color simple por normal (debug)
            XMFLOAT3 c = XMFLOAT3(0.5f * (n.x + 1.0f), 0.5f * (n.y + 1.0f), 0.5f * (n.z + 1.0f));

            verts.push_back({ p, c, n });
        }
    }

    int stride = slices + 1;
    for (int y = 0; y < stacks; ++y)
    {
        for (int x = 0; x < slices; ++x)
        {
            uint32_t i0 = y * stride + x;
            uint32_t i1 = i0 + 1;
            uint32_t i2 = i0 + stride;
            uint32_t i3 = i2 + 1;

            // Dos triángulos por quad
            inds.push_back((uint16_t)i0);
            inds.push_back((uint16_t)i1);
            inds.push_back((uint16_t)i2);

            inds.push_back((uint16_t)i1);
            inds.push_back((uint16_t)i3);
            inds.push_back((uint16_t)i2);
        }
    }

    g_sphereIndexCount = (UINT)inds.size();

    // Subir a GPU (UPLOAD como el cubo)
    const UINT vbSize = (UINT)(verts.size() * sizeof(Vertex));
    const UINT ibSize = (UINT)(inds.size() * sizeof(uint16_t));

    // VB
    {
        D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = vbSize; rd.Height = 1; rd.DepthOrArraySize = 1;
        rd.MipLevels = 1; rd.SampleDesc = { 1,0 }; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(g_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_sphereVB)));

        void* data = nullptr; D3D12_RANGE rr = { 0,0 };
        ThrowIfFailed(g_sphereVB->Map(0, &rr, &data));
        memcpy(data, verts.data(), vbSize);
        g_sphereVB->Unmap(0, nullptr);

        g_sphereVBView.BufferLocation = g_sphereVB->GetGPUVirtualAddress();
        g_sphereVBView.StrideInBytes = sizeof(Vertex);
        g_sphereVBView.SizeInBytes = vbSize;
    }

    // IB
    {
        D3D12_HEAP_PROPERTIES hp = {}; hp.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = ibSize; rd.Height = 1; rd.DepthOrArraySize = 1;
        rd.MipLevels = 1; rd.SampleDesc = { 1,0 }; rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(g_device->CreateCommittedResource(&hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&g_sphereIB)));

        void* data = nullptr; D3D12_RANGE rr = { 0,0 };
        ThrowIfFailed(g_sphereIB->Map(0, &rr, &data));
        memcpy(data, inds.data(), ibSize);
        g_sphereIB->Unmap(0, nullptr);

        g_sphereIBView.BufferLocation = g_sphereIB->GetGPUVirtualAddress();
        g_sphereIBView.Format = DXGI_FORMAT_R16_UINT;
        g_sphereIBView.SizeInBytes = ibSize;
    }
}

void ExtractMeshCPU(
    const aiMesh* mesh,
    std::vector<Vertex>& outVerts,
    std::vector<uint32_t>& outIndices)
{
    outVerts.clear();
    outIndices.clear();

    outVerts.reserve(mesh->mNumVertices);
    outIndices.reserve(mesh->mNumFaces * 3);

    const bool hasNormals = mesh->HasNormals();

    for (unsigned int v = 0; v < mesh->mNumVertices; ++v)
    {
        const aiVector3D& p = mesh->mVertices[v];
        XMFLOAT3 pos = XMFLOAT3(p.x, p.y, p.z);

        XMFLOAT3 nrm = XMFLOAT3(0, 1, 0);
        if (hasNormals)
        {
            const aiVector3D& n = mesh->mNormals[v];
            nrm = XMFLOAT3(n.x, n.y, n.z);
        }

        // color debug por normal transformo [-1, 1]  →  [0, 1]
        XMFLOAT3 col = XMFLOAT3(
            0.5f * (nrm.x + 1.0f),
            0.5f * (nrm.y + 1.0f),
            0.5f * (nrm.z + 1.0f)
        );

        outVerts.push_back(Vertex{ pos, col, nrm });
    }

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f)
    {
        const aiFace& face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;

        outIndices.push_back((uint32_t)face.mIndices[0]);
        outIndices.push_back((uint32_t)face.mIndices[1]);
        outIndices.push_back((uint32_t)face.mIndices[2]);
    }
}

void CreateCustomModelGeometry(const std::string& fileName)
{
    Assimp::Importer importer;

    const unsigned int flags =
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices;

    const aiScene* scene = importer.ReadFile(fileName, flags);

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode || scene->mNumMeshes == 0)
    {
        OutputDebugStringA("Assimp load failed:\n");
        OutputDebugStringA(importer.GetErrorString());
        OutputDebugStringA("\n");
        return;
    }

    const aiMesh* mesh = scene->mMeshes[0];

    std::vector<Vertex> verts;
    std::vector<uint32_t> inds;

    ExtractMeshCPU(mesh, verts, inds);

    g_modelIndexCount = (UINT)inds.size();

    // --- VB (UPLOAD) ---
    {
        const UINT vbSize = (UINT)(verts.size() * sizeof(Vertex));

        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = vbSize;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc = { 1, 0 };
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(g_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&g_modelVB)));

        void* data = nullptr;
        D3D12_RANGE rr = { 0, 0 };
        ThrowIfFailed(g_modelVB->Map(0, &rr, &data));
        memcpy(data, verts.data(), vbSize);
        g_modelVB->Unmap(0, nullptr);

        g_modelVBView.BufferLocation = g_modelVB->GetGPUVirtualAddress();
        g_modelVBView.StrideInBytes = sizeof(Vertex);
        g_modelVBView.SizeInBytes = vbSize;
    }

    // --- IB (UPLOAD) 32-bit ---
    {
        const UINT ibSize = (UINT)(inds.size() * sizeof(uint32_t));

        D3D12_HEAP_PROPERTIES hp = {};
        hp.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC rd = {};
        rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        rd.Width = ibSize;
        rd.Height = 1;
        rd.DepthOrArraySize = 1;
        rd.MipLevels = 1;
        rd.SampleDesc = { 1, 0 };
        rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ThrowIfFailed(g_device->CreateCommittedResource(
            &hp, D3D12_HEAP_FLAG_NONE, &rd,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr, IID_PPV_ARGS(&g_modelIB)));

        void* data = nullptr;
        D3D12_RANGE rr = { 0, 0 };
        ThrowIfFailed(g_modelIB->Map(0, &rr, &data));
        memcpy(data, inds.data(), ibSize);
        g_modelIB->Unmap(0, nullptr);

        g_modelIBView.BufferLocation = g_modelIB->GetGPUVirtualAddress();
        g_modelIBView.Format = DXGI_FORMAT_R32_UINT;
        g_modelIBView.SizeInBytes = ibSize;
    }

    char buf[256];
    sprintf_s(buf, "Model GPU upload OK. Verts: %zu | Indices: %zu\n", verts.size(), inds.size());
    OutputDebugStringA(buf);
}

//--------------------------------------------------------------------------------------
// Init de matrices de cámara
//--------------------------------------------------------------------------------------
void InitCamera()
{
    // Configura view / projection y guarda la posición de la cámara.

    XMVECTOR eye = XMVectorSet(1.5f, 1.2f, -2.0f, 0.0f);
    XMVECTOR at = XMVectorSet(0, 0, 0, 0);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);

    g_view = XMMatrixLookAtLH(eye, at, up);
    g_proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.f), float(Width) / float(Height), 0.1f, 100.0f);

    XMStoreFloat3(&g_eyeWS, eye); // para PBR, vector V en shader.
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Fin: Pipeline y recursos de escena
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Inicio: Ciclo de render por frame
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Update + Record + Present
//--------------------------------------------------------------------------------------

void UpdateCB()
{
    //Actualizo todo lo que el shader necesita para este frame y lo escribo en el constant buffer

    // Tiempo
    auto t1 = std::chrono::high_resolution_clock::now();
    float seconds = std::chrono::duration<float>(t1 - g_t0).count();

    // delta t para rotación del cubo (acumulado solo si no está en pausa)
    float dt = std::chrono::duration<float>(t1 - g_prevTick).count();
    g_prevTick = t1;
    if (!g_pauseRotation) {
        g_rotTime += dt;
    }

    // Matriz de mundo: usar g_rotTime (no seconds) para la rotación del cubo
    XMMATRIX mWorld =
        XMMatrixRotationX(g_rotTime * 0.7f) *
        XMMatrixRotationY(g_rotTime * 1.1f);

    if (g_geomMode == 2) // modelo
    {
        mWorld = XMMatrixScaling(0.25f, 0.25f, 0.25f) * mWorld;
    }

    XMMATRIX mvp = mWorld * g_view * g_proj;

    // luz direccional fija en mundo (arriba-derecha-atrás)
    XMVECTOR L = XMVector3Normalize(XMVectorSet(0.0f, -1.0f, 0.0f, 0.0f));

    // Poblar CBData
    CBData cb;
    cb.mvp = XMMatrixTranspose(mvp);
    cb.world = mWorld;
    XMStoreFloat3(&cb.lightDir, L);
    cb.ambient = 0.15f;      // ambiente base
    cb.mode = g_mode;     // desde el toggle

    cb.viewPos = g_eyeWS;     
    cb.shininess = 64.0f;     // pruebitas: 32-128
    cb.specIntensity = 0.6f;  // k_s

    // Material desde presets / teclas
    cb.baseColor = g_baseColor;
    cb.metallic = g_metallic;   // 0..1, tecla M
    cb.roughness = g_roughness;  // 0..1, tecla R
    cb.ao = g_ao;         // 0..1, tecla A

    XMFLOAT3 lightPosWS;
    if (g_lightPinnedFront) {
        // vector forward de la cámara hacia el origen (0,0,0)
        XMVECTOR eyeV = XMLoadFloat3(&g_eyeWS);
        XMVECTOR toOrigin = XMVector3Normalize(XMVectorSubtract(XMVectorZero(), eyeV));
        XMVECTOR posV = XMVectorAdd(eyeV, XMVectorScale(toOrigin, g_lightFrontDist));
        XMStoreFloat3(&lightPosWS, posV);
    }
    else {
        // órbita simple en XZ
        float radius = 1.2f;                // 1.2 para que pase bien por delante
        float angle = seconds;             // velocidad 1 rad/s
        lightPosWS = XMFLOAT3(cosf(angle) * radius, 1.0f, sinf(angle) * radius);
    }

    // Colocar en el CB
    cb.lightPos = lightPosWS;
    cb.lightIntensity = g_lightIntensity;
    cb.lightColor = g_lightColor;

    *g_cbMapped = cb;

}

void RecordRender()
{
    // Grabo la lista de comandos que el GPU va a ejecutar para este frame

    // Limpio el allocator del frame, reseteo la command list y le asocio el PSO (g_pso).

    ThrowIfFailed(g_cmdAlloc[g_frameIndex]->Reset());
    ThrowIfFailed(g_cmdList->Reset(g_cmdAlloc[g_frameIndex].Get(), g_pso.Get()));

    // Seteo de estado de pipeline base
    g_cmdList->SetGraphicsRootSignature(g_rootSig.Get());
    g_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); //Topología triángulo
    //g_cmdList->IASetVertexBuffers(0, 1, &g_vbView);
    //g_cmdList->IASetIndexBuffer(&g_ibView);
    g_cmdList->SetGraphicsRootConstantBufferView(0, g_cb->GetGPUVirtualAddress()); //Bind del constant buffer (root param 0 → CBV en b0).
    g_cmdList->RSSetViewports(1, &g_viewport);
    g_cmdList->RSSetScissorRects(1, &g_scissorRect);

    // Elegir el backbuffer actual
    auto bb = g_renderTargets[g_frameIndex].Get();

    // Transition BB a Render Target
    Transition(g_cmdList.Get(), bb, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // RTV/DSV handles
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = g_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtv.ptr += g_frameIndex * g_rtvDescriptorSize; //Calculo el handle del RTV del frame actual.
    D3D12_CPU_DESCRIPTOR_HANDLE dsv = g_dsvHeap->GetCPUDescriptorHandleForHeapStart();

    // Limpio color y depth
    const float clearColor[4] = { 0.07f, 0.1f, 0.16f, 1.0f };
    g_cmdList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
    g_cmdList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // Bind del render target + depth al pipeline (OM = Output Merger).
    g_cmdList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    
    // Draw según geometría
    if (g_geomMode == 0) // Cubo
    {
        g_cmdList->IASetVertexBuffers(0, 1, &g_vbView); //Set vertex buffer
        g_cmdList->IASetIndexBuffer(&g_ibView); //Set index buffer
        g_cmdList->DrawIndexedInstanced(36, 1, 0, 0, 0); //36 índices para el cubo
    }
    else if (g_geomMode == 1) // Esfera
    {
        g_cmdList->IASetVertexBuffers(0, 1, &g_sphereVBView);
        g_cmdList->IASetIndexBuffer(&g_sphereIBView);
        g_cmdList->DrawIndexedInstanced(g_sphereIndexCount, 1, 0, 0, 0);
    }
    else // 2: Modelo 
    {
        g_cmdList->IASetVertexBuffers(0, 1, &g_modelVBView);
        g_cmdList->IASetIndexBuffer(&g_modelIBView);
        g_cmdList->DrawIndexedInstanced(g_modelIndexCount, 1, 0, 0, 0);
}

    // Transition a Present listo para que el swap chain lo muestre
    Transition(g_cmdList.Get(), bb, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    ThrowIfFailed(g_cmdList->Close()); //Cerrar la command list
}

void Present()
{
    //Le digo al swap chain que muestre el frame, y sincronizo CPU ↔ GPU

    ThrowIfFailed(g_swapChain->Present(1, 0)); // vsync configurado

    // Avanzar frame + Sincronización de frames con fences
    const UINT64 fenceToSignal = ++g_fenceValue;
    ThrowIfFailed(g_cmdQueue->Signal(g_fence.Get(), fenceToSignal));

    g_frameIndex = g_swapChain->GetCurrentBackBufferIndex();

    //Si el GPU todavía no llegó al valor de fence indicado, espero.
    //Esto evita grabar comandos sobre recursos que el GPU todavía está usando.

    if (g_fence->GetCompletedValue() < fenceToSignal) {
        ThrowIfFailed(g_fence->SetEventOnCompletion(fenceToSignal, g_fenceEvent));
        WaitForSingleObject(g_fenceEvent, INFINITE);
    }
}

//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
// Fin: Ciclo de render por frame
//--------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
// Main
//--------------------------------------------------------------------------------------
int APIENTRY wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int) //Aplicación
{
    CreateAppWindow(hInst);
    UpdateWindowTitle(); //Solo para ver parámetros

    CreateFactoryAndDevice();
    CreateSwapchainAndRTVs();
    CreateDepthBuffer();
    CreateCmdListAndFence();
    CreateRootSigAndPSO();

    CreateCubeGeometryAndCB();
    CreateSphereGeometry(0.5f, 32, 32); // radio y teselación
    CreateCustomModelGeometry("Models/Intergalactic_Spaceship-(Wavefront).obj");
   
    InitCamera();

    g_prevTick = std::chrono::high_resolution_clock::now();

    // Loop
    MSG msg = {};
    while (msg.message != WM_QUIT)  //Mientras la ventana siga viva, proceso mensajes; cuando estoy libre, renderizo.
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
