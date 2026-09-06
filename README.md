# DirectX 12 Renderer / Engine

DirectX 12 기반의 Renderer / Engine 프로젝트입니다.  
렌더링 기능과 **RenderGraph, Multi-threaded Command Recording, FrameResource, UploadAllocator, Descriptor / GPU Resource Lifetime 관리**등 DX12의 명시적 리소스 및 Command 관리 구조를 구현했습니다.

---

## Highlights

### Rendering
- Forward / Deferred Rendering
- MRT / GBuffer
- PBR + IBL
- Normal Mapping / TBN
- Shadow Mapping
- Terrain / Distance-based Tessellation
- FBX Loading / Skeletal Animation
- Compute Shader Skinning

### Engine / Optimization
- Scene / GameObject / Component
- RenderGraph (Topological Sort, Resource State / Berrier Management)
- Multi-threaded Command Recording
- FrameResource / UploadAllocator
- DescriptorAllocator / Deferred Release
- Frustum Culling / Instancing
- GPU Timestamp Profiler / ImGui Diagnostics

---

# Rendering Features

## Deferred Rendering / GBuffer

Geometry Pass에서 GBuffer를 생성하고 Lighting Pass에서 조명을 계산합니다.

- Position
- Normal
- Albedo
- MaterialInfo

![Deferred Rendering / GBuffer](assets/readme/forward_deferred.png)

---

## PBR + IBL

Metallic / Roughness 변화와 Environment Map 기반 반사를 확인하기 위한 테스트 Scene입니다.

<table>
<tr>
<td width="50%">

**Metallic / Roughness**

<img src="assets/readme/pbr_metallic_roughness.png">

</td>
<td width="50%">

**IBL Reflection**

<img src="assets/readme/pbr_ibl.png">

</td>
</tr>
</table>

---

## Normal Mapping / TBN

Tangent-space Normal Map을 TBN 행렬로 변환해 Geometry를 추가하지 않고 표면 조명 디테일을 표현합니다.

![Normal Mapping / TBN](assets/readme/normalmap_tbn.png)

---

## Terrain Tessellation

카메라와 Terrain Patch의 거리에 따라 Tessellation Level을 조절합니다.

<table>
<tr>
<td width="50%">

**Near**

<img src="assets/readme/tessellation_near.png">

</td>
<td width="50%">

**Far**

<img src="assets/readme/tessellation_far.png">

</td>
</tr>
</table>

---

# Performance Benchmark

## Instancing

동일 Mesh / Material 객체를 Instance 단위로 묶어 Draw Call과 CPU Recording 비용을 줄였습니다.

| Metric | OFF | ON |
|---|---:|---:|
| FPS | 3 | 90 |
| CPU Frame | 300.371 ms | 12.054 ms |
| GPU Frame | 3.344 ms | 0.328 ms |
| Command Record | 332.080 ms | 10.102 ms |
| Draw Calls | 410 | 11 |
| Triangles | 309,230 | 309,230 |

동일 Triangle 수를 유지하면서 Draw Calls 410 -> 11로 감소했습니다.

<table>
<tr>
<td width="50%"><img src="assets/readme/instancing_off.png"></td>
<td width="50%"><img src="assets/readme/instancing_on.png"></td>
</tr>
</table>

---

## Frustum Culling

View Frustum 외부 객체를 제외하여 처리 Object / Triangle 수를 감소시켰습니다.

| Metric | OFF | ON |
|---|---:|---:|
| Visible Objects | 401 | 269 |
| Culled Objects | 0 | 132 |
| Triangles | 309,230 | 207,194 |
| Draw Calls | 11 | 11 |

132개 Object를 제외하고 Triangle 수를 309,230 -> 207,194로 감소시켰습니다.

<table>
<tr>
<td width="50%"><img src="assets/readme/frustum_off.png"></td>
<td width="50%"><img src="assets/readme/frustum_on.png"></td>
</tr>
</table>

---

## Multi-threaded Command Recording

RenderGraph Pass를 Worker Thread에 분배하고,
각 Pass를 독립적인 CommandContext에서 병렬 Recording합니다.

| Worker Count | Command Record |
|---:|---:|
| 1 | 52.19 ms |
| 2 | 42.91 ms |
| 4 | **36.88 ms** |
| 8 | 37.72 ms |

Worker 수를 1 → 4로 증가시키면서 Command Recording Time이 감소했습니다.

반면 4 → 8 구간에서는 추가적인 성능 향상이 거의 나타나지 않았습니다.

현재 RenderGraph는 5개의 Pass를 Pass 단위 Job으로 처리하기 때문에,
Worker 수가 Pass 수를 초과하면 추가적인 병렬성을 확보하기 어렵습니다.

따라서 현재 구조에서는 **4 Worker 이후 Command Recording 병렬화 효과가 포화되는 경향**을 확인했습니다.

<details>
<summary>Worker 1 / 2 / 4 / 8 screenshots</summary>
<table>
<tr>
<td width="50%">Worker 1<img src="assets/readme/worker1.png"></td>
<td width="50%">Worker 2<img src="assets/readme/worker2.png"></td>
</tr>
<tr>
<td width="50%">Worker 4<img src="assets/readme/worker4.png"></td>
<td width="50%">Worker 8<img src="assets/readme/worker8.png"></td>
</tr>
</table>
</details>

---

# Engine Architecture

## RenderGraph

Pass가 Resource의 Read / Write / Usage를 선언하면 Dependency와 실행 순서를 계산하고 필요한 D3D12 Resource Barrier를 자동 생성합니다.

```mermaid
flowchart LR
    S[Shadow]
    G[GBuffer]
    L[Lighting]
    F[Final]
    FW[Forward]

    S --> L
    G --> L
    L --> F
    F --> FW
```

```text
Read / Write 선언
→ Dependency 분석
→ Topological Sort
→ Resource State 계산
→ Barrier 생성
→ Execute
```

## Resource / Frame Management

- **FrameResource**  
  CommandAllocator, UploadAllocator, CommandContext Pool, Fence Value를 프레임 단위로 관리합니다.

- **UploadAllocator**  
  프레임별 큰 Upload Buffer를 Persistent Mapping하고 Linear Allocation 방식으로 동적 데이터를 할당합니다.

- **DescriptorAllocator**  
  Descriptor Slot Allocate / Free와 Free Range Merge를 관리합니다.

- **Deferred Release**  
  GPU가 Resource를 더 이상 참조하지 않는 Fence 완료 시점 이후 Resource / Descriptor를 반환합니다.

---

# Diagnostics

ImGui 기반 Diagnostics UI에서 다음 항목을 런타임에 확인할 수 있습니다.

- FPS / CPU Frame / GPU Frame
- Render Prepare
- RenderGraph Compile
- Command Record
- Draw Calls / Triangle Count
- Visible / Culled Objects
- RenderGraph Pass / Barrier Count
- Worker Count
- Frustum Culling / Instancing Toggle
- RenderTarget Debug View

---

# Final Showcase

최종 Scene에서는 다음 요소를 함께 확인할 수 있습니다.

- Terrain
- Animated FBX Character
- Compute Skinning
- Directional Light
- Dynamic Shadow
- Skybox

**Showcase 핵심:**  
Compute Skinning 결과를 Shadow Pass에도 동일하게 사용하여 애니메이션되는 Character의 Dynamic Shadow가 실시간으로 갱신됩니다.

---

# Tech Stack

- C++ / DirectX 12
- HLSL
- FBX SDK
- ImGui
- Visual Studio

---
