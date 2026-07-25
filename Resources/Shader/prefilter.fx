#ifndef _PREFILTER_FX_
#define _PREFILTER_FX_

#include "ibl_params.fx"

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

float RadicalInverseVanDerCorput(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);

    return float(bits) * 2.3283064365386963e-10f;
}

float2 Hammersley(uint index, uint sampleCount)
{
    return float2(float(index) / float(sampleCount), RadicalInverseVanDerCorput(index));
}

float3 ImportanceSampleGGX(float2 xi, float3 normal, float roughness)
{
    const float PI = 3.14159265359f;

    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    float phi = 2.f * PI * xi.x;

    float cosTheta = sqrt((1.f - xi.y) / (1.f + (alpha2 - 1.f) * xi.y));
    float sinTheta = sqrt(max(1.f - cosTheta * cosTheta, 0.f));

    float3 halfVectorTangent = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
    float3 up = abs(normal.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    float3 tangent = normalize(cross(up, normal));
    float3 bitangent = cross(normal, tangent);
    float3 halfVector = tangent * halfVectorTangent.x + bitangent * halfVectorTangent.y + normal * halfVectorTangent.z;

    return normalize(halfVector);
}

float4 PS_Main(VS_OUT input) : SV_Target
{
    float3 N = GetCubeDirection(g_cubeFaceIndex, input.screenUV);

    // Environment prefilter에서는 V와 R을 N과 같다고 둔다.
    float3 R = N;
    float3 V = R;

    uint sampleCount = (g_sampleCount > 0u) ? g_sampleCount : 1024u;
    
    float3 prefilteredColor = float3(0.f, 0.f, 0.f);
    float totalWeight = 0.f;

    for (uint i = 0u; i < sampleCount; ++i)
    {
        float2 xi = Hammersley(i, sampleCount);
        float3 H = ImportanceSampleGGX(xi, N, max(g_cubeRoughness, 0.001f));
        float3 L = normalize(2.f * dot(V, H) * H - V);

        float NdotL = saturate(dot(N, L));

        if (NdotL > 0.f)
        {
            float3 sampleColor = g_environmentCubeMap.SampleLevel(g_linearSampler, L, 0.f).rgb;
            prefilteredColor += sampleColor * NdotL;
            totalWeight += NdotL;
        }
    }

    prefilteredColor /= max(totalWeight, 0.0001f);

    return float4(prefilteredColor, 1.f);
}

#endif