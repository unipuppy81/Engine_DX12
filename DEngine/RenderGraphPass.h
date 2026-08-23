#pragma once

#include "RenderGraphResource.h"

// ============================================================
// 하나의 Pass가 Resource 하나를 어떻게 사용하는지
// ============================================================
struct RGPassResource
{
    RGTextureHandle handle;

    RGResourceUsage usage;
    RGResourceAccess access;
};


class RenderGraphPass
{
    friend class RenderGraph;
    friend class RenderGraphBuilder;

public:
    explicit RenderGraphPass(const string& name) : _name(name) {}
    virtual ~RenderGraphPass() = default;

    virtual void Execute(ID3D12GraphicsCommandList* cmdList) = 0;

private:
    string _name;
    vector<RGPassResource> _resources;
};


// ============================================================
// 실제 Pass 작업을 Lambda로 저장
// ============================================================
template<typename Lambda>
class LambdaRenderGraphPass final : public RenderGraphPass
{
public:
    LambdaRenderGraphPass(const string& name, Lambda lambda) : RenderGraphPass(name), _lambda(move(lambda)) { }

    void Execute(ID3D12GraphicsCommandList* cmdList) override
    {
        _lambda(cmdList);
    }

private:
    Lambda _lambda;
};

// ============================================================
// Pass의 Read / Write 정보를 등록
// ============================================================
class RenderGraphBuilder 
{
public:
    explicit RenderGraphBuilder(RenderGraphPass& pass) : _pass(pass) {}

    void Read(RGTextureHandle handle, RGResourceUsage usage)
    {
        _pass._resources.push_back({ handle, usage, RGResourceAccess::READ });
    }
    void Write(RGTextureHandle handle, RGResourceUsage usage)
    {
        _pass._resources.push_back({ handle, usage, RGResourceAccess::WRITE });
    }

    void ReadWrite(RGTextureHandle handle, RGResourceUsage usage)
    {
        _pass._resources.push_back({ handle, usage, RGResourceAccess::READWRITE}); 
    }

private:
    RenderGraphPass& _pass;
};