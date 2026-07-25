#ifndef _BRDF_LUT_FX_
#define _BRDF_LUT_FX_

#include "ibl_params.fx"

struct VS_OUT
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD0;
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
    output.uv = pos * 0.5f + 0.5f; // UV: x축 = NdotV (0~1), y축 = Roughness (0~1)
    
    return output;
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

// GGX Importance Sampling
float3 ImportanceSampleGGX(float2 xi, float3 N, float roughness)
{
    const float PI = 3.14159265359f;

    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;

    float phi = 2.f * PI * xi.x;
    float cosTheta = sqrt((1.f - xi.y) / (1.f + (alpha2 - 1.f) * xi.y));
    float sinTheta = sqrt(max(1.f - cosTheta * cosTheta, 0.f));

    float3 H;
    H.x = cos(phi) * sinTheta;
    H.y = sin(phi) * sinTheta;
    H.z = cosTheta;

    // N이 (0,0,1)인 기저 좌표계 기준
    float3 up = abs(N.z) < 0.999f ? float3(0.f, 0.f, 1.f) : float3(1.f, 0.f, 0.f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);

    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

// Geometric Shadowing (Smith GGX-Schlick)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    // IBL BRDF LUT 전용 k값 (k = alpha / 2)
    float a = roughness;
    float k = (a * a) / 2.f;

    float nom = NdotV;
    float denom = NdotV * (1.f - k) + k;

    return nom / max(denom, 0.0001f);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.f);
    float NdotL = max(dot(N, L), 0.f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// BRDF 2D 적분 (Scale & Bias 계산)
float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V;
    V.x = sqrt(1.f - NdotV * NdotV); // sin
    V.y = 0.f;
    V.z = NdotV; // cos

    float A = 0.f;
    float B = 0.f;

    float3 N = float3(0.f, 0.f, 1.f);

    const uint sampleCount = 1024u;

    for (uint i = 0u; i < sampleCount; ++i)
    {
        float2 xi = Hammersley(i, sampleCount);
        float3 H = ImportanceSampleGGX(xi, N, roughness);
        float3 L = normalize(2.f * dot(V, H) * H - V);

        float NdotL = max(L.z, 0.f);
        float NdotH = max(H.z, 0.f);
        float VdotH = max(dot(V, H), 0.f);

        if (NdotL > 0.f)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV + 0.0001f);
            float Fc = pow(1.f - VdotH, 5.f);

            A += (1.f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }

    A /= float(sampleCount);
    B /= float(sampleCount);

    return float2(A, B);
}

float4 PS_Main(VS_OUT input) : SV_Target
{
    // input.uv.x = NdotV (0~1)
    // input.uv.y = Roughness (0~1)
    float2 integratedBRDF = IntegrateBRDF(input.uv.x, input.uv.y);
    return float4(integratedBRDF, 0.f, 1.f);
}

#endif