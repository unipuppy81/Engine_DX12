#pragma once

#include "RenderGraphResource.h"
#include "RenderGraphPass.h"

class RenderGraph
{
public:

    explicit RenderGraph(ID3D12GraphicsCommandList* cmdList) : _cmdList(cmdList) { }

    // --------------------------------------------------------
    // 기존 Texture를 RenderGraph에 등록
    // --------------------------------------------------------

    RGTextureHandle ImportTexture(const shared_ptr<Texture>& texture, D3D12_RESOURCE_STATES currentState);

    // --------------------------------------------------------
    // Pass 등록
    // --------------------------------------------------------

    template<typename SetupLambda, typename ExecuteLambda>
    void AddPass(const string& name, SetupLambda&& setup, ExecuteLambda&& execute)
    {
        using ExecuteType = decay_t<ExecuteLambda>;

        auto pass = make_unique<LambdaRenderGraphPass<ExecuteType>>( name, forward<ExecuteLambda>(execute));

        // Read / Write 정보 등록
        RenderGraphBuilder builder(*pass);
        setup(builder);
        _passes.push_back(move(pass));
    }

    void Compile();
    void Execute();

private:

    void BuildDependencyGraph();
    void SortPasses();
    void TransitionResource(RGTextureResource& resource, D3D12_RESOURCE_STATES requiredState);

    D3D12_RESOURCE_STATES GetRequiredState(RGResourceUsage usage) const;

    bool HasDependency(const RenderGraphPass& first, const RenderGraphPass& second) const;

private:

    ID3D12GraphicsCommandList* _cmdList = nullptr;

    // 등록된 Pass
    vector<unique_ptr<RenderGraphPass>> _passes;

    // 등록된 Texture
    vector<RGTextureResource> _textures;

    // Dependency
    vector<vector<uint32_t>> _adjacency;

    // 최종 실행 순서
    vector<uint32_t> _executionOrder;
};

