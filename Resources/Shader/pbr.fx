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
    //output.materialInfo = float4(g_metallic, g_roughness, g_ao, 1.f);
    output.materialInfo = float4(1.f, 0.f, 1.f, 1.f);
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

    // PBR_DirLight Material 기준:
    // g_tex_0 = PositionTarget
    // g_tex_1 = NormalTarget
    // g_tex_2 = DiffuseTarget / AlbedoTarget
    // g_tex_3 = MaterialInfoTarget

    float3 viewPos = g_tex_0.Sample(g_sam_0, input.uv).xyz;

    if (viewPos.z <= 0.f)
        clip(-1);

    float3 viewNormal = normalize(g_tex_1.Sample(g_sam_0, input.uv).xyz);
    float3 albedo = g_tex_2.Sample(g_sam_0, input.uv).rgb;
    float4 materialInfo = g_tex_3.Sample(g_sam_0, input.uv);

    float shadingModel = materialInfo.w;

    // PBR 오브젝트만 처리
    if (shadingModel < 0.5f)
        clip(-1);
    
    float metallic = saturate(materialInfo.x);
    float roughness = max(saturate(materialInfo.y), 0.04f);
    float ao = saturate(materialInfo.z);
    float lightIntensity = 1.0f;
    
    
    // 비금속
    //float metallic = 0.0f;
    //float roughness = 0.4f;
    //float ao = 1.f;
    //float lightIntensity = 5.0f;
    
    // 금속
    // float metallic = 0.9f;
    // float roughness = 0.4f;
    // float ao = 1.f;
    // float lightIntensity = 5.0f;
    
    float3 viewLightDir = normalize(mul(float4(g_light[g_int_0].direction.xyz, 0.f), g_matView).xyz);

    float3 N = -normalize(viewNormal);
    float3 V = normalize(viewPos);
    float3 L = normalize(viewLightDir);

    float3 lightColor = g_light[g_int_0].color.diffuse.rgb;
    float3 radiance = lightColor * lightIntensity; // float3(5.f, 5.f, 5.f);
    
    float3 diffusePBR = float3(0.f, 0.f, 0.f);
    float3 specularPBR = float3(0.f, 0.f, 0.f);

    CalculatePBRDirectional(
        albedo,
        N,
        V,
        L,
        radiance,
        metallic,
        roughness,
        diffusePBR,
        specularPBR
    );

    diffusePBR *= ao;
    
// ============================================================
//  Ambient IBL
// ============================================================
    {
        // F0 계산
        float3 F0 = float3(0.04f, 0.04f, 0.04f);
        F0 = lerp(F0, albedo, metallic);

        // View 방향 기준 Fresnel
        float NdotV = max(dot(N, V), 0.f);
        float3 F = FresnelSchlick(NdotV, F0);

        // diffuse/specular 비율
        float3 kS = F;
        float3 kD = 1.f - kS;
        kD *= 1.f - metallic;

        output.diffuse = float4(diffusePBR, 1.f);
        output.specular = float4(specularPBR, 1.f);
        return output;
    }
    
    
    output.diffuse = float4(diffusePBR, 1.f);
    output.specular = float4(specularPBR, 1.f);

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

    output.diffuse = float4(0.f, 0.f, 0.f, 1.f);
    output.specular = float4(0.f, 0.f, 0.f, 1.f);

    return output;
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
    float3 albedo = g_tex_0.Sample(g_sam_0, input.uv).rgb;
    float3 directDiffuse = g_tex_1.Sample(g_sam_0, input.uv).rgb;
    float3 directSpecular = g_tex_2.Sample(g_sam_0, input.uv).rgb;
    float4 materialInfo = g_tex_3.Sample(g_sam_0, input.uv);

    float shadingModel = materialInfo.w;

    if (shadingModel < 0.5f)
        return float4(albedo * directDiffuse + directSpecular, 1.f);

    float metallic = saturate(materialInfo.x);
    float roughness = max(saturate(materialInfo.y), 0.04f);
    float ao = saturate(materialInfo.z);

    float3 viewPos = g_positionTarget.Sample(g_sam_0, input.uv).xyz;
    float3 viewNormal = normalize(g_normalTarget.Sample(g_sam_0, input.uv).xyz);

    

    //// View 공간에서 카메라는 원점
    float3 V_view = normalize(-viewPos);
    float3 N_view = normalize(viewNormal);
    
    //// Cubemap은 World 방향으로 샘플링
    float3 N_world = normalize(mul(float4(N_view, 0.f), g_matViewInv).xyz);
    float3 V_world = normalize(mul(float4(V_view, 0.f), g_matViewInv).xyz);
    
    float NdotV = saturate(dot(N_world, V_world));
    float3 R_world = reflect(-V_world, N_world);
    
    float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

    float3 F = FresnelSchlick(NdotV, F0);

    float3 kS = F;
    float3 kD = (1.f - kS) * (1.f - metallic);

    // Diffuse IBL
    float3 irradiance = g_irradianceMap.Sample(g_sam_0, N_world).rgb;
    float3 diffuseIBL = irradiance * albedo;

    // Specular IBL
    const float maxMipLevel = 4.f;

    float3 prefilteredColor = g_prefilteredMap.SampleLevel(g_sam_0, R_world, roughness * maxMipLevel).rgb;

    float2 brdf = g_brdfLUT.Sample(g_sam_0, float2(NdotV, roughness)).rg;

    float3 specularIBL = prefilteredColor * (F * brdf.x + brdf.y);
    float3 ambient = (kD * diffuseIBL + specularIBL) * ao;
    float3 result = directDiffuse + directSpecular + ambient;

    result = result / (result + 1.f);
    result = pow(result, 1.f / 2.2f);
    return float4(prefilteredColor, 1.f);
    return float4(result, 1.f);
}

#endif