#ifndef _EQUIRECT_TO_CUBE_FX_
#define _EQUIRECT_TO_CUBE_FX_

#include "ibl_params.fx"

// Input : Equirect HDR 2D Texture
Texture2D g_equirectMap : register(t0);

struct VS_OUT
{
    float4 pos : SV_Position;
    float2 screenUV : TEXCOORD0;
};

VS_OUT VS_Main(uint vertexID : SV_VertexID)
{
    VS_OUT output = (VS_OUT) 0;

    float2 positions[3] =
    {
        float2(-1.f, -1.f),
        float2(-1.f, 3.f),
        float2(3.f, -1.f)
    };

    float2 pos = positions[vertexID];
    output.pos = float4(pos, 0.f, 1.f);
    output.screenUV = pos * 0.5f + 0.5f;        // Clip Space 좌표를 UV 형태로 변환

    return output;
}

float3 GetCubeDirection(int faceIndex, float2 uv)
{
    // uv: 0~1
    float2 p = uv * 2.f - 1.f;

    // 화면 좌표계 보정
    float u = p.x;
    float v = -p.y;

    if (faceIndex == 0) // +X
        return normalize(float3(1.f, v, -u));

    if (faceIndex == 1) // -X
        return normalize(float3(-1.f, v, u));

    if (faceIndex == 2) // +Y
        return normalize(float3(u, 1.f, -v));

    if (faceIndex == 3) // -Y
        return normalize(float3(u, -1.f, v));

    if (faceIndex == 4) // +Z
        return normalize(float3(u, v, 1.f));

    // -Z
    return normalize(float3(-u, v, -1.f));
}

float2 SampleSphericalMap(float3 v)
{
    float2 uv = float2(atan2(v.z, v.x), asin(v.y));
    uv *= float2(0.15915494309f, 0.31830988618f);
    uv += 0.5f;
    return uv;
}

float4 PS_Main(VS_OUT input) : SV_Target
{
    float3 dir = GetCubeDirection(g_cubeFaceIndex, input.screenUV);
    float2 uv = SampleSphericalMap(dir);
    float3 color = g_equirectMap.Sample(g_linearSampler, uv).rgb;

    return float4(color, 1.f);
}

#endif