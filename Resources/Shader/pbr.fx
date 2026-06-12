#ifndef _PBR_FX_
#define _PBR_FX_

#include "params.fx"
#include "utils.fx"

// ============================================================
// PBR GBuffer Pass
// ============================================================

struct VS_PBR_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
};

struct VS_PBR_OUT
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float3 viewPos : POSITION;
    float3 viewNormal : NORMAL;
    float3 viewTangent : TANGENT;
};

struct PS_PBR_OUT
{
    float4 position : SV_Target0;
    float4 normal : SV_Target1;
    float4 color : SV_Target2;
    float4 materialInfo : SV_Target3;
};

VS_PBR_OUT VS_PBR(VS_PBR_IN input)
{
    VS_PBR_OUT output = (VS_PBR_OUT) 0;

    output.pos = mul(float4(input.pos, 1.f), g_matWVP);
    output.uv = input.uv;

    output.viewPos = mul(float4(input.pos, 1.f), g_matWV).xyz;
    output.viewNormal = normalize(mul(float4(input.normal, 0.f), g_matWV).xyz);
    output.viewTangent = normalize(mul(float4(input.tangent, 0.f), g_matWV).xyz);

    return output;
}

PS_PBR_OUT PS_PBR(VS_PBR_OUT input)
{
    PS_PBR_OUT output = (PS_PBR_OUT) 0;

    float4 albedo = g_tex_0.Sample(g_sam_0, input.uv) * g_baseColor;

    output.position = float4(input.viewPos, 1.f);
    output.normal = float4(normalize(input.viewNormal), 1.f);
    output.color = albedo;

    // x = metallic
    // y = roughness
    // z = ao
    // w = shadingModel, 1 = PBR
    output.materialInfo = float4(g_metallic, g_roughness, g_ao, 1.f);
    
    return output;
}

// ============================================================
// Lighting / Final Pass
// ============================================================

struct VS_SCREEN_IN
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VS_SCREEN_OUT
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

struct PS_LIGHT_OUT
{
    float4 diffuse : SV_Target0;
    float4 specular : SV_Target1;
};

VS_SCREEN_OUT VS_DirLight(VS_SCREEN_IN input)
{
    VS_SCREEN_OUT output = (VS_SCREEN_OUT) 0;

    output.pos = float4(input.pos * 2.f, 1.f);
    output.uv = input.uv;

    return output;
}

PS_LIGHT_OUT PS_DirLight(VS_SCREEN_OUT input)
{
    PS_LIGHT_OUT output = (PS_LIGHT_OUT) 0;

    float3 viewPos = g_tex_0.Sample(g_sam_0, input.uv).xyz;
    if (viewPos.z <= 0.f)
        clip(-1);

    // MaterialInfoTarget
    float4 materialInfo = g_tex_3.Sample(g_sam_0, input.uv);
    float shadingModel = materialInfo.w;

    // PBR 오브젝트만 처리
    if (shadingModel < 0.5f)
        clip(-1);

    float metallic = materialInfo.x;
    float roughness = materialInfo.y;
    float ao = materialInfo.z;

    float3 viewNormal = normalize(g_tex_1.Sample(g_sam_0, input.uv).xyz);

    
    // 나중에 교체할 자리
    // float3 pbrLighting = CalculatePBRLight(...);
    // output.diffuse = float4(pbrLighting, 1.f);
    // output.specular = float4(0.f, 0.f, 0.f, 1.f);
    
    // 임시 테스트:
    // PBR Light Pass가 PBR 오브젝트 픽셀에만 적용되는지 확인
    output.diffuse = float4(1.f, 0.f, 0.f, 1.f);
    output.specular = float4(0.f, 0.f, 0.f, 1.f);

    return output;
}

VS_SCREEN_OUT VS_PointLight(VS_SCREEN_IN input)
{
    VS_SCREEN_OUT output = (VS_SCREEN_OUT) 0;

    output.pos = mul(float4(input.pos, 1.f), g_matWVP);
    output.uv = input.uv;

    return output;
}

PS_LIGHT_OUT PS_PointLight(VS_SCREEN_OUT input)
{
    PS_LIGHT_OUT output = (PS_LIGHT_OUT) 0;

    output.diffuse = float4(1.f, 0.f, 0.f, 1.f);
    output.specular = float4(1.f, 0.f, 0.f, 1.f);

    return output;
    //PS_LIGHT_OUT output = (PS_LIGHT_OUT) 0;

    //float2 uv = float2(input.pos.x / g_vec2_0.x, input.pos.y / g_vec2_0.y);

    //float3 viewPos = g_tex_0.Sample(g_sam_0, uv).xyz;
    //if (viewPos.z <= 0.f)
    //    clip(-1);

    //// MaterialInfoTarget
    //float4 materialInfo = g_tex_3.Sample(g_sam_0, uv);
    //float shadingModel = materialInfo.w;

    //// PBR 오브젝트만 처리
    //if (shadingModel < 0.5f)
    //    clip(-1);

    //float metallic = materialInfo.x;
    //float roughness = materialInfo.y;
    //float ao = materialInfo.z;

    //int lightIndex = g_int_0;

    //float3 viewLightPos = mul(float4(g_light[lightIndex].position.xyz, 1.f), g_matView).xyz;
    //float distance = length(viewPos - viewLightPos);

    //if (distance > g_light[lightIndex].range)
    //    clip(-1);

    //float3 viewNormal = normalize(g_tex_1.Sample(g_sam_0, uv).xyz);

    //// 임시 테스트:
    //// PBR Point Light가 PBR 오브젝트에만 들어가는지 확인
    //output.diffuse = float4(1.f, 0.f, 0.f, 1.f);
    //output.specular = float4(1.f, 0.f, 0.f, 1.f);

    //return output;
}

VS_SCREEN_OUT VS_Final(VS_SCREEN_IN input)
{
    VS_SCREEN_OUT output = (VS_SCREEN_OUT) 0;

    output.pos = float4(input.pos * 2.f, 1.f);
    output.uv = input.uv;

    return output;
}

float4 PS_Final(VS_SCREEN_OUT input) : SV_Target
{
    return float4(1.f, 0.f, 0.f, 1.f);
}

#endif