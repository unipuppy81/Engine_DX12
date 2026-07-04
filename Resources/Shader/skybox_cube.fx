#ifndef _SKYBOX_CUBE_FX_
#define _SKYBOX_CUBE_FX_

#include "params.fx"

TextureCube g_cube_0 : register(t0);

struct VS_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct VS_OUT
{
    float4 pos : SV_Position;
    float3 localPos : TEXCOORD0;
};

VS_OUT VS_Main(VS_IN input)
{
    VS_OUT output = (VS_OUT) 0;

    // Cubemap 샘플링 방향은 큐브의 로컬 방향 사용
    output.localPos = input.pos;

    // View Matrix에서 카메라 위치 이동 성분 제거
    matrix viewNoTranslation = g_matView;

    // 이 엔진은 mul(vector, matrix) 방식이므로 translation은 보통 _41, _42, _43
    viewNoTranslation._41 = 0.f;
    viewNoTranslation._42 = 0.f;
    viewNoTranslation._43 = 0.f;

    // 중요:
    // g_matWorld는 유지해야 Skybox 오브젝트의 scale이 적용됨
    float4 worldPos = mul(float4(input.pos, 1.f), g_matWorld);
    float4 viewPos = mul(worldPos, viewNoTranslation);
    float4 projPos = mul(viewPos, g_matProjection);

    // Skybox는 항상 depth 최후방
    output.pos = projPos.xyww;

    return output;
}

float4 PS_Main(VS_OUT input) : SV_Target
{
    float3 dir = normalize(input.localPos);
    float3 color = g_cube_0.Sample(g_sam_0, dir).rgb;

    return float4(color, 1.f);
}

#endif