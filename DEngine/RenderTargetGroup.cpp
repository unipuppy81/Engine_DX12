#include "pch.h"
#include "RenderTargetGroup.h"
#include "DEngine.h"

void RenderTargetGroup::Create(RENDER_TARGET_GROUP_TYPE groupType, vector<RenderTarget>& rtVec, shared_ptr<Texture> dsTexture)
{
	assert(!rtVec.empty());
	assert(rtVec.size() <= MAX_RENDER_TARGET_COUNT);

	_groupType = groupType;
	_rtVec = rtVec;
	_rtCount = static_cast<uint32>(_rtVec.size());
	_dsTexture = dsTexture;

	_rtvHandles.clear();
	_rtvHandles.reserve(_rtCount);

	for (uint32 i = 0; i < _rtCount; ++i)
	{
		assert(_rtVec[i].target != nullptr);

		_rtvHandles.push_back(_rtVec[i].target->GetRTVHandle());

	}

	if (_dsTexture != nullptr)
		_dsvHandle = _dsTexture->GetDSVHandle();
	else
		_dsvHandle = {};
}

void RenderTargetGroup::OMSetRenderTargets(uint32 count, uint32 offset)
{
	assert(offset < _rtCount);
	assert(count > 0);
	assert(offset + count <= _rtCount);

	D3D12_VIEWPORT viewport =
	{
		0.f,
		0.f,
		_rtVec[0].target->GetWidth(),
		_rtVec[0].target->GetHeight(),
		0.f,
		1.f
	};

	D3D12_RECT scissorRect =
	{
		0,
		0,
		static_cast<LONG>(_rtVec[0].target->GetWidth()),
		static_cast<LONG>(_rtVec[0].target->GetHeight())
	};

	GRAPHICS_CMD_LIST->RSSetViewports(1, &viewport);
	GRAPHICS_CMD_LIST->RSSetScissorRects(1, &scissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle = _dsTexture != nullptr ? &_dsvHandle : nullptr;
	GRAPHICS_CMD_LIST->OMSetRenderTargets(count, &_rtvHandles[offset], FALSE, dsvHandle);
}

void RenderTargetGroup::OMSetRenderTargets()
{
	assert(!_rtvHandles.empty());

	D3D12_VIEWPORT viewport =
	{
		0.f,
		0.f,
		_rtVec[0].target->GetWidth(),
		_rtVec[0].target->GetHeight(),
		0.f,
		1.f
	};

	D3D12_RECT scissorRect =
	{
		0,
		0,
		static_cast<LONG>(_rtVec[0].target->GetWidth()),
		static_cast<LONG>(_rtVec[0].target->GetHeight())
	};

	GRAPHICS_CMD_LIST->RSSetViewports(1, &viewport);
	GRAPHICS_CMD_LIST->RSSetScissorRects(1, &scissorRect);

	D3D12_CPU_DESCRIPTOR_HANDLE* dsvHandle = _dsTexture != nullptr ? &_dsvHandle : nullptr;
	GRAPHICS_CMD_LIST->OMSetRenderTargets(_rtCount, _rtvHandles.data(), FALSE, dsvHandle);
}

void RenderTargetGroup::ClearRenderTargetView(uint32 index)
{
	assert(index < _rtCount);

	GRAPHICS_CMD_LIST->ClearRenderTargetView(_rtvHandles[index], _rtVec[index].clearColor, 0, nullptr);

	/*
	if (_dsTexture != nullptr)
	{
		GRAPHICS_CMD_LIST->ClearDepthStencilView(_dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
	}
	*/
}

void RenderTargetGroup::ClearRenderTargetView()
{
	//WaitResourceToTarget();

	for (uint32 i = 0; i < _rtCount; ++i)
	{
		GRAPHICS_CMD_LIST->ClearRenderTargetView(_rtvHandles[i], _rtVec[i].clearColor, 0, nullptr);
	}

	if (_dsTexture != nullptr)
	{
		GRAPHICS_CMD_LIST->ClearDepthStencilView(_dsvHandle,  D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
	}
}