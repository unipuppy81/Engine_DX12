#pragma once

#include "RenderGraphResource.h"
#include "RenderGraphPass.h"

class RenderGraph
{
public:
    RenderGraph() = default;

    //    explicit RenderGraph(ID3D12GraphicsCommandList* cmdList) : _cmdList(cmdList) { }

    // --------------------------------------------------------
    // 기존 Texture를 RenderGraph에 등록
    // --------------------------------------------------------

    RGTextureHandle ImportTexture(const shared_ptr<Texture>& texture);

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
    void CommitResourceStates();

    // ============================
    // Executor용
    // ============================

    const vector<uint32_t>& GetExecutionOrder() const
    {
        return _executionOrder;
    }

    RenderGraphPass* GetPass(uint32_t passIndex)
    {
        assert(passIndex < _passes.size());

        return _passes[passIndex].get();
    }

    const vector<D3D12_RESOURCE_BARRIER>& GetPassBarriers(uint32_t passIndex) const
    {
        assert(passIndex < _passBarriers.size());
        return _passBarriers[passIndex];
    }

private:

    void BuildDependencyGraph();
    void SortPasses();

    D3D12_RESOURCE_STATES GetRequiredState(RGResourceUsage usage) const;

    void ValidatePassResources() const;

    void BuildBarriers();


private:

    // ID3D12GraphicsCommandList* _cmdList = nullptr;

    // 등록된 Pass
    vector<unique_ptr<RenderGraphPass>> _passes;

    // 등록된 Texture
    vector<RGTextureResource> _textures;

    // Dependency
    vector<vector<uint32_t>> _adjacency;

    // 최종 실행 순서
    vector<uint32_t> _executionOrder;

    unordered_map<Texture*, RGTextureHandle> _importedTextures;
    vector<vector<D3D12_RESOURCE_BARRIER>> _passBarriers;
};

