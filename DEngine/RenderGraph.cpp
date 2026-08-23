#include "pch.h"
#include "RenderGraph.h"
#include "Texture.h"
#include "RenderGraphResource.h"

#include <queue>

RGTextureHandle RenderGraph::ImportTexture(const shared_ptr<Texture>& texture)
{
    assert(texture != nullptr);

    RGTextureHandle handle;
    handle.id = static_cast<uint32_t>(_textures.size());

    RGTextureResource resource;
    resource.texture = texture;
    resource.currentState = texture->GetResourceState();
    resource.external = true;

    _textures.push_back(resource);

    return handle;
}

// ============================================================
// Resource Usage -> DirectX12 Resource State
// ============================================================

D3D12_RESOURCE_STATES RenderGraph::GetRequiredState(RGResourceUsage usage) const
{
    switch (usage)
    {
    case RGResourceUsage::PixelSRV:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

    case RGResourceUsage::NonPixelSRV:
        return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    case RGResourceUsage::RenderTarget:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;

    case RGResourceUsage::DepthWrite:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;

    case RGResourceUsage::DepthRead:
        return D3D12_RESOURCE_STATE_DEPTH_READ;

    case RGResourceUsage::UAV:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    case RGResourceUsage::CopySource:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;

    case RGResourceUsage::CopyDest:
        return D3D12_RESOURCE_STATE_COPY_DEST;

    case RGResourceUsage::Present:
        return D3D12_RESOURCE_STATE_PRESENT;
    }

    assert(false);

    return D3D12_RESOURCE_STATE_COMMON;
}

// ============================================================
// Pass 간 Dependency 확인
// ============================================================

bool RenderGraph::HasDependency(const RenderGraphPass& first, const RenderGraphPass& second) const
{
    for (const RGPassResource& firstResource : first._resources)
    {
        for (const RGPassResource& secondResource : second._resources)
        {
            // 서로 다른 Resource
            if (firstResource.handle.id != secondResource.handle.id)
                continue;

            const bool firstWrite = firstResource.access != RGResourceAccess::READ;
            const bool secondWrite = secondResource.access != RGResourceAccess::READ;

            // Read -> Read는 Dependency 없음
            if (!firstWrite && !secondWrite)
                continue;

            // 같은 Resource이고 둘 중 하나라도 Write
            return true;
        }
    }

    return false;
}

// ============================================================
// Dependency Graph 생성
// ============================================================

void RenderGraph::BuildDependencyGraph()
{
    const uint32_t passCount = static_cast<uint32_t>(_passes.size());

    _adjacency.clear();
    _adjacency.resize(passCount);

    for (uint32_t i = 0; i < passCount; ++i)
    {
        for (uint32_t j = i + 1; j < passCount; ++j)
        {
            if (HasDependency(*_passes[i], *_passes[j]))
            {
                _adjacency[i].push_back(j);
            }
        }
    }
}

// ============================================================
// Topological Sort
// ============================================================

void RenderGraph::SortPasses()
{
    const uint32_t passCount = static_cast<uint32_t>(_passes.size());

    vector<uint32_t> inDegree(passCount, 0);

    for (uint32_t i = 0; i < passCount; ++i)
    {
        for (uint32_t next : _adjacency[i])
        {
            ++inDegree[next];
        }
    }

    queue<uint32_t> q;

    for (uint32_t i = 0; i < passCount; ++i)
    {
        if (inDegree[i] == 0)
        {
            q.push(i);
        }
    }

    _executionOrder.clear();
    _executionOrder.reserve(passCount);

    while (!q.empty())
    {
        const uint32_t current = q.front();
        q.pop();

        _executionOrder.push_back(current);

        for (uint32_t next : _adjacency[current])
        {
            --inDegree[next];

            if (inDegree[next] == 0)
            {
                q.push(next);
            }
        }
    }

    // 모든 Pass가 정렬되지 않았다면 Cycle
    assert(_executionOrder.size() == passCount && "RenderGraph dependency cycle detected");
}

// ============================================================
// Compile
// ============================================================

void RenderGraph::Compile()
{
    BuildDependencyGraph();
    SortPasses();
}

// ============================================================
// Resource Barrier
// ============================================================

void RenderGraph::TransitionResource(RGTextureResource& resource, D3D12_RESOURCE_STATES requiredState)
{
    assert(resource.texture != nullptr);
    assert(_cmdList != nullptr);

    // 이미 필요한 State
    if (resource.currentState == requiredState)
    {
        // UAV -> UAV의 경우 State 변화는 없지만
        // 이전 UAV 작업 완료 보장이 필요할 수 있음
        if (requiredState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        {
            D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::UAV(resource.texture->GetTex2D().Get());

            _cmdList->ResourceBarrier(1, &barrier);
        }

        return;
    }

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            resource.texture->GetTex2D().Get(),
            resource.currentState,
            requiredState);

    _cmdList->ResourceBarrier(1, &barrier);

    // RenderGraph 내부 State 갱신
    resource.currentState = requiredState;
    resource.texture->SetResourceState(requiredState);
}

// ============================================================
// Execute
// ============================================================

void RenderGraph::Execute()
{
    Compile();

    for (uint32_t passIndex : _executionOrder)
    {
        RenderGraphPass& pass =
            *_passes[passIndex];

        // ----------------------------------------
        // Pass 실행 전에 필요한 State로 전환
        // ----------------------------------------

        for (const RGPassResource& passResource : pass._resources)
        {
            assert(passResource.handle.IsValid());
            assert(passResource.handle.id < _textures.size());

            RGTextureResource& resource = _textures[passResource.handle.id];

            const D3D12_RESOURCE_STATES requiredState = GetRequiredState(passResource.usage);

            TransitionResource(resource, requiredState);
        }

        // ----------------------------------------
        // 실제 Shadow / GBuffer / Lighting 실행
        // ----------------------------------------

        pass.Execute(_cmdList);
    }
}