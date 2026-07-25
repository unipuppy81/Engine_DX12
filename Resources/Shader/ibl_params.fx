#ifndef _IBL_PARAMS_FX_
#define _IBL_PARAMS_FX_

cbuffer IBL_CUBEMAP_PARAMS : register(b4)
{
    uint g_cubeFaceIndex;
    float g_cubeRoughness;
    uint g_sampleCount;
    float g_padding;
};

SamplerState g_linearSampler : register(s0);
#endif