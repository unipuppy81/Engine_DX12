#ifndef _IRRADIANCE_FX_
#define _IRRADIANCE_FX_

#include "params.fx"

TextureCube<float4> g_environmentCubeMap : register(t0);

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
    output.screenUV = pos * 0.5f + 0.5f;

    return output;
}

float3 GetCubeDirection(int faceIndex, float2 uv)
{
    float2 p = uv * 2.f - 1.f;

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

float4 PS_Main(VS_OUT input) : SV_Target
{
    const float PI = 3.14159265359f;
    const float sampleDelta = 0.025f;

    // 현재 Irradiance CubeMap 픽셀의 법선 방향
    float3 N = GetCubeDirection(g_cubeFaceIndex, input.screenUV);

    // N 기준 탄젠트 좌표계 생성
    float3 up = abs(N.y) < 0.999f ? float3(0.f, 1.f, 0.f) : float3(1.f, 0.f, 0.f);

    float3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    float3 irradiance = float3(0.f, 0.f, 0.f);
    uint sampleCount = 0;

    // N 방향 반구 적분
    for (float phi = 0.f; phi < 2.f * PI; phi += sampleDelta)
    {
        for (float theta = 0.f;
             theta < 0.5f * PI;
             theta += sampleDelta)
        {
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);

            // 탄젠트 공간 반구 방향
            float3 tangentSample = float3(
                sinTheta * cos(phi),
                sinTheta * sin(phi),
                cosTheta
            );

            // 탄젠트 공간 -> CubeMap 방향
            float3 sampleDir =
                tangentSample.x * right +
                tangentSample.y * up +
                tangentSample.z * N;

            float3 environmentColor =
                g_environmentCubeMap.SampleLevel(
                    g_sam_0,
                    normalize(sampleDir),
                    0.f
                ).rgb;

            // cos: Lambert 가중치
            // sin: 구면 좌표 면적 보정
            irradiance += environmentColor * cosTheta * sinTheta;

            sampleCount++;
        }
    }

    irradiance = PI * irradiance / max((float) sampleCount, 1.f);

    return float4(irradiance, 1.f);
}

#endif