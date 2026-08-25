#include "pch.h"
#include "RenderGraph.h"
#include "Texture.h"
#include "RenderGraphResource.h"

#include <queue>

RGTextureHandle RenderGraph::ImportTexture(const shared_ptr<Texture>& texture)
{
    assert(texture != nullptr);

    // 이미 등록된 Texture면 기존 Handle 반환
    auto it = _importedTextures.find(texture.get());
    if (it != _importedTextures.end())
        return it->second;

    RGTextureHandle handle;
    handle.id = static_cast<uint32_t>(_textures.size());

    RGTextureResource resource;
    resource.texture = texture;
    resource.currentState = texture->GetResourceState();

    _textures.push_back(resource);

    _importedTextures.emplace(texture.get(), handle);

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
// Dependency Graph 생성
// ============================================================

void RenderGraph::BuildDependencyGraph()
{
    const uint32_t passCount = static_cast<uint32_t>(_passes.size());
    const uint32_t resourceCount = static_cast<uint32_t>(_textures.size());

    _adjacency.clear();
    _adjacency.resize(passCount);

    vector<int32> lastWriter(resourceCount, -1);
    vector<vector<uint32_t>> readers(resourceCount);

    auto AddEdge = [&](uint32_t from, uint32_t to)
        {
            if (from == to)
                return;

            auto& edges = _adjacency[from];

            if (find(edges.begin(), edges.end(), to) == edges.end())
                edges.push_back(to);
        };

    for (uint32_t passIndex = 0; passIndex < passCount; ++passIndex)
    {
        RenderGraphPass& pass = *_passes[passIndex];

        for (const RGPassResource& resource : pass._resources)
        {
            const uint32_t id = resource.handle.id;

            assert(id < resourceCount);

            switch (resource.access)
            {
            case RGResourceAccess::READ:
            {
                // RAW : 이전 Write → 현재 Read
                if (lastWriter[id] >= 0)
                {
                    AddEdge(static_cast<uint32_t>(lastWriter[id]), passIndex);
                }
                readers[id].push_back(passIndex);
                break;
            }

            case RGResourceAccess::WRITE:
            case RGResourceAccess::READWRITE:
            {
                // WAW : 이전 Write → 현재 Write
                if (lastWriter[id] >= 0)
                {
                    AddEdge(
                        static_cast<uint32_t>(lastWriter[id]),
                        passIndex);
                }

                // WAR : 이전 Read → 현재 Write
                for (uint32_t reader : readers[id])
                {
                    AddEdge(reader, passIndex);
                }

                readers[id].clear();
                lastWriter[id] = static_cast<int32>(passIndex);

                break;
            }
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
    ValidatePassResources();

    BuildDependencyGraph();
    SortPasses();

    BuildBarriers();
}

// ============================================================
// Execute
// ============================================================

void RenderGraph::Execute()
{
    Compile();

    for (uint32_t passIndex : _executionOrder)
    {
        RenderGraphPass& pass = *_passes[passIndex];
        const auto& barriers = _passBarriers[passIndex];

        if (!barriers.empty())
        {
            _cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }

        pass.Execute(_cmdList);
    }

    // 모든 Barrier/Pass 기록이 끝난 뒤 최종 상태 반영
    for (RGTextureResource& resource : _textures)
    {
        resource.texture->SetResourceState(resource.currentState);
    }
}

/*
void RenderGraph::Execute()
{
    Compile();

    for (uint32_t passIndex : _executionOrder)
    {
        RenderGraphPass& pass = *_passes[passIndex];

        vector<D3D12_RESOURCE_BARRIER> barriers;

        for (const RGPassResource& passResource : pass._resources)
        {
            assert(passResource.handle.IsValid());
            assert(passResource.handle.id < _textures.size());

            RGTextureResource& resource = _textures[passResource.handle.id];

            D3D12_RESOURCE_STATES requiredState = GetRequiredState(passResource.usage);

            // 같은 State면 Transition 필요 없음
            if (resource.currentState == requiredState)
            {
                // UAV는 같은 State여도 실행 순서 보장용 Barrier 필요
                if (requiredState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                {
                    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(resource.texture->GetTex2D().Get()));
                }

                continue;
            }

            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource.texture->GetTex2D().Get(), resource.currentState, requiredState));

            resource.currentState = requiredState;
            resource.texture->SetResourceState(requiredState);
        }

        // Pass 실행 직전에 Barrier 한 번에 처리
        if (!barriers.empty())
        {
            _cmdList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
        }

        pass.Execute(_cmdList);
    }
}
*/

void RenderGraph::ValidatePassResources() const
{
    vector<bool> used(_textures.size(), false);

    for (const auto& pass : _passes)
    {
        const auto& resources = pass->_resources;

        for (uint32_t i = 0; i < resources.size(); ++i)
        {
            assert(resources[i].handle.IsValid());
            assert(resources[i].handle.id < _textures.size());

            used[resources[i].handle.id] = true;

            // 같은 Pass에서 같은 Resource 중복 선언 방지
            for (uint32_t j = i + 1; j < resources.size(); ++j)
            {
                assert(resources[i].handle.id != resources[j].handle.id && "Same resource declared multiple times in one RenderGraph pass");
            }
        }
    }

    // Import했는데 어떤 Pass에서도 안 쓰는 리소스 검출
    for (uint32_t i = 0; i < used.size(); ++i)
    {
        assert(used[i] && "Imported RenderGraph resource is never used");
    }
}

void RenderGraph::BuildBarriers()
{
    _passBarriers.clear();
    _passBarriers.resize(_passes.size());

    for (uint32_t passIndex : _executionOrder)
    {
        RenderGraphPass& pass = *_passes[passIndex];
        auto& barriers = _passBarriers[passIndex];

        for (const RGPassResource& passResource : pass._resources)
        {
            RGTextureResource& resource = _textures[passResource.handle.id];

            D3D12_RESOURCE_STATES requiredState = GetRequiredState(passResource.usage);

            if (resource.currentState == requiredState)
            {
                if (requiredState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
                {
                    barriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(resource.texture->GetTex2D().Get()));
                }

                continue;
            }

            barriers.push_back(CD3DX12_RESOURCE_BARRIER::Transition(resource.texture->GetTex2D().Get(), resource.currentState, requiredState));
            resource.currentState = requiredState;

            //resource.texture->SetResourceState(requiredState);
        }
    }
}