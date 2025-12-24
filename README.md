# DirectX 12 PBR – Rotating Cube / Sphere / Model

This is a small **learning project** written in C++ and DirectX 12.  
It renders a rotating cube, sphere or pre loaded model using a basic **Physically-Based Rendering (PBR)** shading model (Cook–Torrance / GGX).

> **Note:** This project is intentionally simple and self-contained.  
> It is not meant to be a reusable engine or production-quality code.  
> The goal is to learn DX12 fundamentals by doing everything manually, without helper libraries like `d3dx12.h`.

> **Note:** Make sure you run DX12_PBR project inside the solution. All the other projects are basically a pathway to reach this one which is the last version.
---

<img width="1282" height="752" alt="image" src="https://github.com/user-attachments/assets/c5ae955f-6e1f-4899-bc09-a08d4342d550" />

<img width="1282" height="752" alt="image" src="https://github.com/user-attachments/assets/99628ac3-871c-48e5-87b6-3cbfbb979adb" />

<img width="1282" height="752" alt="image" src="https://github.com/user-attachments/assets/93cecf7c-698f-4264-9d44-f0a4aadf2ddf" />


## Purpose of the Project

- Build a minimal but complete **DX12 rendering pipeline** from scratch.
- Understand:
  - Device creation, swap chain, descriptor heaps, command queues.
  - Constant buffers, resource transitions, PSO, root signatures.
  - Vertex/index buffers and rendering flow.
- Implement a working **PBR shader** with adjustable material parameters.
- Provide a clear, heavily commented example suitable for study and portfolio review.

The project prioritizes **clarity over architecture** and **explicitness over abstraction**.

---

## Technical Topics Covered

### **DirectX 12 Core**
- Win32 window creation (`wWinMain`, `WndProc`).
- DXGI factory, hardware adapter selection, WARP fallback.
- `ID3D12Device`, command queue, command allocators, command list.
- Swap chain: back buffers, presentation, frame index tracking.
- Resource barriers and synchronization using `ID3D12Fence`.

### **GPU Resources & Memory**
- `ID3D12Resource` for:
  - Vertex buffers
  - Index buffers
  - Constant buffers
  - Render targets (swap chain back buffers)
  - Depth buffer (`D32_FLOAT`)
- DX12 heap types:
  - `UPLOAD` – for CPU-updated buffers (constant buffers)
  - `DEFAULT` – for GPU-optimal resources
- Constant buffer alignment rules (256-byte alignment).

### **Descriptors & Pipeline**
- RTV (Render Target View) descriptor heap.
- DSV (Depth-Stencil View) descriptor heap.
- Root Signature with one `CBV` (buffer `b0`).
- Graphics Pipeline State Object (PSO):
  - Input layout
  - Rasterizer state
  - Depth/stencil state
  - Blend state
  - Shader bytecode
  - Render target and depth formats

### **Shaders (HLSL)**
- Vertex shader transforms:
  - World → View → Projection
  - Outputs world-space normal and position
- Pixel shader implements:
  - Cook–Torrance BRDF
  - GGX normal distribution
  - Smith geometry term
  - Schlick Fresnel approximation
  - Point light with physical 1/r² attenuation
- Modes for debugging:
  - Unlit
  - Ambient only
  - Lambert diffuse
  - PBR specular only
  - Direct PBR (diffuse + specular)
  - Full PBR (ambient + direct)

### **Geometry & Camera**
- Hardcoded cube (24 vertices, per-face normals).
- Procedurally generated sphere (stacks × slices).
- Pre loaded model using assimp library.
- Camera setup:
  - `XMMatrixLookAtLH`
  - `XMMatrixPerspectiveFovLH`
- Runtime animation using chrono timers.
- Light can orbit or be pinned in front of the camera.

---

## Controls

| Key | Action |
|-----|--------|
| **T** | Cycle lighting/debug mode (0–5) |
| **M** | Cycle metallic presets |
| **R** | Cycle roughness presets |
| **A** | Cycle ambient occlusion presets |
| **P** | Pause/resume rotation |
| **G** | Toggle geometry (cube ↔ sphere ↔ model) |
| **F** | Pin/unpin light to the camera |

---

## Requirements

- Windows 10 or later
- Windows 10/11 SDK with **DirectX 12** headers and libs
- Visual Studio (recommended)
- A GPU supporting Direct3D 12 (or WARP will be used)

---

## Build & Run Instructions

1. Open the project in **Visual Studio**.
2. Add:
   - `main.cpp`
   - `PBR.hlsl`
3. Link the following libraries:
   - `d3d12.lib`
   - `dxgi.lib`
   - `d3dcompiler.lib`
4. Ensure `PBR.hlsl` is located where the executable can load it  
   (or adjust the path in `D3DCompileFromFile`).
5. **Build and run in `Release x64`** — this is the default expected configuration.
6. Run the executable.  
   You should see a rotating cube or sphere with real-time lighting.

---

## Notes & Limitations

- No engine architecture; everything lives in a single translation unit for clarity.
- Uses `UPLOAD` heaps for simplicity — not optimal for real engines.
- The PBR implementation is simplified:
  - No Image-Based Lighting (IBL)
  - Ambient term is a placeholder
  - No texture support yet
- Code structure is intentionally straightforward for educational purposes.

---
