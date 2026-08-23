#pragma once

class Texture;


// ============================================================
// Render Graph에서 Texture를 가리키는 ID
// ============================================================
struct RGTextureHandle
{
	uint32 id = UINT32_MAX;

	bool IsValid() const { return id != UINT32_MAX; }
};


// ============================================================
// 해당 Pass 에서 리소스 사용 용도
// ============================================================
enum class RGResourceUsage
{
	PixelSRV,
	NonPixelSRV,

	RenderTarget,
	DepthWrite,
	DepthRead,

	UAV,

	CopySource,
	CopyDest,
		
	Present
};

enum class RGResourceAccess
{
	READ,
	WRITE,
	READWRITE
};

// ============================================================
// RenderGraph 내부에서 관리되는 실제 Texture 정보
// ============================================================
struct RGTextureResource
{
	shared_ptr<Texture> texture;

	D3D12_RESOURCE_STATES currentState = D3D12_RESOURCE_STATE_COMMON;
	bool external = true;
};

class RenderGraphResource
{

};

